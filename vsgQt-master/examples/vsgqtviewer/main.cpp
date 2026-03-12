#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QTimer>
#include <QDir>
#include <QDebug>
#include <QQuickWidget>
#include <QQuickItem>
#include <QQmlContext>
#include <cmath>
#include <vsg/all.h>
#include <vsg/state/material.h>
#include <vsgQt/Window.h>
#include <vsgXchange/all.h>
#include <rocky/vsg/MapNode.h>
#include <rocky/vsg/MapManipulator.h>
#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/GeoTransform.h>
#include <rocky/TMSImageLayer.h>

struct AppState {
    QString pendingModelType = "";
    bool isPlacingMode = false;
};
static AppState g_appState;


vsg::dvec3 manual_XYZ_to_LatLon(const vsg::dvec3& ecef) {
    double x = ecef.x;
    double y = ecef.y;
    double z = ecef.z;

    double lon = std::atan2(y, x);
    double hyp = std::sqrt(x * x + y * y);
    double lat = std::atan2(z, hyp);
    double alt = std::sqrt(x * x + y * y + z * z) - 6378137.0;

    return vsg::dvec3(vsg::degrees(lat), vsg::degrees(lon), alt);
}


vsg::dmat4 computeLocalToWorldTransform(const vsg::dvec3& ecefPos) {
    vsg::dvec3 up = vsg::normalize(ecefPos);
    vsg::dvec3 north = vsg::dvec3(0,0,1);
    if (std::abs(vsg::dot(up, north)) > 0.99) north = vsg::dvec3(0,1,0);
    double dot = vsg::dot(north, up);
    north = vsg::normalize(north - up * dot);
    vsg::dvec3 east = vsg::cross(north, up);

    return vsg::dmat4(
        east.x,  east.y,  east.z,  0.0,
        north.x, north.y, north.z, 0.0,
        up.x,    up.y,    up.z,    0.0,
        ecefPos.x, ecefPos.y, ecefPos.z, 1.0
        );
}

class FixMaterialVisitor : public vsg::Visitor {
public:
    using vsg::Visitor::apply;

    void apply(vsg::Object& object) override { object.traverse(*this); }

    void apply(vsg::StateGroup& sg) override {
        for (auto& command : sg.stateCommands) {
            if (auto blend = command.cast<vsg::ColorBlendState>()) {
                blend->attachments[0].blendEnable = VK_FALSE;
            }
            if (auto depth = command.cast<vsg::DepthStencilState>()) {
                depth->depthTestEnable = VK_TRUE;
                depth->depthWriteEnable = VK_TRUE;
            }
            if (auto mat = command.cast<vsg::PbrMaterialValue>()) {
                mat->value().alphaMask = 1.0f;
                mat->value().alphaMaskCutoff = 0.5f;
            }
        }
        sg.traverse(*this);
    }
};


class EditorEventHandler : public vsg::Inherit<vsg::Visitor, EditorEventHandler>
{
public:
    EditorEventHandler(vsg::ref_ptr<rocky::MapNode> mapNode,
                       vsg::ref_ptr<vsg::Group> entityGroup,
                       vsg::ref_ptr<vsg::Camera> camera,
                       vsg::ref_ptr<vsg::Viewer> viewer)
        : _mapNode(mapNode), _entityGroup(entityGroup), _camera(camera), _viewer(viewer)
    {
        initModelCache();
    }

    void apply(vsg::ButtonPressEvent& pressEvent) override {
        if (pressEvent.button != 1) return;
        if (!g_appState.isPlacingMode) return;
        if (!_camera || !_mapNode) return;

        auto intersector = vsg::LineSegmentIntersector::create(*_camera, pressEvent.x, pressEvent.y);
        _mapNode->accept(*intersector);

        if (intersector->intersections.empty()) return;

        vsg::dvec3 clickPos = intersector->intersections.front()->worldIntersection;

        placePlaneAt(clickPos);
    }

private:
    vsg::ref_ptr<vsg::Node> _cachedModelNode;

