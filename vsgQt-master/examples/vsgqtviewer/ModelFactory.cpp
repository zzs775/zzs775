#include "ModelFactory.h"
#include <QFile>
#include <vsg/utils/ComputeBounds.h>

ModelFactory* ModelFactory::instance()
{
    static ModelFactory s_instance;
    return &s_instance;
}

void FixMaterialVisitor::apply(vsg::Object& object)
{
    object.traverse(*this);
}

void FixMaterialVisitor::apply(vsg::StateGroup& sg)
{
    for (auto& command : sg.stateCommands)
    {
        if (auto depth = command.cast<vsg::DepthStencilState>())
        {
            depth->depthTestEnable = VK_TRUE;
            depth->depthWriteEnable = VK_TRUE;
        }
        if (auto blend = command.cast<vsg::ColorBlendState>())
        {
            if (!blend->attachments.empty())
            {
                auto& attachment = blend->attachments[0];
                attachment.blendEnable = VK_TRUE;
                attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attachment.colorBlendOp = VK_BLEND_OP_ADD;
                attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attachment.alphaBlendOp = VK_BLEND_OP_ADD;
            }
        }
    }
    sg.traverse(*this);
}

void ModelFactory::init(vsg::ref_ptr<vsg::Options> options)
{
    _options = options;

    // ★ 诊断：打印 VSG Options 中的所有搜索路径
    qDebug() << "  [ModelFactory] VSG Options 搜索路径:";
    for (const auto& p : _options->paths)
    {
        qDebug() << "    -> " << QString::fromStdString(p.string());
    }

    // 字体候选列表（优先序排列）
    QStringList fontPaths = {
        "C:/Users/cfh12/Desktop/rocky_qt/sim.vsg-master/sim.vsg/data/fonts/times.vsgb",
        "C:/Users/cfh12/Desktop/rocky_qt/rocky-main (1)/install/share/rocky/data/times.vsgb",
        "C:/Windows/Fonts/simhei.ttf", 
        "C:/Windows/Fonts/msyh.ttc",   
        "C:/Windows/Fonts/arial.ttf",  
        "fonts/SimHei.ttf",
        "fonts/times.vsgb"
    };

    for (const auto& fontPath : fontPaths)
    {
        // ★ 诊断：先用 QFile 检查该路径在磁盘上是否真正存在
        bool fileExists = QFile::exists(fontPath);
        qDebug() << "  [Font] 尝试:" << fontPath
                 << " | 磁盘存在:" << (fileExists ? "YES" : "NO");

        _font = vsg::read_cast<vsg::Font>(fontPath.toStdString(), _options);
        if (_font)
        {
            qDebug() << "  ★ [ModelFactory] 字体成功加载 ->" << fontPath;
            break;
        }
        else
        {
            qDebug() << "  [Font] vsg::read_cast 返回 nullptr ->" << fontPath;
        }
    }

    if (!_font)
    {
        qWarning() << "   ModelFactory: 所有字体路径均失败！标签功能将不可用。";
        qWarning() << "   请确认 .vsgb 字体文件确实存在于上述路径之一";
    }

    _initialized = true;
    qDebug() << "ModelFactory: 初始化完成, 字体状态:" << (_font ? "已加载" : "未加载");
}

vsg::ref_ptr<vsg::Node> ModelFactory::getRawModel(const std::string& filename)
{
    auto it = _modelCache.find(filename);
    if (it != _modelCache.end())
    {
        return it->second;
    }

    // 延迟加载 vsgXchange：第一次加载模型时才初始化
    if (!_vsgXchangeLoaded)
    {
        qDebug() << "ModelFactory: 首次加载模型，初始化 vsgXchange...";
        _options->add(vsgXchange::all::create());
        _vsgXchangeLoaded = true;
    }

    qDebug() << "ModelFactory: 加载模型 ->" << QString::fromStdString(filename);

    auto model = vsg::read_cast<vsg::Node>(filename, _options);
    if (!model)
    {
        qWarning() << "ModelFactory: 模型加载失败 ->" << QString::fromStdString(filename);
        return nullptr;
    }

    FixMaterialVisitor fixMaterial;
    model->accept(fixMaterial);

    forceBrightMaterial(model);

    _modelCache[filename] = model;
    qDebug() << "ModelFactory: 模型已缓存 ->" << QString::fromStdString(filename);

    return model;
}

