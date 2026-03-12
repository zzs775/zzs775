cmake_minimum_required(VERSION 3.10)
project(vsgqtviewer)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 1. 查找依赖
find_package(Qt6 COMPONENTS Core Gui Quick Widgets QuickControls2 REQUIRED)
find_package(vsg CONFIG REQUIRED)
find_package(vsgXchange CONFIG REQUIRED)
find_package(rocky CONFIG REQUIRED)
# vsgQt 是当前项目的一部分，不需要 find_package

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 🌟 2. 定义源文件列表 (关键修改！)
# 把所有的 .cpp, .h 和 .qml 都列在这里
set(SOURCES
    main.cpp
    QmlBridge.cpp
    QmlBridge.h
    ModelFactory.cpp
    ModelFactory.h
    ModelListModel.cpp
    ModelListModel.h
    InstanceManager.cpp
    InstanceManager.h
    SimData.h
    SimDataManager.cpp
    SimDataManager.h
    SimClock.h
    SimClock.cpp
    AcmiUdpReceiver.h
    StyleHelper.h
    TechOverlay.qml
    SciFiButton.qml
    ManagePanel.qml
    ModelListPanel.qml
    TimelineBar.qml
    resources.qrc
)

# 3. 创建可执行文件
add_executable(vsgqtviewer ${SOURCES})

# ✨ 按文件类型分组，在 Visual Studio 解决方案资源管理器中展示虚拟文件夹
source_group("Header Files" REGULAR_EXPRESSION "\\.h$")
source_group("Source Files" REGULAR_EXPRESSION "\\.cpp$")
source_group("QML Files"    REGULAR_EXPRESSION "\\.qml$")

# 4. 链接库
target_link_libraries(vsgqtviewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Widgets Qt6::QuickControls2
    vsg::vsg
    vsgXchange::vsgXchange
    rocky::rocky
    vsgQt
)

# 5. 每次构建后自动复制 QML 和图片到 bin 目录（POST_BUILD 保证每次 Build 都同步）
add_custom_command(TARGET vsgqtviewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/TechOverlay.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/SciFiButton.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/ManagePanel.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/ModelListPanel.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/TimelineBar.qml"
        "${CMAKE_BINARY_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/top_banner_bg.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/btn_border.png"
        "${CMAKE_BINARY_DIR}/bin"
    COMMENT "Copying QML and image assets to bin directory..."
)