    vsg::dmat4 computeLocalToWorldTransform(const vsg::dvec3& ecefPos) {
        vsg::dvec3 up = vsg::normalize(ecefPos);
        vsg::dvec3 north = vsg::dvec3(0, 0, 1);
        if (std::abs(vsg::dot(up, north)) > 0.99) north = vsg::dvec3(0, 1, 0);

        double dot = vsg::dot(north, up);
        north = vsg::normalize(north - up * dot);
        vsg::dvec3 east = vsg::cross(north, up);

        return vsg::dmat4(
            east.x,  east.y,  east.z,  0.0,
            north.x, north.y, north.z, 0.0,
            up.x,    up.y,    up.z,    0.0,
            ecefPos.x, ecefPos.y, ecefPos.z, 1.0
        );
    }

    void placePlaneAt(vsg::dvec3 position) {
        if (!_cachedModelNode) return;

        vsg::dmat4 locationMatrix = computeLocalToWorldTransform(position);

        double scale = 50000.0;

        vsg::dmat4 liftMatrix = vsg::translate(0.0, 0.0, scale * 1.5);

        vsg::dmat4 rotateMatrix = vsg::rotate(vsg::radians(0.0), 1.0, 0.0, 0.0);

        vsg::dmat4 scaleMatrix = vsg::scale(scale, scale, scale);

        vsg::dmat4 finalMatrix = locationMatrix * liftMatrix * rotateMatrix * scaleMatrix;

        auto transform = vsg::MatrixTransform::create();
        transform->matrix = finalMatrix;
        transform->addChild(_cachedModelNode);

        _entityGroup->addChild(transform);
        _viewer->compile();

        qDebug() << "模型已放置 ";
    }

    void initModelCache() {
        std::string filename = "F-16A.glb";
        std::string fullPath = "C:/Users/cfh12/Desktop/rocky_qt/sim.vsg-master/sim.vsg/data/3DModel/" + filename;

        auto options = vsg::Options::create();
        options->add(vsgXchange::all::create());
        auto model = vsg::read_cast<vsg::Node>(fullPath, options);

        if (!model) {
            qDebug() << "模型加载失败，使用红盒子代替";
            auto builder = vsg::Builder::create();
            vsg::GeometryInfo info; info.color.set(1,0,0,1);
            model = builder->createBox(info);
        } else {
            FixMaterialVisitor cleaner;
            model->accept(cleaner);

            auto computeBounds = vsg::visit<vsg::ComputeBounds>(model);
            vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;

            double height = computeBounds.bounds.max.y - computeBounds.bounds.min.y;

            auto centerTrans = vsg::MatrixTransform::create();
            centerTrans->matrix = vsg::translate(-center.x, -center.y, -center.z);
            centerTrans->addChild(model);
            model = centerTrans;
        }

        auto stateGroup = vsg::StateGroup::create();
        auto depthStencil = vsg::DepthStencilState::create();
        depthStencil->depthTestEnable = VK_TRUE;
        depthStencil->depthWriteEnable = VK_TRUE;
        depthStencil->depthCompareOp = VK_COMPARE_OP_LESS;

        stateGroup->add(depthStencil);
        stateGroup->addChild(model);

        _cachedModelNode = stateGroup;
    }

    vsg::ref_ptr<rocky::MapNode> _mapNode;
    vsg::ref_ptr<vsg::Group> _entityGroup;
    vsg::ref_ptr<vsg::Camera> _camera;
    vsg::ref_ptr<vsg::Viewer> _viewer;
};