vsg::ref_ptr<vsg::Node> ModelFactory::getColoredModel(const std::string& filename, const std::string& teamColor)
{
    if (teamColor.empty())
        return getRawModel(filename);

    std::string key = filename + "|" + teamColor;
    auto it = _coloredModelCache.find(key);
    if (it != _coloredModelCache.end())
        return it->second;

    // 从磁盘重新读取一份完全独立的模型副本 ——
    // VSG 的 clone/CopyOp 无论如何都会共享 DescriptorBuffer/BufferInfo，
    // 导致材质修改互相污染。只有重新读取才能获得物理隔离的材质数据。
    if (!_vsgXchangeLoaded)
    {
        _options->add(vsgXchange::all::create());
        _vsgXchangeLoaded = true;
    }

    auto model = vsg::read_cast<vsg::Node>(filename, _options);
    if (!model)
    {
        qWarning() << "[Color] 重新读取模型失败，回退到原始缓存:" << QString::fromStdString(filename);
        return getRawModel(filename);
    }

    // 对独立副本执行与 getRawModel 相同的材质修正流程
    FixMaterialVisitor fixMaterial;
    model->accept(fixMaterial);
    forceBrightMaterial(model);

    // 在完全独立的模型上应用阵营颜色
    applyTeamColor(model.get(), teamColor);

    _coloredModelCache[key] = model;
    qDebug() << "[Color] 已缓存染色模型:" << QString::fromStdString(key);
    return model;
}

vsg::ref_ptr<vsg::Node> ModelFactory::createLabel(const QString& text, double height)
{
    if (text.isEmpty())
        return nullptr;

    // 字体回退机制
    vsg::ref_ptr<vsg::Font> font = _font;
    if (!font)
    {
        font = vsg::read_cast<vsg::Font>("fonts/times.vsgb", _options);
        if (!font)
        {
            qWarning() << "  [Label] 字体加载失败，无法创建标签：" << text;
            return nullptr;
        }
        qDebug() << "  [Label] 使用回退字体 times.vsgb";
    }

    try
    {
        auto layout = vsg::StandardLayout::create();
        layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
        layout->verticalAlignment   = vsg::StandardLayout::BASELINE_ALIGNMENT;
        layout->color               = vsg::vec4(0.0f, 1.0f, 1.0f, 1.0f);  // 青色
        layout->outlineColor        = vsg::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        layout->outlineWidth        = 0.1f;

        // ★ Billboard：字体始终朝向相机
        layout->billboard = true;

        auto textNode = vsg::Text::create();
        textNode->font   = font;
        textNode->text   = vsg::stringValue::create(text.toStdString());
        textNode->layout = layout;

        // ★【关键修复2】必须调用 setup() 才能生成 GPU 顶点与图集数据
        textNode->setup(0, _options);

        // ★【原因排查 2】: 解决文字在 Rocky 日志深度下不显示的问题
        // 遍历生成后的文字管线，强行关掉它的深度测试和写入，让它作为 UI 绝对悬浮在最上层
        struct DisableDepthVisitor : public vsg::Visitor
        {
            void apply(vsg::Object& object) override { object.traverse(*this); }
            void apply(vsg::BindGraphicsPipeline& bindPipeline) override
            {
                if (bindPipeline.pipeline)
                {
                    for (auto& state : bindPipeline.pipeline->pipelineStates)
                    {
                        if (auto ds = state.cast<vsg::DepthStencilState>())
                        {
                            ds->depthTestEnable = VK_FALSE;
                            ds->depthWriteEnable = VK_FALSE;
                        }
                    }
                }
                bindPipeline.traverse(*this);
            }
        };
        DisableDepthVisitor ddv;
        textNode->accept(ddv);

        // ★【关键修复3】调大基础字号并加大间距，防止嵌在模型体内
        double labelScale = height * 1.5;

        auto transform = vsg::MatrixTransform::create();
        transform->matrix = vsg::scale(labelScale, labelScale, labelScale);
        transform->addChild(textNode);

        qDebug() << "  [Label] 已创建标签节点:" << text << "scale=" << labelScale;
        return transform;
    }
    catch (const std::exception& e)
    {
        qWarning() << "  [Label] 创建标签时发生异常:" << e.what();
        return nullptr;
    }
    catch (...)
    {
        qWarning() << "  [Label] 创建标签时发生未知异常";
        return nullptr;
    }
}


