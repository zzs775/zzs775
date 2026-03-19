#include "SimClock.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMainWindow>
#include <QQmlContext>
#include <QQuickView>
#include <QString>
#include <QSurfaceFormat>
#include <QTimer>
#include <QWidget>
#include <chrono>
#include <cmath>

#include "AcmiTelemetryForwarder.h"
#include <algorithm>
#include <optional>
#include <rocky/DateTime.h>
#include <rocky/GeoPoint.h>
#include <rocky/TMSImageLayer.h>
#include <rocky/vsg/MapManipulator.h>
#include <rocky/vsg/MapNode.h>
#include <rocky/vsg/SkyNode.h>
#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/terrain/TerrainNode.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vsg/all.h>
#include <vsg/io/read.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/utils/Builder.h>
#include <vsgQt/Viewer.h>
#include <vsgQt/Window.h>
#include <vsgXchange/all.h>

#include "ModelFactory.h"
#include "ModelListModel.h"
#include "QmlBridge.h"
#include "SimData.h"
#include "SimDataManager.h"

#include <rocky/ecs/Transform.h>
#include <rocky/vsg/ecs/ECSNode.h>
// 航迹拖尾：纯 VSG 实现，不依赖 Rocky ECS
#include "TrackNode.h"

extern AppState g_appState;
AppState g_appState;
static bool g_timelineInitialized = false; // 8分钟时间轴是否已初始化

vsg::dvec3 worldToLatLonAlt(const vsg::dvec3& p)
{
    const double a = 6378137.0;
    const double e2 = 0.00669437999014;
    const double PI = 3.14159265358979323846;
    double lon = std::atan2(p.y, p.x);
    double p_len = std::sqrt(p.x * p.x + p.y * p.y);
    double theta = std::atan2(p.z * a, p_len * a * std::sqrt(1.0 - e2));
    double lat = std::atan2(p.z + e2 * (1.0 - e2) * a * std::pow(std::sin(theta), 3),
                            p_len - e2 * a * std::pow(std::cos(theta), 3));
    double N = a / std::sqrt(1.0 - e2 * std::sin(lat) * std::sin(lat));
    double alt = p_len / std::cos(lat) - N;
    return vsg::dvec3(lat * 180.0 / PI, lon * 180.0 / PI, alt);
}

// 获取真实模型的物理尺寸（米）, 咱们的 VSG 的 targetSize 对应 AABB 最大的长边
double getRealisticModelSize(const std::string& name)
{
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    if (n.find("su-27") != std::string::npos || n.find("su27") != std::string::npos) return 21.9;
    if (n.find("f-16") != std::string::npos || n.find("f16") != std::string::npos) return 15.1;
    if (n.find("r-27") != std::string::npos || n.find("r27") != std::string::npos) return 4.1;
    if (n.find("aim-120") != std::string::npos || n.find("aim120") != std::string::npos) return 3.7;
    if (n.find("aim-9") != std::string::npos || n.find("aim9") != std::string::npos) return 3.0;
    // 有些旧代码可能只传递了 aim 这个字，默认分配为 AIM-120 处理
    if (n.find("aim") != std::string::npos) return 3.7;
    if (n.find("r-73") != std::string::npos || n.find("r73") != std::string::npos) return 2.9;
    if (n.find("missile") != std::string::npos) return 3.7;
    return 15.1; // 默认给个 F16 的大小，或者差不多的通用大小
}

// --- ECS 驱动的帧更新器 (Step 5) ---
// 它是整个仿真的核心，每一帧都会被调用，负责根据最新数据更新所有飞机/导弹的位置和状态
#include <rocky/vsg/imgui/ImGuiIntegration.h>

class SimulationUpdateHandler : public vsg::Inherit<vsg::Visitor, SimulationUpdateHandler>
{
public:
    // 实体追踪状态（用于平滑与显隐控制）
    struct EntityState
    {
        // 平滑坐标（渲染侧 EMA，用于消除时钟抖动和帧间卡顿）
        double smoothLon = 0.0;
        double smoothLat = 0.0;
        double smoothAlt = 0.0;
        // 平滑姿态（±180° 最短路径 EMA）
        double smoothYaw = 0.0;
        double smoothPitch = 0.0;
        double smoothRoll = 0.0;
        bool hasSmoothPos = false; // 首帧直接赋值，避免从 (0,0,0) 追过来

        // 显示状态控制
        bool isVisible = true;
        vsg::ref_ptr<vsg::MatrixTransform> transformNode;
        vsg::ref_ptr<TrackNode> trackNode; // 航迹线节点

        double lastDataReceivedTime = -1.0;
        bool isFadingOut = false;
    };

    std::unordered_map<std::string, EntityState> _entityStates;

    SimulationUpdateHandler(SimDataManager* dm, SimClock* clk = nullptr, vsg::ref_ptr<vsg::Group> entitiesRoot = nullptr, rocky::VSGContext context = {}, class AcmiTelemetryForwarder* fwd = nullptr) : dataManager(dm), simClock(clk), _entitiesRoot(entitiesRoot), rockyContext(context), forwarder(fwd) {}