int main(int argc, char *argv[]) {
    system("chcp 65001");
    QApplication app(argc, argv);

    auto viewer = vsgQt::Viewer::create();

    QString binDir = QCoreApplication::applicationDirPath();
    qputenv("VSG_FILE_PATH", binDir.toLocal8Bit());
    QString rockySharePath = "C:/Users/cfh12/Desktop/rocky_qt/rocky-main (1)/install/share";
    if (!QDir(rockySharePath).exists()) {
        qDebug() << "路径错误：" << rockySharePath;
        return -1;
    }
    qputenv("ROCKY_FILE_PATH", rockySharePath.toLocal8Bit());

    auto rockyContext = rocky::VSGContextFactory::create(viewer);
    auto mapNode = rocky::MapNode::create(rockyContext);
    auto entityGroup = vsg::Group::create();

    auto layer = rocky::TMSImageLayer::create();
    layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    mapNode->map->add(layer);

    auto vsg_scene = vsg::Group::create();
    vsg_scene->addChild(mapNode);
    vsg_scene->addChild(entityGroup);
    vsg_scene->addChild(vsg::createHeadlight());

    auto traits = vsg::WindowTraits::create();
    traits->windowTitle = "Rocky HUD";
    traits->width = 1200; traits->height = 800;

    auto vsgWindow = new vsgQt::Window(viewer, traits);
    vsgWindow->initializeWindow();

    auto camera = vsg::Camera::create(
        vsg::Perspective::create(30.0, 1.5, 100.0, 6378137.0 * 10.0),
        vsg::LookAt::create(vsg::dvec3(0, -6378137.0*3.0, 0), vsg::dvec3(0,0,0), vsg::dvec3(0,0,1)),
        vsg::ViewportState::create(VkExtent2D{1200, 800})
        );

    auto editor = EditorEventHandler::create(mapNode, entityGroup, camera, viewer);
    viewer->addEventHandler(editor);
    viewer->addEventHandler(rocky::MapManipulator::create(mapNode, vsgWindow->windowAdapter, camera, rockyContext));

    auto commandGraph = vsg::createCommandGraphForView(*vsgWindow, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    QMainWindow mainWindow;
    QWidget* centralWidget = new QWidget(&mainWindow);
    mainWindow.setCentralWidget(centralWidget);

    QWidget* vsgContainer = QWidget::createWindowContainer(vsgWindow);
    vsgContainer->setParent(centralWidget);

    QQuickWidget* qmlOverlay = new QQuickWidget(centralWidget);
    qmlOverlay->setSource(QUrl("qrc:/TechOverlay.qml"));
    qmlOverlay->setResizeMode(QQuickWidget::SizeRootObjectToView);
    qmlOverlay->setClearColor(Qt::transparent);
    qmlOverlay->setAttribute(Qt::WA_TranslucentBackground);
    qmlOverlay->setAttribute(Qt::WA_AlwaysStackOnTop);

    QTimer* layoutTimer = new QTimer();
    QObject::connect(layoutTimer, &QTimer::timeout, [=](){
        if(centralWidget) {
            QSize s = centralWidget->size();
            vsgContainer->setGeometry(0, 0, s.width(), s.height());
            qmlOverlay->setGeometry(0, 0, s.width(), s.height());
            qmlOverlay->raise();
        }
    });
    layoutTimer->start(20);

    // // 修复版 connect：加入了 &app 作为第三个参数
    // if (qmlOverlay->rootObject()) {
    //     QObject::connect(
    //         qmlOverlay->rootObject(),           // 发送者
    //         SIGNAL(requestAddModel(QString)),   // 信号
    //         &app,                               // 🌟【关键修改】这里必须加 &app
    //         [&](const QString& type){           // Lambda 槽
    //             if (type == "模型列表") return;
    //             g_appState.pendingModelType = type;
    //             g_appState.isPlacingMode = true;
    //             QApplication::setOverrideCursor(Qt::CrossCursor);
    //             qDebug() << ">>> [QML] 请求部署: " << type;
    //         }
    //         );
    // }

    mainWindow.resize(1200, 800);
    mainWindow.show();
    viewer->compile();

    QTimer* renderTimer = new QTimer();
    QObject::connect(renderTimer, &QTimer::timeout, [&](){
        try {
            viewer->advanceToNextFrame();
            if (viewer->active()) {
                viewer->handleEvents();
                viewer->update();
                viewer->recordAndSubmit();
                viewer->present();
            }
        } catch (...) {
            qDebug() << "渲染循环异常";
        }
    });
    renderTimer->start(0);

    return app.exec();
}
