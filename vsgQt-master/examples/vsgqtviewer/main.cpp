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

#include <algorithm>
#include <optional>
#include <rocky/GeoPoint.h>
#include <rocky/TMSImageLayer.h>
#include <rocky/vsg/MapManipulator.h>
#include <rocky/vsg/MapNode.h>
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

// --- ECS 驱动的帧更新器 (Step 5) ---
// 它是整个仿真的核心，每一帧都会被调用，负责根据最新数据更新所有飞机/导弹的位置和状态
class SimulationUpdateHandler : public vsg::Inherit<vsg::Visitor, SimulationUpdateHandler>
{
public:
    SimulationUpdateHandler(SimDataManager* dm, SimClock* clk = nullptr, vsg::ref_ptr<vsg::Group> entitiesRoot = nullptr, rocky::VSGContext context = {}) : dataManager(dm), simClock(clk), _entitiesRoot(entitiesRoot), rockyContext(context) {}

    // 核心函数：每一帧渲染前执行，处理所有逻辑更新
    void apply(vsg::FrameEvent& frame) override
    {
        _frameCount++;

        if (!dataManager || !_entitiesRoot) return;

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

        static std::unordered_map<std::string, EntityState> _entityStates;

        // (播放速度暂不参与 EMA 计算，统一用固定 smoothingK)

        // 【第一阶段：数据获取】
        std::vector<InterpolatedPacket> latestData;

        if (dataManager->acmiPacketCount() > 0 && simClock)
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
            else if (std::abs(currentTime - state.lastDataReceivedTime) > 2.0 && !state.isFadingOut)
            {
                // 超时2秒没有数据，标记完全淡出并移除
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

                double modelScale = 10000.0;
                std::string n = name;
                std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                if (n.find("aim") != std::string::npos || n.find("missile") != std::string::npos)
                {
                    modelScale = 5000.0;
                }

                auto modelNode = ModelFactory::instance()->createVisualEntity(
                    modelFile,
                    QString::fromStdString(name),
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

            if (!state.hasSmoothPos)
            {
                // 首帧直接赋值，避免从 (0,0,0) 慢慢追过来
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
        }
    }

    SimDataManager* dataManager = nullptr;
    SimClock* simClock = nullptr;
    vsg::ref_ptr<vsg::Group> _entitiesRoot = nullptr;
    rocky::VSGContext rockyContext;

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
        qDebug() << ">>> 模型删除请求已加入队列, ID:" << id;
    }

    // 清空所有实体节点
    void removeAllEntityNodes()
    {
        _entities->children.clear();
        _pendingRemovals.clear();
        _needsCompile = true;
        qDebug() << ">>> 已清空所有场景模型";
    }

    void processPendingOperations()
    {
        for (const auto& placement : _pendingPlacements)
        {
            double modelSize = 10000.0;
            std::string n = placement.name.toStdString();
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n.find("aim") != std::string::npos || n.find("r27") != std::string::npos || n.find("r73") != std::string::npos || n.find("missile") != std::string::npos)
            {
                modelSize = 5000.0; // 导弹比飞机小一倍
            }
            auto factory = ModelFactory::instance();
            auto visual = factory->createVisualEntity(placement.modelFile, placement.name, modelSize);
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

            qDebug() << ">>> 模型已添加到场景, ID:" << placement.entityId;
        }
        _pendingPlacements.clear();

        // 处理待删除的节点
        for (const auto& id : _pendingRemovals)
        {
            // This is a simplified removal. In a real app, you'd map entity IDs to VSG nodes.
            // For now, we assume _entities is cleared by removeAllEntityNodes() or individual removal is not critical.
            // A proper implementation would involve storing a map from entity ID to vsg::Node.
            qDebug() << ">>> 尝试移除模型, ID:" << id << " (当前简化实现不直接支持按ID移除)";
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

int main(int argc, char* argv[])
{
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    // system("chcp 65001"); // Qt Creator 环境下调用 system 可能导致静默退出，暂时注释

    QApplication app(argc, argv);
    QString binDir = QCoreApplication::applicationDirPath();
    qputenv("VSG_FILE_PATH", binDir.toLocal8Bit());

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

    auto rockyContext = rocky::VSGContextFactory::create(viewer);
    auto mapNode = rocky::MapNode::create(rockyContext);

    if (mapNode->map)
    {
        auto layer = rocky::TMSImageLayer::create();
        layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
        mapNode->map->add(layer);
    }

    auto entities = vsg::Group::create();
    auto scene = vsg::Group::create();
    scene->addChild(mapNode);
    scene->addChild(entities);
    scene->addChild(vsg::createHeadlight());

    auto fixedLight = vsg::DirectionalLight::create();
    fixedLight->direction = vsg::normalize(vsg::dvec3(0.2, 0.5, -1.0));
    fixedLight->color = vsg::vec3(1.0f, 1.0f, 1.0f);
    fixedLight->intensity = 2.8f;
    scene->addChild(fixedLight);

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

    // ★ 初始化 ECS 全局实体管理器 (Step 3) - Old code removed

    // ★ 创建帧级更新器 (Step 5)
    auto simUpdateHandler = SimulationUpdateHandler::create(dataManager, nullptr, entities, rockyContext);

    // ECS 初始化已移除（改用纯 VSG TrackNode，不需要 registry）

    ModelListModel* modelListModel = new ModelListModel(&app);
    QString modelDir = "C:/Users/cfh12/Desktop/rocky_qt/sim.vsg-master/sim.vsg/data/3DModel";
    modelListModel->loadFromDirectory(modelDir);
    qDebug() << "=== 模型目录加载完成，共" << modelListModel->rowCount() << "个模型";

    auto mapManipulator = rocky::MapManipulator::create(mapNode, vsgWindow->windowAdapter, camera, rockyContext);

    auto editor = EditorEventHandler::create(mapNode, entities, camera, viewer, dataManager, vsgWindow, mapManipulator);

    viewer->addEventHandler(editor);
    viewer->addEventHandler(mapManipulator);

    // 连接已移至后面 bridge 实例化之后

    auto commandGraph = vsg::createCommandGraphForView(*vsgWindow, camera, scene);
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
    qmlView->setTransientParent(vsgWindow);                // 建立父子关系，主窗口关闭时联动

    QmlBridge* bridge = new QmlBridge(&app);
    bridge->setBackgroundWindow(vsgWindow);
    qmlView->rootContext()->setContextProperty("ControlBridge", bridge);
    qmlView->rootContext()->setContextProperty("simModel", dataManager);
    qmlView->rootContext()->setContextProperty("modelListModel", modelListModel);
    qmlView->setSource(QUrl::fromLocalFile(binDir + "/TechOverlay.qml"));

    auto* clock = new SimClock(&app);
    clock->setTimeRange(rocky::DateTime(2018, 9, 15, 0.0), rocky::DateTime(2018, 9, 16, 0.0));
    bridge->setSimClock(clock);
    // skyNode->setDateTime(...) 等以后加了 SkyNode 再接
    clock->setAnimating(true);

    // ★ 将 SimClock 传递给帧级更新器，用于 ACMI 缓冲回放时间控制
    simUpdateHandler->simClock = clock;

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
            vp.range = rocky::Distance(5000.0, rocky::Units::METERS); // 初始距离 5km
            vp.pitch = rocky::Angle(-30.0, rocky::Units::DEGREES);    // 俯视 30 度
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
    QObject::connect(renderTimer, &QTimer::timeout, [vsgWindow, viewer, camera, scene, &app, editor, simUpdateHandler, mapManipulator, bridge, lastFrameTime, dataManager, clock, rockyContext]() mutable {
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

    renderTimer->start(16);
    return app.exec();
}
