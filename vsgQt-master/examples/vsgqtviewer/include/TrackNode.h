#pragma once
#include <algorithm>
#include <rocky/vsg/PipelineState.h>
#include <rocky/vsg/VSGContext.h>
#include <vsg/all.h>

// 与 Rocky rocky.line.vert shader 对应的 line uniform 结构体 (LineStyleRecord)
struct LineStyle
{
    vsg::vec4 color = {1.f, 1.f, 0.f, 1.f}; // 黄色
    float width = 2.0f;
    int32_t stipplePattern = 0xFFFF;
    int32_t stippleFactor = 1;
    float depthOffset = 0.0f;
    uint32_t perVertexMask = 0x1; // 0x1 = 使用顶点颜色 (in_color)
    float devicePixelRatio = 1.0f;
    uint32_t padding[2] = {0, 0};
};

#include <QDebug>

class TrackNode : public vsg::Inherit<vsg::Group, TrackNode>
{
public:
    static vsg::ref_ptr<TrackNode> create(rocky::VSGContext context, size_t maxPoints = 3600)
    {
        return vsg::ref_ptr<TrackNode>(new TrackNode(context, maxPoints));
    }

    void clear()
    {
        _count = 0;
        _originSet = false;
        _dirty = true;
    }

    void addPoint(const vsg::dvec3& ecefPos, double currentTime)
    {
        if (!_vertices) return;

        if (!_originSet)
        {
            _originEcef = ecefPos;
            _originSet = true;
            _transform->matrix = vsg::translate(_originEcef);
        }
        vsg::vec3 rel = vsg::vec3(ecefPos - _originEcef);

        // 平移队列：新点插入末尾，超出容量时整体左移
        if (_count < _maxPoints)
        {
            (*_vertices)[_count] = rel;
            _times[_count] = currentTime;
            _count++;
        }
        else
        {
            std::copy(_vertices->data() + 1, _vertices->data() + _maxPoints, _vertices->data());
            std::copy(_times.data() + 1, _times.data() + _maxPoints, _times.data());
            (*_vertices)[_maxPoints - 1] = rel;
            _times[_maxPoints - 1] = currentTime;
        }

        // 剔除无效点：1. 超过5秒的老点  2. 倒放时出现的"未来"点
        size_t validStart = 0;
        size_t validCount = _count;

        for (size_t i = 0; i < _count; i++)
        {
            double diff = currentTime - _times[i];
            if (diff > 30.0 || diff < -0.1) // 允许微小的时钟抖动
            {
                // 如果是老点，挪动起点
                if (diff > 30.0) validStart = i + 1;
                // 如果是未来点，截断终点
                if (diff < -0.1)
                {
                    validCount = i;
                    break;
                }
            }
            else if (diff <= 30.0)
            {
                break; // 找到第一个在30秒内的老点后停止搜索起点
            }
        }

        if (validStart > 0 || validCount < _count)
        {
            if (validStart >= validCount)
            {
                _count = 0;
            }
            else
            {
                _count = validCount - validStart;
                std::copy(_vertices->data() + validStart, _vertices->data() + validStart + _count, _vertices->data());
                std::copy(_times.data() + validStart, _times.data() + validStart + _count, _times.data());
            }
        }

        _dirty = true;
    }