    // 核心函数：每一帧渲染前执行，处理所有逻辑更新
    void apply(vsg::FrameEvent& frame) override
    {
        _frameCount++;

        if (!dataManager || !_entitiesRoot) return;

        // (播放速度暂不参与 EMA 计算，统一用固定 smoothingK)

        // 【第一阶段：数据获取】
        std::vector<InterpolatedPacket> latestData;

        bool isPlayback = (dataManager->acmiPacketCount() > 0 && simClock);
        if (isPlayback)
        {
            // [回放模式] 计算当前仿真绝对时间 = 进度时间 + 文件的起始绝对时间
            double absoluteTime = simClock->currentSeconds() + dataManager->acmiTimeMin();
            latestData = dataManager->getInterpolatedData(absoluteTime);
        }
        else
        {
            // [直播模式]
            auto rawData = dataManager->getLatestUdpData();
            for (const auto& [id, p] : rawData)
            {
                latestData.push_back({id, p.lat, p.lon, p.alt, p.pitch, p.yaw, p.roll, std::string(p.name)});
            }
        }

        // 【第二阶段：实体生命周期与显隐管理】
        std::unordered_set<std::string> entitiesInSnapshot;
        for (const auto& packet : latestData)
        {
            // 过滤未初始化的幽灵坐标 (0.0, 0.0)
            if (std::abs(packet.lat) > 1e-4 || std::abs(packet.lon) > 1e-4)
            {
                entitiesInSnapshot.insert(packet.id);
            }
        }

        auto currentTime = (dataManager->acmiPacketCount() > 0 && simClock)
                               ? (simClock->currentSeconds() + dataManager->acmiTimeMin())
                               : std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();

        for (auto it = _entityStates.begin(); it != _entityStates.end();)
        {
            const std::string& id = it->first;
            EntityState& state = it->second;

            bool inSnapshot = entitiesInSnapshot.count(id) > 0;

            if (inSnapshot)
            {
                state.lastDataReceivedTime = currentTime;
                state.isFadingOut = false;
            }
            else if (std::abs(currentTime - state.lastDataReceivedTime) > 15.0 && !state.isFadingOut)
            {
                // 超时15秒没有数据，标记完全淡出并移除
                // 注意：ACMI 匀速直线飞行时可能多秒不发更新，改为 15s 防止误删
                state.isFadingOut = true;
                inSnapshot = false;
            }

            if (state.isVisible != inSnapshot)
            {
                state.isVisible = inSnapshot;
                auto& ch = _entitiesRoot->children;
                if (!inSnapshot)
                {
                    ch.erase(std::remove(ch.begin(), ch.end(), vsg::ref_ptr<vsg::Node>(state.transformNode)), ch.end());
                    if (state.trackNode)
                    {
                        ch.erase(std::remove(ch.begin(), ch.end(), vsg::ref_ptr<vsg::Node>(state.trackNode)), ch.end());
                    }
                    dataManager->removeEntity(QString::fromStdString(id), false); // 自动超时不拉黑
                    _needsCompile = true;
                }
                else
                {
                    if (std::find(ch.begin(), ch.end(), vsg::ref_ptr<vsg::Node>(state.transformNode)) == ch.end())
                    {
                        ch.push_back(state.transformNode);
                        _needsCompile = true;
                    }
                }
            }

            if (state.isFadingOut)
            {
                it = _entityStates.erase(it); // 从 map 中彻底移除
            }
            else
            {
                ++it;
            }
        }

        // 【第三阶段：更新位置与矩阵计算】
        for (const auto& packet : latestData)
        {
            std::string id = packet.id;
            std::string name = packet.name;

            // 再次过滤无效坐标
            if (std::abs(packet.lat) < 1e-4 && std::abs(packet.lon) < 1e-4) continue;

            auto& state = _entityStates[id];

            // 1. [动态创建] 和手动放置完全一致的逻辑！
            if (!state.transformNode)
            {
                std::string modelFile = inferModelFile(name);

                double modelScale = getRealisticModelSize(name);

                auto modelNode = ModelFactory::instance()->createVisualEntity(
                    modelFile,
                    QString::fromStdString(id), // 需求：将标签内容改为模型 ID
                    modelScale,
                    packet.color);

                if (!modelNode)
                {
                    vsg::GeometryInfo info;
                    info.color.set(1.0f, 0.0f, 0.0f, 1.0f);
                    modelNode = vsg::Builder::create()->createBox(info);
                }

                auto transformNode = vsg::MatrixTransform::create();
                transformNode->addChild(modelNode);

                state.transformNode = transformNode;
                _entitiesRoot->addChild(transformNode);
                _needsCompile = true;
            }

            // 2. [坐标与矩阵更新] ── 统一使用渲染侧指数平滑 (EMA)
            // 公式：alpha = 1 - exp(-k * dt)，与帧率无关
            // ┌─────────────────────────────────────────────────────┐
            // │  smoothingK 调参指南 (渲染帧率 ≈ 60fps, dt ≈ 16ms) │
            // │  k = 8  → alpha ≈ 12% / 帧，极度顺滑，有约 4 帧滞后 │
            // │  k = 12 → alpha ≈ 18% / 帧，顺滑，约 2-3 帧滞后   │
            // │  k = 20 → alpha ≈ 28% / 帧，响应快，轻微弹性感     │
            // │  ACMI 帧间隔大时建议 k=10~12；实时 UDP 建议 k=15~20│
            // └─────────────────────────────────────────────────────┘
            constexpr double smoothingK = 12.0;      // ← 调这一个数字即可
            constexpr double nominalDt = 1.0 / 60.0; // 16ms 渲染周期
            const double renderAlpha = 1.0 - std::exp(-smoothingK * nominalDt);

            // 角度插值辅助：走 ±180° 最短路径，防止跨零点大幅跳转
            auto smoothAngle = [](double cur, double target, double a) -> double {
                double diff = target - cur;
                while (diff > 180.0) diff -= 360.0;
                while (diff < -180.0) diff += 360.0;
                return cur + a * diff;
            };

            // ★ 如果是在回放模式（已经有完美的精确 Lerp 结果），直接赋予坐标，绝不能再盖一次 EMA，否则会引发“弹簧追赶效应”导致高速剧烈抖动
            if (!state.hasSmoothPos || isPlayback)
            {
                // 100% 信任最新坐标
                state.smoothLon = packet.lon;
                state.smoothLat = packet.lat;
                state.smoothAlt = packet.alt;
                state.smoothYaw = packet.yaw;
                state.smoothPitch = packet.pitch;
                state.smoothRoll = packet.roll;
                state.hasSmoothPos = true;
            }
            else
            {
                // 只有直播 UDP 模式，因为不存在未来帧，才使用 EMA 弹性逼近
                // 位置平滑（经纬高均为线性量，直接 EMA）
                state.smoothLon += renderAlpha * (packet.lon - state.smoothLon);
                state.smoothLat += renderAlpha * (packet.lat - state.smoothLat);
                state.smoothAlt += renderAlpha * (packet.alt - state.smoothAlt);
                // 姿态平滑（角度走最短路径）
                state.smoothYaw = smoothAngle(state.smoothYaw, packet.yaw, renderAlpha);
                state.smoothPitch = smoothAngle(state.smoothPitch, packet.pitch, renderAlpha);
                state.smoothRoll = smoothAngle(state.smoothRoll, packet.roll, renderAlpha);
            }

            // 【提取计算核心 ECEF】
            auto worldSRS = rocky::SRS::ECEF;
            rocky::GeoPoint llaPoint(rocky::SRS::WGS84, state.smoothLon, state.smoothLat, state.smoothAlt);
            rocky::GeoPoint ecefPoint = llaPoint.transform(worldSRS);
            glm::dvec3 ecefPos(ecefPoint.x, ecefPoint.y, ecefPoint.z);

            vsg::dmat4 localToWorldMatrix = rocky::to_vsg(worldSRS.ellipsoid().topocentricToGeocentricMatrix(ecefPos));

            // ACMI 姿态旋转矩阵（平滑后，消除帧间抖动）
            vsg::dmat4 rotZ = vsg::rotate(vsg::radians(-state.smoothYaw), 0.0, 0.0, 1.0);
            vsg::dmat4 rotX = vsg::rotate(vsg::radians(state.smoothPitch), 1.0, 0.0, 0.0);
            vsg::dmat4 rotY = vsg::rotate(vsg::radians(state.smoothRoll), 0.0, 1.0, 0.0);
            vsg::dmat4 fullMatrix = localToWorldMatrix * rotZ * rotX * rotY;

            // 3. [矩阵更新与姿态修正]
            if (state.transformNode)
            {
                vsg::dmat4 baseRot = vsg::rotate(vsg::radians(90.0), 0.0, 0.0, 1.0);
                state.transformNode->matrix = fullMatrix * baseRot;

                // ★ 航迹线：动态编译并追加位置点
                if (!state.trackNode)
                {
                    state.trackNode = TrackNode::create(rockyContext, 3600);
                    _entitiesRoot->addChild(state.trackNode);
                    _needsCompile = true; // 只用全局 compile，不用 rockyContext->compile
                }
                vsg::dvec3 ecefVsg(ecefPos.x, ecefPos.y, ecefPos.z);

                // 检测时间跳变（拖动进度条、回放跳转）
                if (state.trackNode)
                {
                    double timeDiff = std::abs(currentTime - state.lastDataReceivedTime);
                    if (state.hasSmoothPos && timeDiff > 1.0) // 跳变超过1秒就清轨迹
                    {
                        state.trackNode->clear();
                    }
                }

                state.trackNode->addPoint(ecefVsg, currentTime);
                state.trackNode->update();
            }

            // 4. [UI数据同步]
            if (!dataManager->updateEntityPosition(
                    QString::fromStdString(id), state.smoothLat, state.smoothLon,
                    state.smoothAlt, packet.yaw, packet.pitch, packet.roll))
            {
                ModelType mType = ModelType::Aircraft;
                std::string n = name;
                std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                if (n.find("aim") != std::string::npos || n.find("missile") != std::string::npos) mType = ModelType::Missile;

                dataManager->addEntityWithId(
                    QString::fromStdString(id), QString::fromStdString(name),
                    mType, state.smoothLat, state.smoothLon, state.smoothAlt);
            }

            // 5. [遥测数据转发]
            if (forwarder)
            {
                // 使用仿真当前秒数，确保暂停同步
                forwarder->forward(QString::fromStdString(id), currentTime, state.smoothLat, state.smoothLon, state.smoothAlt, state.smoothYaw, state.smoothPitch, state.smoothRoll);
            }
        }
    }

    SimDataManager* dataManager = nullptr;
    SimClock* simClock = nullptr;
    vsg::ref_ptr<vsg::Group> _entitiesRoot = nullptr;
    rocky::VSGContext rockyContext;
    class AcmiTelemetryForwarder* forwarder = nullptr;