vsg::dbox ModelFactory::computeBoundingBox(vsg::ref_ptr<vsg::Node> node)
{
    vsg::ComputeBounds computeBounds;
    node->accept(computeBounds);
    return computeBounds.bounds;
}

double ModelFactory::computeScaleFactor(const vsg::dbox& box, double targetSize)
{
    vsg::dvec3 min = box.min;
    vsg::dvec3 max = box.max;

    double width = max.x - min.x;
    double height = max.y - min.y;
    double depth = max.z - min.z;

    double maxSize = std::max({width, height, depth});

    if (maxSize < 1e-6)
    {
        return 1.0;
    }

    return targetSize / maxSize;
}

vsg::ref_ptr<vsg::Node> ModelFactory::createVisualEntity(
    const std::string& filename,
    const QString& label,
    double targetSize,
    const std::string& teamColor)
{
    auto group = vsg::Group::create();

    auto model = getColoredModel(filename, teamColor);
    if (!model)
    {
        qWarning() << "ModelFactory: 无法创建实体，模型加载失败";
        return nullptr;
    }

    vsg::dbox bounds = computeBoundingBox(model);

    double scaleFactor = computeScaleFactor(bounds, targetSize);

    vsg::dvec3 center(
        (bounds.min.x + bounds.max.x) * 0.5,
        (bounds.min.y + bounds.max.y) * 0.5,
        (bounds.min.z + bounds.max.z) * 0.5);

    auto transform = vsg::MatrixTransform::create();

    // 模型朝向修正 - glTF/GLB 使用 Y-up 坐标系，-Z 为前方
    // Rx(+90°) 正确地将各轴映射到 ENU 局部坐标系：
    //   Model Y (up)      → 局部 +Z → 世界 Up    (机背朝天) ✓
    //   Model -Z (nose)   → 局部 +Y → 世界 North  (机鼻朝北) ✓
    //   Model X (r-wing)  → 局部 +X → 世界 East   (右翼朝东) ✓
    // 注意：Rx(-90°) 会将 Y → -Z（朝地），导致模型俯冲入地
    vsg::dmat4 orientationFix = vsg::rotate(vsg::radians(0.0), 1.0, 0.0, 0.0);

    vsg::dmat4 scaleMat = vsg::scale(scaleFactor, scaleFactor, scaleFactor);
    vsg::dmat4 centerMat = vsg::translate(-center);

    // 变换顺序：先居中 -> 再修正朝向 -> 最后缩放
    transform->matrix = scaleMat * orientationFix * centerMat;
    transform->addChild(model);
    group->addChild(transform);

    if (!label.isEmpty())
    {
        auto labelNode = createLabel(label, targetSize);
        if (labelNode)
        {
            auto labelTransform = vsg::MatrixTransform::create();
            // 将标签大幅上移，防止被庞大的飞机模型或包围盒遮挡
            labelTransform->matrix = vsg::translate(0.0, 0.0, targetSize * 3.0);
            labelTransform->addChild(labelNode);
            group->addChild(labelTransform);
            qDebug() << "  [createVisualEntity] 标签节点已挂入 group:" << label;
        }
        else
        {
            qWarning() << "  [createVisualEntity] 标签创建失败，无标签显示:" << label;
        }
    }

    return group;
}