    void update()
    {
        if (!_vertices || !_dirty) return;
        _dirty = false;

        // 只更新有效点的 prev/next
        for (size_t i = 0; i < _count; ++i)
        {
            size_t prev = (i == 0) ? 0 : i - 1;
            size_t next = (i == _count - 1) ? _count - 1 : i + 1;
            (*_verticesPrev)[i] = (*_vertices)[prev];
            (*_verticesNext)[i] = (*_vertices)[next];
        }

        if (_draw) _draw->vertexCount = static_cast<uint32_t>(_count); // 只画有效点

        _vertices->dirty();
        _verticesPrev->dirty();
        _verticesNext->dirty();
    }

private:
    explicit TrackNode(rocky::VSGContext context, size_t maxPoints) : _maxPoints(maxPoints)
    {
        _times.resize(_maxPoints, 0.0);

        auto searchPaths = context->searchPaths;
        searchPaths.push_back(".");
        searchPaths.push_back("shaders");
        searchPaths.push_back("../share/rocky/shaders");

        auto vertShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT, "main",
            vsg::findFile("rocky.line.vert", searchPaths),
            context->readerWriterOptions);
        auto fragShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT, "main",
            vsg::findFile("rocky.line.frag", searchPaths),
            context->readerWriterOptions);

        if (!vertShader || !fragShader)
        {
            _transform = vsg::MatrixTransform::create();
            this->addChild(_transform);
            return;
        }

        auto shaderSet = vsg::ShaderSet::create(
            vsg::ShaderStages{vertShader, fragShader});
        shaderSet->addAttributeBinding("in_vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_vertex_prev", "", 1, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_vertex_next", "", 2, VK_FORMAT_R32G32B32_SFLOAT, {});
        shaderSet->addAttributeBinding("in_color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, {});
        shaderSet->addDescriptorBinding("line", "", 0, 1,
                                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});
        rocky::PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_VERTEX_BIT);
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        auto config = vsg::GraphicsPipelineConfig::create(shaderSet);
        config->shaderHints = context->shaderCompileSettings;
        config->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        config->enableArray("in_vertex_prev", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        config->enableArray("in_vertex_next", VK_VERTEX_INPUT_RATE_VERTEX, 12);
        config->enableArray("in_color", VK_VERTEX_INPUT_RATE_VERTEX, 16);
        config->enableDescriptor("line");
        rocky::PipelineUtils::enableViewDependentData(config);

        struct SetStates : public vsg::Visitor
        {
            void apply(vsg::RasterizationState& s) override { s.cullMode = VK_CULL_MODE_NONE; }
            void apply(vsg::DepthStencilState& s) override { s.depthTestEnable = VK_TRUE; }
            void apply(vsg::ColorBlendState& s) override
            {
                s.attachments = vsg::ColorBlendState::ColorBlendAttachments{{true, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                                                                             VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                                                                             VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};
            }
        } ss;
        config->accept(ss);
        config->init();

        _vertices = vsg::vec3Array::create(_maxPoints);
        _verticesPrev = vsg::vec3Array::create(_maxPoints);
        _verticesNext = vsg::vec3Array::create(_maxPoints);
        _colors = vsg::vec4Array::create(_maxPoints);
        std::fill(_vertices->begin(), _vertices->end(), vsg::vec3(0, 0, 0));
        std::fill(_verticesPrev->begin(), _verticesPrev->end(), vsg::vec3(0, 0, 0));
        std::fill(_verticesNext->begin(), _verticesNext->end(), vsg::vec3(0, 0, 0));
        for (size_t i = 0; i < _maxPoints; ++i)
        {
            float alpha = float(i) / float(_maxPoints);
            (*_colors)[i] = vsg::vec4(1.f, 1.f, 0.f, 1.f - alpha);
        }
        _vertices->properties.dataVariance = vsg::DYNAMIC_DATA;
        _verticesPrev->properties.dataVariance = vsg::DYNAMIC_DATA;
        _verticesNext->properties.dataVariance = vsg::DYNAMIC_DATA;

        auto lineStyle = vsg::Value<LineStyle>::create();
        auto lineDescriptor = vsg::DescriptorBuffer::create(
            vsg::BufferInfoList{vsg::BufferInfo::create(lineStyle)}, 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        auto descriptorSet = vsg::DescriptorSet::create(config->layout->setLayouts[0], vsg::Descriptors{lineDescriptor});
        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, config->layout, 0, descriptorSet);

        auto commands = vsg::Commands::create();
        commands->addChild(config->bindGraphicsPipeline);
        commands->addChild(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, config->layout, rocky::VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX));
        commands->addChild(bindDescriptorSet);

        auto bindVB = vsg::BindVertexBuffers::create();
        bindVB->assignArrays({_vertices, _verticesPrev, _verticesNext, _colors});
        commands->addChild(bindVB);

        _draw = vsg::Draw::create(0, 1, 0, 0); // 初始 vertexCount=0
        commands->addChild(_draw);

        _transform = vsg::MatrixTransform::create();
        _transform->addChild(commands);
        this->addChild(_transform);
    }

    size_t _maxPoints = 3600;
    size_t _count = 0;
    bool _dirty = false;
    bool _originSet = false;
    vsg::dvec3 _originEcef{};

    std::vector<double> _times;    // 每个点对应的时间戳
    vsg::ref_ptr<vsg::Draw> _draw; // 改为成员变量
    vsg::ref_ptr<vsg::MatrixTransform> _transform;
    vsg::ref_ptr<vsg::vec3Array> _vertices;
    vsg::ref_ptr<vsg::vec3Array> _verticesPrev;
    vsg::ref_ptr<vsg::vec3Array> _verticesNext;
    vsg::ref_ptr<vsg::vec4Array> _colors;
};