    bool needsCompile() const
    {
        return _needsCompile;
    }
    void clearCompileFlag()
    {
        _needsCompile = false;
    }

private:
    bool _needsCompile = false;
    int _frameCount = 0;
    std::string inferModelFile(const std::string& name)
    {
        std::string n = name;
        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
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

class EditorEventHandler : public vsg::Inherit<vsg::Visitor, EditorEventHandler>
{
public:
    struct PendingPlacement
    {
        std::string modelFile;
        QString name;
        QString entityId;
        vsg::dvec3 lla; // lat, lon, alt
    };

    EditorEventHandler(
        vsg::ref_ptr<rocky::MapNode> mapNode,
        vsg::ref_ptr<vsg::Group> entities,
        vsg::ref_ptr<vsg::Camera> camera,
        vsg::ref_ptr<vsg::Viewer> viewer,
        SimDataManager* dataManager,
        QWindow* window,
        vsg::ref_ptr<rocky::MapManipulator> mapManipulator) : _mapNode(mapNode),
                                                              _entities(entities),
                                                              _camera(camera),
                                                              _viewer(viewer),
                                                              _dataManager(dataManager),
                                                              _window(window),
                                                              _mapManipulator(mapManipulator) {}

    void apply(vsg::ButtonReleaseEvent& release) override
    {
        if (release.button == 1) _leftButtonDown = false;

        if (!g_appState.followedTargetId.isEmpty() && !g_appState.isPlacingMode)
        {
            if (_mapManipulator)
            {
                auto vpBefore = _mapManipulator->viewpoint();

                vsg::ButtonReleaseEvent synthetic = release;
                // 注意：如果不是左键平移（比如是右键或中键旋转），则转发中心坐标
                // 如果是左键（button 1），为了让平移生效，转发原始坐标
                if (release.button != 1)
                {
                    synthetic.x = _windowWidth / 2;
                    synthetic.y = _windowHeight / 2;
                    _mapManipulator->apply(synthetic);

                    auto vpAfter = _mapManipulator->viewpoint();
                    rocky::Viewpoint vpCorrected;
                    vpCorrected.point = vpBefore.point;
                    vpCorrected.pointFunction = [dataManager = _dataManager, idStr = g_appState.followedTargetId.toStdString()]() -> rocky::GeoPoint {
                        auto* e = dataManager->findEntityById(QString::fromStdString(idStr));
                        if (e) return rocky::GeoPoint(rocky::SRS::WGS84, e->lon, e->lat, e->alt);
                        return rocky::GeoPoint(rocky::SRS::WGS84, 0, 0, 0);
                    };
                    vpCorrected.range = vpBefore.range;
                    vpCorrected.heading = vpAfter.heading;
                    vpCorrected.pitch = vpAfter.pitch;
                    _mapManipulator->setViewpoint(vpCorrected, std::chrono::milliseconds(0));
                }
                else
                {
                    // [需求] 锁定模型时，禁用左键平移事件，防止视角偏移
                    // _mapManipulator->apply(release);
                }
            }
            release.handled = true;
            return;
        }
    }

    void apply(vsg::ButtonPressEvent& press) override
    {
        if (press.button == 1)
        {
            _leftButtonDown = true;
            _buttonDownPos = vsg::dvec2(static_cast<double>(press.x), static_cast<double>(press.y));
        }

        _lastClickTime = std::chrono::steady_clock::now();
        _lastClickButton = press.button;
        _lastClickPos = vsg::dvec2(static_cast<double>(press.x), static_cast<double>(press.y));

        if (!g_appState.followedTargetId.isEmpty() && !g_appState.isPlacingMode)
        {
            if (_mapManipulator)
            {
                auto vpBefore = _mapManipulator->viewpoint();

                vsg::ButtonPressEvent synthetic = press;
                // 同上，左键按住不进行中心点欺骗，否则平移动不了
                if (press.button != 1)
                {
                    synthetic.x = _windowWidth / 2;
                    synthetic.y = _windowHeight / 2;
                    _mapManipulator->apply(synthetic);

                    auto vpAfter = _mapManipulator->viewpoint();
                    rocky::Viewpoint vpCorrected;
                    vpCorrected.point = vpBefore.point;
                    vpCorrected.pointFunction = [dataManager = _dataManager, idStr = g_appState.followedTargetId.toStdString()]() -> rocky::GeoPoint {
                        auto* e = dataManager->findEntityById(QString::fromStdString(idStr));
                        if (e) return rocky::GeoPoint(rocky::SRS::WGS84, e->lon, e->lat, e->alt);
                        return rocky::GeoPoint(rocky::SRS::WGS84, 0, 0, 0);
                    };
                    vpCorrected.range = vpBefore.range;
                    vpCorrected.heading = vpAfter.heading;
                    vpCorrected.pitch = vpAfter.pitch;
                    _mapManipulator->setViewpoint(vpCorrected, std::chrono::milliseconds(0));
                }
                else
                {
                    // [需求] 锁定模型时，禁用左键平移事件
                    // _mapManipulator->apply(press);
                }
            }

            press.handled = true;
            return;
        }

        if (!g_appState.isPlacingMode) return;

        // 右键取消放置
        if (press.button == 3)
        { // 3 is usually right click in OSG/VSG
            cancelPlacement();
            press.handled = true;
            return;
        }

        if (press.button != 1) return;

        if (press.handled)
        {
            qDebug() << ">>> 事件已被处理，跳过";
            return;
        }

        press.handled = true;

        qDebug() << "=== 射线求交诊断 ===";
        qDebug() << "  鼠标坐标 (x, y):" << press.x << press.y;

        // 策略1：对 mapNode 做射线求交
        auto intersector = vsg::LineSegmentIntersector::create(*_camera, press.x, press.y);
        _mapNode->accept(*intersector);
        qDebug() << "  [策略1 mapNode] 交点数量:" << intersector->intersections.size();

        if (!intersector->intersections.empty())
        {
            std::sort(intersector->intersections.begin(), intersector->intersections.end(),
                      [](const auto& a, const auto& b) {
                          return a->ratio < b->ratio;
                      });

            auto hit = intersector->intersections.front();
            vsg::dvec3 hitPoint = hit->worldIntersection;
            placeModelAt(hitPoint);
            return;
        }

        // 策略2：WGS84 椭球面解析求交回退
        qDebug() << ">>> 射线未与地球相交 — 尝试椭球面回退求交";
        auto fallbackHit = ellipsoidIntersect(press.x, press.y);
        if (fallbackHit.has_value())
        {
            qDebug() << ">>> 椭球面回退求交成功";
            placeModelAt(fallbackHit.value());
        }
        else
        {
            qDebug() << ">>> 椭球面回退求交也失败，放弃放置";
        }
    }

    void apply(vsg::ConfigureWindowEvent& resize) override
    {
        _windowWidth = resize.width;
        _windowHeight = resize.height;
    }

    // ESC 键取消放置
    void apply(vsg::KeyPressEvent& key) override
    {
        if (!g_appState.isPlacingMode) return;
        if (key.keyBase == vsg::KEY_Escape)
        {
            cancelPlacement();
            key.handled = true;
        }
    }

    void cancelPlacement()
    {
        g_appState.isPlacingMode = false;
        g_appState.pendingModelPath = "";
        g_appState.pendingModelType = "";
        if (_window)
        {
            _window->setCursor(Qt::ArrowCursor);
        }
        qDebug() << ">>> 退出放置模式";
    }

    // 需求1：鼠标移动时实时输出经纬度以及拖拽检测
    void apply(vsg::MoveEvent& move) override
    {
        if (!g_appState.isPlacingMode) return;

        // 100ms 节流，避免日志刷屏
        static auto lastLogTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLogTime).count() < 100) return;
        lastLogTime = now;

        auto intersector = vsg::LineSegmentIntersector::create(*_camera, move.x, move.y);
        _mapNode->accept(*intersector);
        if (intersector->intersections.empty()) return;

        auto hit = intersector->intersections.front();
        vsg::dvec3 lla = worldToLatLonAlt(hit->worldIntersection);
        (void)lla; // Suppress unused warning
    }

    bool needsCompile() const
    {
        return _needsCompile;
    }
    void clearCompileFlag()
    {
        _needsCompile = false;
    }

    bool hasPendingOperations() const
    {
        return !_pendingPlacements.empty() || !_pendingRemovals.empty();
    }

    // 将实体从场景中移除（加入待删除队列）
    void removeEntityNode(const QString& id)
    {
        _pendingRemovals.append(id);
        _needsCompile = true;
        qDebug() << ">>> 模型删除请求已加入队列, ID:" << id;
    }

    // 清空所有实体节点
    void removeAllEntityNodes()
    {
        _entities->children.clear();
        _pendingRemovals.clear();
        _placedNodes.clear();
        _needsCompile = true;
        qDebug() << ">>> 已清空所有场景模型";
    }

