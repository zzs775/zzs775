#include "ModelFactory.h"
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

    QStringList fontPaths = {
        "fonts/SimHei.ttf",
        "fonts/simhei.ttf",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/arial.ttf"};

    for (const auto& fontPath : fontPaths)
    {
        _font = vsg::read_cast<vsg::Font>(fontPath.toStdString(), _options);
        if (_font)
        {
            qDebug() << "  ModelFactory: 字体加载成功 ->" << fontPath;
            break;
        }
    }

    if (!_font)
    {
        qWarning() << "   ModelFactory: 字体加载失败！标签功能将不可用。";
        qWarning() << "   请确保 fonts/SimHei.ttf 存在于 bin 目录下";
    }

    _initialized = true;
    qDebug() << "ModelFactory: 初始化完成";
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
    // 安全字体屏蔽机制：字体缺失时直接返回空节点，禁用3D文字标签功能
    // 未来优化方向：使用QML 2D屏幕空间标签系统替代VSG 3D文字
    if (!_font)
    {
        // 静默返回，不打印警告避免刷屏
        return nullptr;
    }

    if (text.isEmpty())
    {
        return nullptr;
    }

    try
    {
        auto layout = vsg::StandardLayout::create();
        layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
        layout->verticalAlignment = vsg::StandardLayout::BASELINE_ALIGNMENT;
        layout->color = vsg::vec4(0.0f, 1.0f, 1.0f, 1.0f);

        auto textNode = vsg::Text::create();
        textNode->font = _font;
        textNode->text = vsg::stringValue::create(text.toStdString());
        textNode->layout = layout;

        double labelScale = height * 0.3;

        vsg::dmat4 rotationMatrix = vsg::rotate(vsg::PI * 0.5, 1.0, 0.0, 0.0);
        vsg::dmat4 scaleMatrix = vsg::scale(labelScale, labelScale, labelScale);
        vsg::dmat4 translateMatrix = vsg::translate(0.0, 0.0, height * 0.5);

        auto transform = vsg::MatrixTransform::create();
        transform->matrix = translateMatrix * rotationMatrix * scaleMatrix;
        transform->addChild(textNode);

        return transform;
    }
    catch (...)
    {
        // 捕获任何异常，确保不会因字体问题导致崩溃
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

    double rawWidth = bounds.max.x - bounds.min.x;
    double rawHeight = bounds.max.y - bounds.min.y;
    double rawDepth = bounds.max.z - bounds.min.z;

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

    if (_font && !label.isEmpty())
    {
        auto labelNode = createLabel(label, targetSize);
        if (labelNode)
        {
            auto labelTransform = vsg::MatrixTransform::create();
            labelTransform->matrix = vsg::translate(0.0, 0.0, targetSize * 0.5);
            labelTransform->addChild(labelNode);
            group->addChild(labelTransform);
        }
    }

    return group;
}
