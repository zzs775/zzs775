#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QDebug>
#include <QDir>
#include <QProcess>

#include <vsg/all.h>
#include <vsgQt/Window.h>

#include <rocky/vsg/MapNode.h>
#include <rocky/vsg/MapManipulator.h>
#include <rocky/vsg/VSGContext.h>
#include <rocky/TMSImageLayer.h>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    QString binDir = QCoreApplication::applicationDirPath();
    qputenv("VSG_FILE_PATH", binDir.toLocal8Bit());

    QString rockySharePath = "C:/Users/cfh12/Desktop/rocky_qt/rocky-main (rebulid)/install/share";
    qputenv("ROCKY_FILE_PATH", rockySharePath.toLocal8Bit());

    auto viewer = vsgQt::Viewer::create();
    auto rockyContext = rocky::VSGContextFactory::create(viewer);

    auto mapNode = rocky::MapNode::create(rockyContext);

    auto layer = rocky::TMSImageLayer::create();
    layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    mapNode->map->add(layer);

    auto vsg_scene = vsg::Group::create();
    vsg_scene->addChild(mapNode);

    auto traits = vsg::WindowTraits::create();
    traits->windowTitle = "Rocky Qt Viewer";
    traits->debugLayer = true;
    traits->width = 1200;
    traits->height = 800;

    auto window = new vsgQt::Window(viewer, traits);
    window->initializeWindow();

    if (!traits->device) traits->device = window->windowAdapter->getOrCreateDevice();

    double radius = 6378137.0;
    auto lookAt = vsg::LookAt::create(vsg::dvec3(0, -radius*3.0, 0), vsg::dvec3(0,0,0), vsg::dvec3(0,0,1));
    auto perspective = vsg::Perspective::create(30.0, 1200.0/800.0, 0.1, radius * 10.0);
    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(VkExtent2D{1200, 800}));

    auto manipulator = rocky::MapManipulator::create(mapNode, window->windowAdapter, camera, rockyContext);
    viewer->addEventHandler(manipulator);

    auto commandGraph = vsg::createCommandGraphForView(*window, camera, vsg_scene);

    for (auto& child : commandGraph->children)
    {
        if (auto renderGraph = child.cast<vsg::RenderGraph>())
        {
            if (!renderGraph->clearValues.empty())
            {
                renderGraph->clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            }
        }
    }

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    QMainWindow* mainWindow = new QMainWindow();
    auto widget = QWidget::createWindowContainer(window, mainWindow);
    mainWindow->setCentralWidget(widget);
    mainWindow->resize(1200, 800);
    mainWindow->setWindowTitle("Rocky Qt Viewer");
    mainWindow->show();

    viewer->setInterval(0);
    viewer->continuousUpdate = true;
    viewer->compile();

    return application.exec();
}