    void processPendingOperations()
    {
        for (const auto& placement : _pendingPlacements)
        {
            double modelSize = getRealisticModelSize(placement.name.toStdString());
            auto factory = ModelFactory::instance();
            auto visual = factory->createVisualEntity(placement.modelFile, placement.entityId, modelSize);
            if (!visual)
            {
                qWarning() << ">>> 模型加载失败，使用默认方块";
                vsg::GeometryInfo info;
                info.color.set(1.0f, 0.0f, 0.0f, 1.0f);
                visual = vsg::Builder::create()->createBox(info);
            }

            auto transformNode = vsg::MatrixTransform::create();
            transformNode->addChild(visual);

            auto worldSRS = rocky::SRS::ECEF;
            rocky::GeoPoint llaPoint(rocky::SRS::WGS84, placement.lla.y, placement.lla.x, placement.lla.z);
            rocky::GeoPoint ecefPoint = llaPoint.transform(worldSRS);
            glm::dvec3 ecefPos(ecefPoint.x, ecefPoint.y, ecefPoint.z);

            vsg::dmat4 localToWorld = rocky::to_vsg(
                worldSRS.ellipsoid().topocentricToGeocentricMatrix(ecefPos));

            vsg::dmat4 baseRot = vsg::rotate(vsg::radians(90.0), 0.0, 0.0, 1.0);

            transformNode->matrix = localToWorld * baseRot;
            _entities->addChild(transformNode);
            _placedNodes[placement.entityId] = transformNode;

            qDebug() << ">>> 模型已添加到场景, ID:" << placement.entityId;
        }
        _pendingPlacements.clear();

        // 处理待删除的节点
        for (const auto& id : _pendingRemovals)
        {
            auto it = _placedNodes.find(id);
            if (it != _placedNodes.end())
            {
                auto& ch = _entities->children;
                ch.erase(std::remove(ch.begin(), ch.end(), vsg::ref_ptr<vsg::Node>(it->second)), ch.end());
                _placedNodes.erase(it);
                qDebug() << ">>> 已移除手动放置的模型, ID:" << id;
            }
            else
            {
                qDebug() << ">>> 未找到手动放置的模型节点, ID:" << id << "(可能由 SimulationUpdateHandler 管理)";
            }
            _needsCompile = true;
        }
        _pendingRemovals.clear();
    }

private:
    vsg::ref_ptr<rocky::MapNode> _mapNode;
    vsg::ref_ptr<vsg::Group> _entities;
    vsg::ref_ptr<vsg::Camera> _camera;
    vsg::ref_ptr<vsg::Viewer> _viewer;
    SimDataManager* _dataManager = nullptr;
    QWindow* _window = nullptr;
    vsg::ref_ptr<rocky::MapManipulator> _mapManipulator;
    uint32_t _windowWidth = 1920;
    uint32_t _windowHeight = 1080;
    bool _needsCompile = false;
    std::map<QString, vsg::ref_ptr<vsg::MatrixTransform>> _placedNodes; // 追踪手动放置的模型节点

    std::chrono::steady_clock::time_point _lastClickTime = std::chrono::steady_clock::now();
    uint32_t _lastClickButton = 0;
    vsg::dvec2 _lastClickPos;
    bool _leftButtonDown = false;
    vsg::dvec2 _buttonDownPos;

    std::vector<PendingPlacement> _pendingPlacements;
    QList<QString> _pendingRemovals; // 待删除节点 ID 队列

    void placeModelAt(const vsg::dvec3& pos)
    {
        qDebug() << "=== 放置位置诊断 ===";
        qDebug() << "  ECEF 坐标:" << pos.x << pos.y << pos.z;

        auto worldSRS = rocky::SRS::ECEF;
        rocky::GeoPoint ecefPoint(worldSRS, pos.x, pos.y, pos.z);
        rocky::GeoPoint llaPoint = ecefPoint.transform(rocky::SRS::WGS84);

        // 强制固定高度为 10000 米
        llaPoint.z = 10000.0;

        rocky::GeoPoint finalEcef = llaPoint.transform(rocky::SRS::ECEF);

        qDebug() << "  放置经纬高 (lla) Lon:" << llaPoint.x << "Lat:" << llaPoint.y << "Alt:" << llaPoint.z;

        ModelType entityType = ModelType::Aircraft;
        QString typeStr = g_appState.pendingModelType.toLower();
        if (typeStr.contains("ship"))
            entityType = ModelType::Ship;
        else if (typeStr.contains("vehicle"))
            entityType = ModelType::Vehicle;
        else if (typeStr.contains("building"))
            entityType = ModelType::Building;
        else if (typeStr.contains("aim") || typeStr.contains("missile") ||
                 typeStr.contains("r27") || typeStr.contains("r73") ||
                 typeStr.contains("r-27") || typeStr.contains("r-73"))
            entityType = ModelType::Missile;

        QString baseName = g_appState.pendingModelType;
        if (baseName.endsWith(".glb", Qt::CaseInsensitive))
        {
            baseName = baseName.left(baseName.length() - 4);
        }
        if (baseName.isEmpty()) baseName = "Unknown";

        if (_dataManager) _dataManager->addEntityWithName(baseName, entityType, llaPoint.y, llaPoint.x, llaPoint.z); // lat, lon, alt
        QString entityId;
        QString name;
        if (_dataManager && _dataManager->count() > 0)
        {
            const auto& lastEntity = _dataManager->getAllData().last();
            name = lastEntity.name;
            entityId = lastEntity.id;
        }
        else
        {
            name = baseName;
            entityId = baseName + "_01";
        }

        std::string modelFile;
        if (!g_appState.pendingModelPath.isEmpty())
        {
            modelFile = g_appState.pendingModelPath.toStdString();
        }
        else if (!g_appState.pendingModelType.isEmpty())
        {
            modelFile = "C:/Users/cfh12/Desktop/rocky_qt/sim.vsg-master/sim.vsg/data/3DModel/" + g_appState.pendingModelType.toStdString();
        }
        else
        {
            modelFile = "models/F-16.glb";
        }

        PendingPlacement placement;
        placement.modelFile = modelFile;
        placement.name = name;
        placement.entityId = entityId;
        placement.lla = vsg::dvec3(llaPoint.y, llaPoint.x, llaPoint.z); // lat, lon, alt

        _pendingPlacements.push_back(placement);
        _needsCompile = true;

        qDebug() << ">>> 模型放置请求已加入队列";

        // 连点模式：不退出放置模式，用户可以继续放置模型
        // 按 ESC 键或右键可退出放置模式
    }

