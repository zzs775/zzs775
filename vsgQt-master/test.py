import re

try:
    with open('c:/Users/cfh12/Desktop/rocky_qt/vsgQt-(rubulid)/vsgQt-master/examples/vsgqtviewer/main.cpp', 'r', encoding='utf-8') as f:
        text = f.read()

    start_marker = 'class SimulationUpdateHandler : public vsg::Inherit<vsg::Visitor, SimulationUpdateHandler>'
    end_marker = 'class EditorEventHandler'

    start_idx = text.find(start_marker)
    end_idx = text.find(end_marker)

    if start_idx != -1 and end_idx != -1:
        print('Found markers!')
        new_text = text[:start_idx] + '''class SimulationUpdateHandler : public vsg::Inherit<vsg::Visitor, SimulationUpdateHandler>
{
public:
    SimulationUpdateHandler(SimDataManager* dm, SimClock* clk = nullptr) : dataManager(dm), simClock(clk) {}

    struct EntityState
    {
        double smoothLon = 0.0, smoothLat = 0.0, smoothAlt = 0.0;
        bool hasSmoothPos = false;
        bool isVisible = true;

        vsg::ref_ptr<vsg::Switch> switchNode;
        vsg::ref_ptr<vsg::MatrixTransform> trailNode;
        vsg::ref_ptr<vsg::Switch> trailSwitch;
        vsg::ref_ptr<vsg::vec3Array> trailVertices;
        vsg::ref_ptr<vsg::vec4Array> trailColors;

        struct TrailPoint { vsg::dvec3 ecef; double time; };
        std::deque<TrailPoint> liveHistory;
        double lastKnownTime = -1.0;
    };

    void apply(vsg::FrameEvent& frame) override
    {
        if (!dataManager || !_G::entityManager) return;

        static constexpr int TRAIL_POINTS = 2000;
        static constexpr double TRAIL_SPAN = 600.0;
        double playbackSpeed = simClock ? simClock->multiplier() : 1.0;

        if (!bindTrailPipeline) {
            initTrailPipeline();
        }

        std::vector<InterpolatedPacket> latestData;
        double currentTime = 0.0;
        bool isReplay = (dataManager->acmiPacketCount() > 0 && simClock);

        if (isReplay) {
            currentTime = simClock->currentSeconds() + dataManager->acmiTimeMin();
            latestData = dataManager->getInterpolatedData(currentTime);
        } else {
            currentTime = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            auto rawData = dataManager->getLatestUdpData();
            for (const auto& [id, p] : rawData)
                latestData.push_back({id, p.lat, p.lon, p.alt, p.pitch, p.yaw, p.roll, std::string(p.name)});
        }

        std::unordered_set<std::string> entitiesInSnapshot;
        for (const auto& packet : latestData) entitiesInSnapshot.insert(packet.id);

        for (auto& [id, state] : _entityStates) {
            bool inSnapshot = entitiesInSnapshot.count(id) > 0;
            if (state.isVisible != inSnapshot) {
                state.isVisible = inSnapshot;
                if (state.switchNode) for (auto& child : state.switchNode->children) child.mask = inSnapshot;
                if (state.trailSwitch) state.trailSwitch->setAllChildren(inSnapshot);
                if (!inSnapshot) dataManager->removeEntity(QString::fromStdString(id));
            }
        }

        for (const auto& packet : latestData)
        {
            std::string id = packet.id;
            std::string name = packet.name;
            auto& state = _entityStates[id];

            if (!_G::entityManager->HasEntity(id)) {
                createEntityNodes(id, name, state);
            }

            if (isReplay) {
                state.smoothLon = packet.lon; state.smoothLat = packet.lat; state.smoothAlt = packet.alt;
            } else {
                if (!state.hasSmoothPos) {
                    state.smoothLon = packet.lon; state.smoothLat = packet.lat; state.smoothAlt = packet.alt; state.hasSmoothPos = true;
                } else {
                    double alpha = std::min(1.0, 0.7 * std::abs(playbackSpeed));
                    state.smoothLon += alpha * (packet.lon - state.smoothLon);
                    state.smoothLat += alpha * (packet.lat - state.smoothLat);
                    state.smoothAlt += alpha * (packet.alt - state.smoothAlt);
                }
            }

            auto worldSRS = rocky::SRS::ECEF;
            rocky::GeoPoint lla(rocky::SRS::WGS84, state.smoothLon, state.smoothLat, state.smoothAlt);
            auto ecef = lla.transform(worldSRS);
            vsg::dvec3 ecefP(ecef.x, ecef.y, ecef.z);

            vsg::dmat4 l2w = rocky::to_vsg(worldSRS.ellipsoid().topocentricToGeocentricMatrix({ecefP.x, ecefP.y, ecefP.z}));
            vsg::dmat4 full = l2w * vsg::rotate(vsg::radians(-packet.yaw), 0,0,1) * vsg::rotate(vsg::radians(packet.pitch), 1,0,0) * vsg::rotate(vsg::radians(packet.roll), 0,1,0);
            vsg::dvec3 fwd(full[1][0], full[1][1], full[1][2]), up(full[2][0], full[2][1], full[2][2]);
            const double NOZZLE_BACK = 8.0, NOZZLE_DOWN = 1.5;
            vsg::dvec3 nozzle = ecefP + fwd * (-NOZZLE_BACK) + up * (-NOZZLE_DOWN);

            if (state.trailNode) state.trailNode->matrix = vsg::translate(nozzle.x, nozzle.y, nozzle.z);
            if (state.lastKnownTime > 0.0 && currentTime < state.lastKnownTime - 1.0) state.liveHistory.clear();
            state.lastKnownTime = currentTime;

            bool entityExistsThisFrame = false;
            if (isReplay) {
                auto nowData = dataManager->getInterpolatedData(currentTime);
                for (auto& p : nowData) if (p.id == id) { entityExistsThisFrame = true; break; }
            } else entityExistsThisFrame = true;

            if (entityExistsThisFrame && state.isVisible) {
                if (state.liveHistory.empty() || std::abs(currentTime - state.liveHistory.front().time) > 0.01)
                    state.liveHistory.push_front({nozzle, currentTime});
                while (!state.liveHistory.empty() && currentTime - state.liveHistory.back().time > TRAIL_SPAN)
                    state.liveHistory.pop_back();
            }

            if (!entityExistsThisFrame || !state.isVisible || state.liveHistory.empty()) {
                vsg::dvec3 safePos = state.liveHistory.empty() ? nozzle : state.liveHistory.front().ecef;
                vsg::dvec3 localPos = safePos - nozzle;
                for (int i = 0; i < TRAIL_POINTS; ++i) { state.trailVertices->at(i) = vsg::vec3((float)localPos.x, (float)localPos.y, (float)localPos.z); state.trailColors->at(i) = vsg::vec4(0, 0, 0, 0); }
            } else {
                double step = TRAIL_SPAN / (double)(TRAIL_POINTS - 1);
                int hIdx = 0; vsg::dvec3 lastP = state.liveHistory.front().ecef;
                for (int i = 0; i < TRAIL_POINTS; ++i) {
                    double targetT = currentTime - step * i;
                    while (hIdx < (int)state.liveHistory.size() - 1 && state.liveHistory[hIdx].time > targetT) ++hIdx;
                    if (hIdx < (int)state.liveHistory.size() && std::abs(state.liveHistory[hIdx].time - targetT) < step * 2.0) {
                        lastP = state.liveHistory[hIdx].ecef;
                        vsg::dvec3 lP = lastP - nozzle; state.trailVertices->at(i) = vsg::vec3((float)lP.x, (float)lP.y, (float)lP.z);
                        float a = (i > TRAIL_POINTS * 0.8) ? std::max(0.0f, 1.0f - ((float)i/TRAIL_POINTS - 0.8f)*5.0f) : 1.0f;
                        state.trailColors->at(i) = vsg::vec4(0.31, 0.76, 0.97, a);
                    } else {
                        vsg::dvec3 lP = lastP - nozzle; state.trailVertices->at(i) = vsg::vec3((float)lP.x, (float)lP.y, (float)lP.z);
                        state.trailColors->at(i) = vsg::vec4(0,0,0,0);
                    }
                }
            }
            state.trailVertices->dirty(); state.trailColors->dirty();

            _G::entityManager->PatchEntity<TransformComponent>(id, [&](TransformComponent& tc) {
                if (tc.node) tc.node->matrix = fullMatrix * vsg::rotate(vsg::radians(90.0), 0.0, 0.0, 1.0);
            });

            if (!dataManager->updateEntityPosition(QString::fromStdString(id), state.smoothLat, state.smoothLon, state.smoothAlt, packet.yaw, packet.pitch, packet.roll)) {
                ModelType mType = (name.find("aim") != std::string::npos || name.find("missile") != std::string::npos) ? ModelType::Missile : ModelType::Aircraft;
                dataManager->addEntityWithId(QString::fromStdString(id), QString::fromStdString(name), mType, state.smoothLat, state.smoothLon, state.smoothAlt);
            }
        }
    }

    SimDataManager* dataManager = nullptr;
    SimClock* simClock = nullptr;
    bool needsCompile() const { return _needsCompile; }
    void clearCompileFlag() { _needsCompile = false; }

private:
    std::unordered_map<std::string, EntityState> _entityStates;
    bool _needsCompile = false;
    vsg::ref_ptr<vsg::BindGraphicsPipeline> bindTrailPipeline;

    void initTrailPipeline() {
        const char* vertSrc = R"(#version 450
            layout(location = 0) in vec3 vsg_Vertex;
            layout(location = 1) in vec4 vsg_Color;
            layout(push_constant) uniform PushConstants { mat4 projection; mat4 modelview; } pc;
            layout(location = 0) out vec4 outColor;
            void main() { gl_Position = pc.projection * pc.modelview * vec4(vsg_Vertex, 1.0); outColor = vsg_Color; })";
        const char* fragSrc = R"(#version 450
            layout(location = 0) in vec4 inColor;
            layout(location = 0) out vec4 fragColor;
            void main() { fragColor = inColor; })";
        auto vertexShader = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", vertSrc);
        auto fragmentShader = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragSrc);
        auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);
        shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
        shaderSet->addAttributeBinding("vsg_Color", "", 1, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
        vsg::PushConstantRanges vkPCRanges;
        for (const auto& pcr : shaderSet->pushConstantRanges) vkPCRanges.push_back(pcr.range);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{vsg::DescriptorSetLayout::create()}, vkPCRanges);
        vsg::GraphicsPipelineStates pipelineStates;
        auto vertexInputState = vsg::VertexInputState::create();
        vertexInputState->vertexBindingDescriptions = { {0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, {1, sizeof(vsg::vec4), VK_VERTEX_INPUT_RATE_VERTEX} };
        vertexInputState->vertexAttributeDescriptions = { {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, {1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0} };
        pipelineStates.push_back(vertexInputState);
        auto iaState = vsg::InputAssemblyState::create(); iaState->topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; pipelineStates.push_back(iaState);
        auto rasterState = vsg::RasterizationState::create(); rasterState->lineWidth = 2.0f; pipelineStates.push_back(rasterState);
        pipelineStates.push_back(vsg::MultisampleState::create());
        auto colorBlendState = vsg::ColorBlendState::create();
        colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{ {VK_TRUE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT} };
        pipelineStates.push_back(colorBlendState);
        pipelineStates.push_back(vsg::DepthStencilState::create());
        auto trailPipeline = vsg::GraphicsPipeline::create(pipelineLayout, shaderSet->getShaderStages(), pipelineStates);
        bindTrailPipeline = vsg::BindGraphicsPipeline::create(trailPipeline);
    }

    void createEntityNodes(const std::string& id, const std::string& name, EntityState& state) {
        std::string modelFile = inferModelFile(name);
        double modelScale = (name.find("aim") != std::string::npos || name.find("missile") != std::string::npos) ? 5000.0 : 10000.0;
        auto modelNode = ModelFactory::instance()->createVisualEntity(modelFile, QString::fromStdString(name), modelScale);
        if (!modelNode) modelNode = vsg::Group::create();
        state.switchNode = vsg::Switch::create(); state.switchNode->addChild(true, modelNode);
        _G::entityManager->CreateEntity(id, name, state.switchNode);

        state.trailVertices = vsg::vec3Array::create(2000);
        state.trailColors = vsg::vec4Array::create(2000);
        for (int i = 0; i < 2000; ++i) { state.trailVertices->at(i) = vsg::vec3(0, 0, 0); state.trailColors->at(i) = vsg::vec4(0, 0, 0, 0); }
        vsg::DataList vertexArrays; vertexArrays.push_back(state.trailVertices); vertexArrays.push_back(state.trailColors);
        auto bindVBs = vsg::BindVertexBuffers::create(0, vertexArrays);
        auto draw = vsg::Draw::create(2000, 1, 0, 0);
        auto stateGroup = vsg::StateGroup::create(); stateGroup->add(bindTrailPipeline); stateGroup->addChild(bindVBs); stateGroup->addChild(draw);
        state.trailNode = vsg::MatrixTransform::create(); state.trailNode->addChild(stateGroup); state.trailNode->setValue("DataVariance", vsg::DYNAMIC_DATA);
        state.trailSwitch = vsg::Switch::create(); state.trailSwitch->addChild(true, state.trailNode);
        _G::entityManager->CreateEntity(id + "_trail", name + "_trail", state.trailSwitch);
        _G::entityManager->PatchEntity<TransformComponent>(id + "_trail", [&](TransformComponent& tc) { if (tc.node) tc.node->matrix = vsg::dmat4(1.0); });
        _needsCompile = true;
    }

    std::string inferModelFile(const std::string& name) {
        std::string n = name; std::transform(n.begin(), n.end(), n.begin(), ::tolower);
        static const std::string BASE = "C:/Users/cfh12/Desktop/rocky_qt/sim.vsg-master/sim.vsg/data/3DModel/";
        if (n.find("f-16") != std::string::npos || n.find("f16") != std::string::npos) return BASE + "F-16A.glb";
        if (n.find("su-27") != std::string::npos || n.find("su27") != std::string::npos) return BASE + "su-27.glb";
        if (n.find("aim") != std::string::npos || n.find("r-77") != std::string::npos) return BASE + "AIM-120.glb";
        if (n.find("aim-9") != std::string::npos) return BASE + "AIM-9.glb";
        if (n.find("r27") != std::string::npos || n.find("r-27") != std::string::npos) return BASE + "R27.glb";
        if (n.find("r73") != std::string::npos || n.find("r-73") != std::string::npos) return BASE + "R73.glb";
        if (n.find("missile") != std::string::npos) return BASE + "AIM-120.glb";
        return BASE + "F-16A.glb";
    }
};
''' + text[end_idx:]
        
        with open('c:/Users/cfh12/Desktop/rocky_qt/vsgQt-(rubulid)/vsgQt-master/examples/vsgqtviewer/main.cpp', 'w', encoding='utf-8') as f:
            f.write(new_text)
        print('Successfully replaced.')
    else:
        print('Could not find markers.')
except Exception as e:
    print('Error:', e)