    // 需求3：WGS84 椭球面解析求交回退
    std::optional<vsg::dvec3> ellipsoidIntersect(int32_t screenX, int32_t screenY)
    {
        // 获取投影矩阵和视图矩阵
        vsg::dmat4 projMatrix;
        if (auto persp = _camera->projectionMatrix.cast<vsg::Perspective>())
        {
            projMatrix = persp->transform();
        }
        else
        {
            qDebug() << "  [椭球回退] 无法获取透视投影矩阵";
            return std::nullopt;
        }
        vsg::dmat4 viewMatrix = _camera->viewMatrix->transform();

        // 获取视口
        auto viewportState = _camera->viewportState;
        if (!viewportState || viewportState->viewports.empty()) return std::nullopt;
        auto& vp = viewportState->viewports.front();
        double vpWidth = vp.width;
        double vpHeight = vp.height;
        double vpX = vp.x;
        double vpY = vp.y;
        if (vpWidth <= 0 || vpHeight <= 0) return std::nullopt;

        // 屏幕坐标 -> 归一化设备坐标 (NDC), 考虑视口偏移
        double ndcX = 2.0 * (screenX - vpX) / vpWidth - 1.0;
        double ndcY = 2.0 * (screenY - vpY) / vpHeight - 1.0; // Vulkan Y: 0 at top

        // NDC -> 视空间：用齐次除法正确反投影
        vsg::dmat4 invProj = vsg::inverse(projMatrix);
        vsg::dvec4 nearClip(ndcX, ndcY, 0.0, 1.0);
        vsg::dvec4 farClip(ndcX, ndcY, 1.0, 1.0);
        vsg::dvec4 nearEye = invProj * nearClip;
        vsg::dvec4 farEye = invProj * farClip;
        // 齐次除法
        if (std::abs(nearEye.w) > 1e-12) nearEye = nearEye / nearEye.w;
        if (std::abs(farEye.w) > 1e-12) farEye = farEye / farEye.w;

        // 视空间 -> 世界空间
        vsg::dmat4 invView = vsg::inverse(viewMatrix);
        vsg::dvec4 nearWorld = invView * nearEye;
        vsg::dvec4 farWorld = invView * farEye;

        vsg::dvec3 rayOrigin(nearWorld.x, nearWorld.y, nearWorld.z);
        vsg::dvec3 rayEnd(farWorld.x, farWorld.y, farWorld.z);
        vsg::dvec3 rayDir = vsg::normalize(rayEnd - rayOrigin);

        // 如果 camera 有 LookAt，直接用 eye 作为起点（最可靠）
        if (auto lookAt = _camera->viewMatrix.cast<vsg::LookAt>())
        {
            rayOrigin = lookAt->eye;
            rayDir = vsg::normalize(rayEnd - rayOrigin);
        }

        qDebug() << "  [椭球回退] 射线起点:" << rayOrigin.x << rayOrigin.y << rayOrigin.z;
        qDebug() << "  [椭球回退] 射线方向:" << rayDir.x << rayDir.y << rayDir.z;

        // 与 WGS84 椭球面求交: (x/a)^2 + (y/a)^2 + (z/b)^2 = 1
        const double a = 6378137.0;      // 赤道半径
        const double b = 6356752.314245; // 极半径
        vsg::dvec3 o(rayOrigin.x / a, rayOrigin.y / a, rayOrigin.z / b);
        vsg::dvec3 d(rayDir.x / a, rayDir.y / a, rayDir.z / b);
        double A = vsg::dot(d, d);
        double B = 2.0 * vsg::dot(o, d);
        double C = vsg::dot(o, o) - 1.0;
        double disc = B * B - 4.0 * A * C;

        qDebug() << "  [椭球回退] 判别式:" << disc;

        if (disc < 0)
        {
            qDebug() << "  椭球面判别式 < 0，射线未与地球相交";
            return std::nullopt;
        }

        double sqrtDisc = std::sqrt(disc);
        double root1 = (-B - sqrtDisc) / (2.0 * A);
        double root2 = (-B + sqrtDisc) / (2.0 * A);

        double t = -1.0;
        if (root1 > 0.0 && root2 > 0.0)
        {
            t = std::min(root1, root2);
        }
        else if (root1 > 0.0)
        {
            t = root1;
        }
        else if (root2 > 0.0)
        {
            t = root2;
        }

        if (t < 0.0)
        {
            qDebug() << "  椭球面交点在射线背后, root1=" << root1 << "root2=" << root2;
            return std::nullopt;
        }

        vsg::dvec3 hitPoint = rayOrigin + rayDir * t;
        qDebug() << "  椭球面交点 ECEF:" << hitPoint.x << hitPoint.y << hitPoint.z;
        return hitPoint;
    }
};

// 创建 ImGui Context Node 来绘制悬浮标签
class TagsImGuiNode : public vsg::Inherit<rocky::ImGuiContextNode, TagsImGuiNode>
{
public:
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<SimulationUpdateHandler> updater;

    TagsImGuiNode() = default;

    void render(ImGuiContext* context) const override
    {
        if (context) ImGui::SetCurrentContext(context);

        // 每帧从相机实时获取视图矩阵和投影矩阵（随拖动实时更新）
        vsg::dmat4 viewMat = camera->viewMatrix->transform();
        vsg::dmat4 projMat = camera->projectionMatrix->transform();
        vsg::dmat4 vp = projMat * viewMat;

        // 从视图矩阵逆矩阵提取相机的世界坐标（用于地球遮挡检测）
        vsg::dmat4 invView = vsg::inverse(viewMat);
        vsg::dvec3 camWorld(invView[3][0], invView[3][1], invView[3][2]);

        ImGuiIO& io = ImGui::GetIO();
        double w = io.DisplaySize.x;
        double h = io.DisplaySize.y;
        if (w <= 0.0 || h <= 0.0) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PopStyleVar();
            return;
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("TagsOverlay", nullptr, flags))
        {
            auto* drawList = ImGui::GetWindowDrawList();
            rocky::SRSOperation toEcef = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
            // WGS84 赤道半径（米），用于地球遮挡球体检测（略小一点防止自遮挡）
            const double R = 6371000.0;

            for (auto& pair : updater->_entityStates)
            {
                const auto& state = pair.second;
                if (!state.isVisible || (state.smoothLat == 0.0 && state.smoothLon == 0.0))
                    continue;

                // === 1. WGS84 (lon, lat, alt) → ECEF 三维坐标 ===
                vsg::dvec3 lonLatAlt(state.smoothLon, state.smoothLat, state.smoothAlt);
                vsg::dvec3 ecef;
                toEcef(lonLatAlt, ecef);

                // === 2. 地球球体遮挡深度测试 ===
                // 从相机到模型的射线，与地球球体求交
                // 若交点在相机与模型之间，说明地球挡住了模型 → 隐藏标签
                vsg::dvec3 toModel = ecef - camWorld;
                double distToModel = vsg::length(toModel);
                if (distToModel < 1.0) continue;
                vsg::dvec3 rayDir = toModel / distToModel;

                double b    = 2.0 * vsg::dot(camWorld, rayDir);
                double cval = vsg::dot(camWorld, camWorld) - R * R;
                double disc = b * b - 4.0 * cval;
                if (disc > 0.0) {
                    double sq = std::sqrt(disc);
                    double t1 = (-b - sq) * 0.5;
                    double t2 = (-b + sq) * 0.5;
                    // 交点在 [ε, distToModel) 范围内 → 被地球遮挡
                    if ((t1 > 100.0 && t1 < distToModel - 100.0) ||
                        (t2 > 100.0 && t2 < distToModel - 100.0))
                        continue;
                }

                // === 3. 三维坐标 → 裁剪坐标 → NDC ===
                vsg::dvec4 clip = vp * vsg::dvec4(ecef.x, ecef.y, ecef.z, 1.0);
                if (clip.w <= 0.0) continue; // 在摄像机背面

                vsg::dvec3 ndc(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);

                // 屏幕范围剔除
                if (ndc.x < -1.0 || ndc.x > 1.0 ||
                    ndc.y < -1.0 || ndc.y > 1.0) continue;
                // Vulkan 深度范围 [0, 1] 剔除
                if (ndc.z < 0.0 || ndc.z > 1.0) continue;

                // === 4. NDC → 屏幕像素坐标 ===
                // 【关键修复】VSG 使用 Vulkan NDC 约定：Y 轴向下
                //   ndc.y = -1 → 屏幕顶部(y=0)
                //   ndc.y = +1 → 屏幕底部(y=h)
                // 正确公式：yScreen = (ndc.y + 1) * 0.5 * h
                // 旧公式 (1 - ndc.y) 是 OpenGL 约定，在 Vulkan 下 Y 轴完全翻转，
                // 导致标签出现在模型实际位置的镜像位置，拖动时明显漂移！
                double xScreen = (ndc.x + 1.0) * 0.5 * w;
                double yScreen = (ndc.y + 1.0) * 0.5 * h;

                // === 5. 绘制标签（始终在模型投影点正上方） ===
                const std::string& label = pair.first;
                ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
                const float kOffY = 52.0f;  // 标签底部距模型投影点的像素距离
                const float kPadX = 6.0f;
                const float kPadY = 4.0f;

                // 标签左上角：水平居中于投影点，垂直位于投影点上方 kOffY 像素
                ImVec2 textPos(
                    (float)xScreen - textSize.x * 0.5f,
                    (float)yScreen - textSize.y - kOffY
                );
                if (textPos.y < kPadY) textPos.y = kPadY; // 防止超出屏幕顶部

                ImVec2 boxMin(textPos.x - kPadX, textPos.y - kPadY);
                ImVec2 boxMax(textPos.x + textSize.x + kPadX, textPos.y + textSize.y + kPadY);
                // 指示线的锚点：标签框底部中心
                ImVec2 boxAnchor((boxMin.x + boxMax.x) * 0.5f, boxMax.y);
                // 模型在屏幕上的投影点
                ImVec2 modelPt((float)xScreen, (float)yScreen);

                // 背景（深蓝半透明）
                drawList->AddRectFilled(boxMin, boxMax, IM_COL32(4, 22, 80, 215), 5.0f);
                // 边框（青色发光效果）
                drawList->AddRect(boxMin, boxMax, IM_COL32(0, 210, 255, 240), 5.0f, 0, 1.8f);
                // 文字（白色）
                drawList->AddText(textPos, IM_COL32(230, 245, 255, 255), label.c_str());
                // 指示线：标签框底部中心 → 模型投影锚点
                drawList->AddLine(boxAnchor, modelPt, IM_COL32(0, 210, 255, 170), 1.6f);
                // 模型投影点：实心圆 + 外圈
                drawList->AddCircleFilled(modelPt, 3.5f, IM_COL32(0, 220, 255, 255));
                drawList->AddCircle(modelPt, 6.5f, IM_COL32(0, 220, 255, 100), 0, 1.2f);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
};

int main(int argc, char* argv[])
{
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    // system("chcp 65001"); // Qt Creator 环境下调用 system 可能导致静默退出，暂时注释

    QApplication app(argc, argv);
    QString binDir = QCoreApplication::applicationDirPath();
    qputenv("VSG_FILE_PATH", binDir.toLocal8Bit());

    // 强制开启动态大气层（通过 SkyNode）
    bool useSky = true;

    // 动态解析 ROCKY_FILE_PATH
    // Rocky 需要 ROCKY_FILE_PATH 直接指向包含 shaders 的目录，即 install/share/rocky
    QString rockySharePath = "C:/Users/cfh12/Desktop/rocky_qt/rocky-main (1)/install/share/rocky";
    QDir testDir(binDir);
    while (!testDir.isRoot())
    {
        if (QDir(testDir.absoluteFilePath("install/share/rocky/shaders")).exists())
        {
            rockySharePath = testDir.absoluteFilePath("install/share/rocky");
            break;
        }
        testDir.cdUp();
    }

    if (!QDir(rockySharePath).exists())
    {
        qWarning() << "CRITICAL ERROR: ROCKY_FILE_PATH 路径不存在，地图着色器将无法加载！" << rockySharePath;
        rockySharePath = binDir + "/share/rocky";
    }
    qputenv("ROCKY_FILE_PATH", rockySharePath.toLocal8Bit());

    auto viewer = vsgQt::Viewer::create();
    auto vsgOptions = vsg::Options::create();
    // vsgXchange 在 ModelFactory 中延迟加载，避免启动时同步加载所有插件导致卡死 5 分钟
    vsgOptions->paths.push_back(binDir.toStdString());
    vsgOptions->paths.push_back((binDir + "/fonts").toStdString());
    ModelFactory::instance()->init(vsgOptions);

    // 搜索路径包含 Rocky 资源
    vsgOptions->paths.push_back(rockySharePath.toStdString());

    auto rockyContext = rocky::VSGContextFactory::create(viewer);
    rockyContext->readerWriterOptions = vsgOptions;
    rockyContext->searchPaths.push_back(rockySharePath.toStdString());
    rockyContext->searchPaths.push_back(binDir.toStdString());

    auto mapNode = rocky::MapNode::create(rockyContext);

    if (mapNode->map)
    {
        auto layer = rocky::TMSImageLayer::create();
        layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
        mapNode->map->add(layer);
    }

    auto entities = vsg::Group::create();
    auto scene = vsg::Group::create();

    vsg::ref_ptr<rocky::SkyNode> skyNode;
    if (useSky)
    {
        skyNode = rocky::SkyNode::create(rockyContext);
        skyNode->setWorldSRS(rocky::SRS::ECEF); // ★ 必须用 ECEF，不能用 WGS84
        skyNode->setShowAtmosphere(true);

        scene->addChild(skyNode);

        // ★ 调整光照强度，防止模型变黑
        if (skyNode->ambient)
        {
            skyNode->ambient->color = vsg::vec3(0.5f, 0.5f, 0.5f); // 提升环境光
            skyNode->ambient->intensity = 1.2f;
        }
        if (skyNode->sun)
        {
            skyNode->sun->color = vsg::vec3(1.0f, 1.0f, 1.0f);
            skyNode->sun->intensity = 3.5f;
        }
    }
    else
    {
        scene->addChild(vsg::createHeadlight());

        auto fixedLight = vsg::DirectionalLight::create();
        fixedLight->direction = vsg::normalize(vsg::dvec3(0.2, 0.5, -1.0));
        fixedLight->color = vsg::vec3(1.0f, 1.0f, 1.0f);
        fixedLight->intensity = 2.8f;
        scene->addChild(fixedLight);
    }

    // 1. 基础节点（后渲染/或保持相对顺序）
    scene->addChild(mapNode);
    scene->addChild(entities);

    auto traits = vsg::WindowTraits::create();
    traits->width = 1600;
    traits->height = 900; // 与下方 setGeometry 保持一致
    traits->windowTitle = "Sim Platform";
    traits->debugLayer = false;

    auto vsgWindow = new vsgQt::Window(viewer, traits);
    vsgWindow->setFlags(vsgWindow->flags() | Qt::FramelessWindowHint);
    vsgWindow->initializeWindow();

    // [修复 Bug1] traits 与 setGeometry 尺寸统一，避免初始 aspect 被提前算错
    vsgWindow->setGeometry(100, 100, 1600, 900);
    vsgWindow->show();

    // [修复 Bug1] 改用 Vulkan 真实物理像素初始化相机，自动处理高 DPI
    VkExtent2D extent;
    if (vsgWindow->windowAdapter)
    {
        extent = vsgWindow->windowAdapter->extent2D();
    }
    else
    {
        // 回退：使用与 traits 一致的初始值
        extent = {static_cast<uint32_t>(traits->width), static_cast<uint32_t>(traits->height)};
    }
    if (extent.width == 0 || extent.height == 0)
    {
        extent = {1600u, 900u};
    }

    auto perspective = vsg::Perspective::create(30.0,
                                                static_cast<double>(extent.width) / static_cast<double>(extent.height),
                                                100.0, 6378137.0 * 20);

    auto lookAt = vsg::LookAt::create(vsg::dvec3(0, -6378137 * 3, 0), vsg::dvec3(0, 0, 0), vsg::dvec3(0, 0, 1));

    auto camera = vsg::Camera::create(
        perspective,
        lookAt,
        vsg::ViewportState::create(extent));

    SimDataManager* dataManager = new SimDataManager(&app);
    dataManager->startUdpReceiver(19999); // 启动二进制 UDP 接收器

    ModelListModel* modelListModel = new ModelListModel(&app);
    QString modelDir = "C:/Users/cfh12/Desktop/rocky_qt/sim.vsg-master/sim.vsg/data/3DModel";
    modelListModel->loadFromDirectory(modelDir);

    auto* telemetryForwarder = new AcmiTelemetryForwarder(&app);
    auto* clock = new SimClock(&app);
    clock->setTimeRange(rocky::DateTime(2018, 9, 15, 12.0), rocky::DateTime(2018, 9, 16, 12.0));
    clock->setAnimating(true);

    if (skyNode)
    {
        skyNode->setDateTime(clock->currentDateTime());
    }

    auto simUpdateHandler = SimulationUpdateHandler::create(dataManager, nullptr, entities, rockyContext, telemetryForwarder);
    simUpdateHandler->simClock = clock;

    QmlBridge* bridge = new QmlBridge(&app);
    bridge->setBackgroundWindow(vsgWindow);
    bridge->setTelemetryForwarder(telemetryForwarder);
    bridge->setSimClock(clock);
    qDebug() << "=== 模型目录加载完成，共" << modelListModel->rowCount() << "个模型";

    auto mapManipulator = rocky::MapManipulator::create(mapNode, vsgWindow->windowAdapter, camera, rockyContext);

    auto editor = EditorEventHandler::create(mapNode, entities, camera, viewer, dataManager, vsgWindow, mapManipulator);

    viewer->addEventHandler(editor);
    viewer->addEventHandler(mapManipulator);

    // 连接已移至后面 bridge 实例化之后

    // === 【新增】：注册 ImGui 浮空标签管线 ===
    auto renderImGui = rocky::RenderImGuiContext::create(vsgWindow->windowAdapter, nullptr);
    auto tagsNode = TagsImGuiNode::create();
    tagsNode->camera = camera;
    tagsNode->updater = simUpdateHandler;
    renderImGui->add(tagsNode);
    scene->addChild(renderImGui);
    
    // 把 ImGui 事件监听加入 viewer (处理屏幕分辨率自适应)
    auto imGuiEvents = rocky::SendEventsToImGuiContext::create(vsgWindow->windowAdapter, nullptr);
    viewer->addEventHandler(imGuiEvents);
    // ===================================

    auto commandGraph = vsg::createCommandGraphForView(*vsgWindow, camera, scene, VK_SUBPASS_CONTENTS_INLINE, false);
    if (!commandGraph->children.empty())
    {
        if (auto renderGraph = commandGraph->children[0].cast<vsg::RenderGraph>())
        {
            renderGraph->clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        }
    }
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // ★ 关键：ECS 系统初始化（LineSystem 等需要在 compile 之前初始化 Vulkan 管线）
    // qDebug() << "=== [2.5] 初始化 ECS 系统...";
    // ecsNode->initialize(rockyContext);
    // qDebug() << "=== [2.5b] ECS 系统初始化完成";

    qDebug() << "=== [2.6] 强制预编译着色器...";
    viewer->compile();
    qDebug() << "=== [2.6b] 着色器预编译完成";

    qDebug() << "=== [3] 开始创建 QML 窗口...";

    // [修复 Bug2] 移除 WindowStaysOnTopHint，建立系统级父子关系 (TransientParent)
    QQuickView* qmlView = new QQuickView();
    qmlView->setFormat(qmlView->format());
    qmlView->setColor(Qt::transparent);
    qmlView->setFlags(Qt::FramelessWindowHint | Qt::Window | Qt::NoDropShadowWindowHint);
    qmlView->setTransientParent(vsgWindow); // 建立父子关系，主窗口关闭时联动

    qmlView->rootContext()->setContextProperty("ControlBridge", bridge);
    qmlView->rootContext()->setContextProperty("simModel", dataManager);
    qmlView->rootContext()->setContextProperty("modelListModel", modelListModel);
    qmlView->setSource(QUrl::fromLocalFile(binDir + "/TechOverlay.qml"));

    // 连接删除信号：面板删除模型时，同步移除场景中的3D节点
    QObject::connect(dataManager, &SimDataManager::entityRemoved, [editor, bridge, mapManipulator](const QString& id) {
        editor->removeEntityNode(id);
        if (g_appState.followedTargetId == id)
        {
            g_appState.followedTargetId = "";
            mapManipulator->clearViewpoint();
            emit bridge->targetUntethered();
            qDebug() << ">>> [锁定解除] 删除了当前锁定的目标，视角回退到地球";
        }
    });

    // 连接清空信号：批量删除时，清空所有场景节点
    QObject::connect(dataManager, &SimDataManager::sceneResetRequested, [editor, bridge, mapManipulator, clock]() {
        qDebug() << ">>> [Main] 收到场景重置请求，清空所有ECS实体";

        editor->removeAllEntityNodes();
        if (!g_appState.followedTargetId.isEmpty())
        {
            g_appState.followedTargetId = "";
            mapManipulator->clearViewpoint();
            emit bridge->targetUntethered();
            qDebug() << ">>> [锁定解除] 删除了全部目标，视角回退到地球";
        }

        if (clock)
        {
            clock->stop();
        }
        g_timelineInitialized = false; // 允许下次收到数据时重新初始化时间轴
        qDebug() << ">>> [Main] 时间轴标记已重置，等待新数据";
    });

    QObject::connect(dataManager, &QAbstractItemModel::modelReset, [editor, bridge, mapManipulator]() {
        editor->removeAllEntityNodes();
        if (!g_appState.followedTargetId.isEmpty())
        {
            g_appState.followedTargetId = "";
            mapManipulator->clearViewpoint();
            emit bridge->targetUntethered();
            qDebug() << ">>> [锁定解除] 删除了全部目标，视角回退到地球";
        }
    });

    // --- [核心功能：相机跟随锁定] ---
    // 当用户在 QML 列表中点击“跟随”按钮时，QML 会发出 focusEntityRequested 信号
    QObject::connect(bridge, &QmlBridge::focusEntityRequested, [dataManager, mapManipulator](const QString& id) {
        if (id.isEmpty())
        {
            // 如果 ID 为空，说明用户取消了锁定
            g_appState.followedTargetId = "";
            mapManipulator->clearViewpoint();
            qDebug() << ">>> [相机跟随] 已取消锁定，锁定点重置为地球";
            return;
        }

        auto* entity = dataManager->findEntityById(id);
        if (entity)
        {
            g_appState.followedTargetId = id;
            qDebug() << ">>> [相机跟随] 开始跟随模型 ID:" << id;

            rocky::Viewpoint vp;
            // 设置初始目标点
            vp.point = rocky::GeoPoint(rocky::SRS::WGS84, entity->lon, entity->lat, entity->alt);

            // 【关键点：动态绑定】原生 Rocky 的 tethering 机制
            // 传入一个函数指针，Rocky 每一帧渲染时都会调用它来获取目标飞机的最新位置
            vp.pointFunction = [dataManager, idStr = id.toStdString()]() -> rocky::GeoPoint {
                auto* e = dataManager->findEntityById(QString::fromStdString(idStr));
                if (e) return rocky::GeoPoint(rocky::SRS::WGS84, e->lon, e->lat, e->alt);
                return rocky::GeoPoint(rocky::SRS::WGS84, 0, 0, 0);
            };

            // 设置相机的观察角度和距离
            vp.range = rocky::Distance(300.0, rocky::Units::METERS); // ★ 缩小初始追踪距离，距离 5km -> 300m
            vp.pitch = rocky::Angle(-15.0, rocky::Units::DEGREES);    // 俯视 15 度，更平缓
            vp.heading = rocky::Angle(entity->yaw, rocky::Units::DEGREES);

            // 执行相机切换动画，时长 0.5 秒
            mapManipulator->setViewpoint(vp, std::chrono::milliseconds(500));
        }
    });

    // 初始位置与主窗口对齐后再显示
    qmlView->setGeometry(vsgWindow->geometry());
    qmlView->show();

    // --- [窗口位置同步] ---
    // 由于 QML 悬浮窗口是另一个窗口，必须强制它在位置和大小上始终与主 VSG 窗口重合
    QTimer* syncTimer = new QTimer();
    QObject::connect(syncTimer, &QTimer::timeout, [vsgWindow, qmlView]() {
        if (!vsgWindow || !qmlView) return;

        if (vsgWindow->isVisible() && vsgWindow->width() > 0 && vsgWindow->height() > 0)
        {
            // 将主窗口的局部坐标(0,0)映射到屏幕全局坐标
            QPoint pos = vsgWindow->mapToGlobal(QPoint(0, 0));
            // 强行把 QML 窗口挪过去并拉满大小
            qmlView->setGeometry(pos.x(), pos.y(), vsgWindow->width(), vsgWindow->height());
        }
    });
    syncTimer->start(16); // 16ms 同步一次，实现视觉上的“吸附”效果

    // --- [核心：主渲染循环] ---
    // 每一个仿真软件都有一个死循环，在这里是用 QTimer 模拟的
    QTimer* renderTimer = new QTimer();
    auto lastFrameTime = std::chrono::steady_clock::now();
    QObject::connect(renderTimer, &QTimer::timeout, [vsgWindow, viewer, camera, scene, &app, editor, simUpdateHandler, mapManipulator, bridge, lastFrameTime, dataManager, clock, rockyContext, skyNode]() mutable {
        if (!vsgWindow || !viewer || !vsgWindow->isExposed() || vsgWindow->width() <= 0 || vsgWindow->height() <= 0)
        {
            return;
        }

        try
        {
            // if (ecsNode)
            // {
            //     ecsNode->update(rockyContext);
            // }

            // 1. VSG 框架推进帧准备 (Advance)
            if (viewer->advanceToNextFrame())
            {
                // 处理窗口事件（按钮点击、相机拖拽等）
                viewer->handleEvents();

                // ★【固定步长时钟推进】每帧前进 8ms，与 renderTimer->start(8) 完美匹配
                // 无论 GPU 是否卡顿，时间推进永远是均匀的 0.008 秒
                // 0.2s 数据间隔 / 0.008s 步长 = 精确 25 帧插值，永不波动
                constexpr double FIXED_DT = 0.008; // 8ms = 125 FPS
                if (clock) clock->tickFixed(FIXED_DT);

                // 2. 动态模型逻辑更新 (The Heart of Simulation)
                // 在这里计算飞机的最新位置、插值、显隐切换
                {
                    vsg::FrameEvent fe;
                    simUpdateHandler->apply(fe);

                    editor->processPendingOperations(); // ← 消费放置队列

                    // 合并两个 needsCompile 判断，只调用一次 compile
                    bool needsRecompile = simUpdateHandler->needsCompile() || editor->needsCompile();
                    if (needsRecompile)
                    {
                        viewer->compile();
                        simUpdateHandler->clearCompileFlag();
                        editor->clearCompileFlag();
                    }

                    if (skyNode && clock)
                    {
                        skyNode->setDateTime(clock->currentDateTime());
                    }
                }

                // 3. 处理投影矩阵同步（防止窗口拉伸时地球变扁）
                if (vsgWindow->windowAdapter)
                {
                    VkExtent2D physicalExtent = vsgWindow->windowAdapter->extent2D();
                    if (physicalExtent.width > 0 && physicalExtent.height > 0)
                    {
                        double currentAspect = static_cast<double>(physicalExtent.width) / static_cast<double>(physicalExtent.height);
                        if (auto persp = camera->projectionMatrix.cast<vsg::Perspective>())
                        {
                            if (std::abs(persp->aspectRatio - currentAspect) > 1e-5)
                                persp->aspectRatio = currentAspect;
                        }
                    }
                }

                // (跟随清理逻辑已移除，由用户纯手动或后续再完善)

                // ★ 更新桥接器数据 (状态栏相机 & 性能)
                auto vp = mapManipulator->viewpoint();
                double fov = 30.0;
                double currentAspect = 1.0;
                if (auto persp = camera->projectionMatrix.cast<vsg::Perspective>())
                {
                    fov = persp->fieldOfViewY;
                    currentAspect = persp->aspectRatio;
                }
                double viewH = vp.range.has_value() ? vp.range->as(rocky::Units::METERS) : 0.0;
                double pitch = vp.pitch.has_value() ? vp.pitch.value().as(rocky::Units::DEGREES) : 0.0;
                double heading = vp.heading.has_value() ? vp.heading.value().as(rocky::Units::DEGREES) : 0.0;

                // 计算比例尺文本 (假定 UI 的比例尺条宽度为 36 像素)
                // 地面视宽 = 2 * 距离 * tan(fov/2) * aspect
                double frustumHeightAtGround = 2.0 * viewH * std::tan(vsg::radians(fov) * 0.5);
                double frustumWidthAtGround = frustumHeightAtGround * currentAspect;
                // 窗口实际宽度
                double windowWidth = static_cast<double>(vsgWindow->width());
                double metersPerPixel = (windowWidth > 0.0) ? (frustumWidthAtGround / windowWidth) : 0.0;
                double barMeters = metersPerPixel * 36.0; // 36 是 QML 里定义的比例尺物理宽度

                QString scaleText;
                if (barMeters >= 1000.0)
                    scaleText = QString::number(barMeters / 1000.0, 'f', 1) + " km";
                else
                    scaleText = QString::number(barMeters, 'f', 0) + " m";

                // ★ [修复经纬度大数值问题] 通过 rocky 地图操作器获取WGS84地理坐标
                rocky::GeoPoint camGeo;
                if (mapManipulator->viewpoint().point.srs.valid())
                {
                    camGeo = mapManipulator->viewpoint().point.transform(rocky::SRS::WGS84);
                }

                bridge->updateCamera(camGeo.x, camGeo.y, camGeo.z, fov, pitch, heading, viewH, scaleText);

                auto now = std::chrono::steady_clock::now();
                double ms = std::chrono::duration<double, std::milli>(now - lastFrameTime).count();
                lastFrameTime = now;
                int fps = ms > 0.0 ? static_cast<int>(1000.0 / ms) : 0;
                bridge->updatePerf(ms, fps);

                // ★ 固定8分钟时间轴：收到第一帧数据时设定，之后不再变动
                if (dataManager->acmiPacketCount() > 0 && !g_timelineInitialized)
                {
                    double acmiMin = dataManager->acmiTimeMin();
                    constexpr double FIXED_DURATION = 480.0; // 8分钟 = 480秒
                    rocky::DateTime acmiStart(2018, 9, 15, acmiMin / 3600.0);
                    rocky::DateTime acmiStop(2018, 9, 15, (acmiMin + FIXED_DURATION) / 3600.0);
                    clock->setTimeRange(acmiStart, acmiStop);
                    clock->setAnimating(true); // 收到数据后自动开始播放
                    g_timelineInitialized = true;
                    qDebug() << ">>> [Timeline] 固定8分钟时间轴已初始化, 起点=" << acmiMin << "s";
                }
                bridge->updateAcmiStats(dataManager->acmiPacketCount(), false);

                double totalSim = clock->totalSeconds();
                double bufferedProg = totalSim > 0.0 ? (dataManager->maxReceivedTime() - dataManager->acmiTimeMin()) / totalSim : 0.0;
                bridge->updateMaxBufferedProgress(bufferedProg);

                // ★ 防止红线超过白线（缓冲控制）
                if (clock->progress() > bufferedProg && bufferedProg > 0.0)
                {
                    // 如果红线跑到了白线前面，强行把它拉回来并暂停播放
                    clock->seekToProgress(bufferedProg);
                    if (clock->isAnimating())
                    {
                        clock->setAnimating(false);
                        qDebug() << ">>> [Timeline] 缓冲不足，暂停播放并等待数据...";
                        g_appState._isWaitingForBuffer = true;
                    }
                }
                else if (g_appState._isWaitingForBuffer && (bufferedProg - clock->progress() > 0.02)) // 留出 2% 的防抖空间再恢复
                {
                    // 数据进来了，白线重新超过了红线（加一点点防抖），自动恢复播放
                    clock->setAnimating(true);
                    g_appState._isWaitingForBuffer = false;
                    qDebug() << ">>> [Timeline] 缓冲恢复，自动继续播放";
                }

                viewer->update();

                // ★ 统一编译：检查是否有新节点（如 TrackNode）加入并需要编译
                if (simUpdateHandler->needsCompile() || editor->needsCompile())
                {
                    viewer->compile();
                    simUpdateHandler->clearCompileFlag();
                    editor->clearCompileFlag();
                }

                // TrackNode 直接在 apply() 里每帧更新，此处无需额外调用

                viewer->recordAndSubmit();
                viewer->present();
            }
            else
            {
                app.quit();
            }
        }
        catch (const vsg::Exception& ve)
        {
            qWarning() << "VSG 异常:" << ve.message.c_str();
        }
        catch (const std::exception& e)
        {
            qWarning() << "C++ 异常:" << e.what();
        }
        catch (...)
        {
            qWarning() << "发生未知渲染异常！";
        }
    });

    // [修复 Bug2] 同时处理 Hidden 和 Minimized 两种关闭/最小化状态
    QObject::connect(vsgWindow, &QWindow::visibilityChanged,
                     [&app, qmlView, syncTimer, renderTimer](QWindow::Visibility visibility) {
                         if (visibility == QWindow::Hidden)
                         {
                             qDebug() << "=== vsgWindow 已关闭，同步关闭 QML 并退出 ===";
                             if (syncTimer) syncTimer->stop();
                             if (renderTimer) renderTimer->stop();
                             if (qmlView)
                             {
                                 qmlView->close();
                                 qmlView->deleteLater();
                             }
                             app.quit();
                         }
                         else if (visibility == QWindow::Minimized)
                         {
                             // 主窗口最小化时同步隐藏 QML，防止悬空残留
                             if (qmlView) qmlView->hide();
                         }
                         else if (visibility == QWindow::Windowed || visibility == QWindow::FullScreen)
                         {
                             // 主窗口还原时重新显示 QML
                             if (qmlView) qmlView->show();
                         }
                     });

    QObject::connect(vsgWindow, &QWindow::windowStateChanged, [bridge](Qt::WindowState) {
        emit bridge->windowStateChanged();
    });

    syncTimer->setParent(vsgWindow);
    renderTimer->setParent(vsgWindow);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        qDebug() << "=== aboutToQuit：清理所有资源 ===";
        dataManager->stopUdpReceiver(); // 停止二进制 UDP 接收线程
        // _G::entityManager.reset();      // 销毁 ECS（注释掉，因从未初始化可能引发段错误）
        if (syncTimer) syncTimer->deleteLater();
        if (renderTimer) renderTimer->deleteLater();
        if (qmlView)
        {
            qmlView->close();
            qmlView->deleteLater();
        }
        if (viewer)
        {
            viewer->deviceWaitIdle();
        }
    });

    renderTimer->start(8); // 将渲染循环从 16ms (60FPS) 提升至 8ms (125FPS)，大幅度增加数据插帧采样密度
    return app.exec();
}
