cmake_minimum_required(VERSION 3.10)
project(vsgqtviewer)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── 1. 查找依赖 ────────────────────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Core Gui Quick Widgets QuickControls2 Network REQUIRED)
find_package(vsg CONFIG REQUIRED)
find_package(vsgXchange CONFIG REQUIRED)
find_package(rocky CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
# vsgQt 是当前项目的一部分，不需要 find_package

# ── 2. Include 路径 ────────────────────────────────────────────────────────────
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ── 3. 源文件定义（按实际目录分组）────────────────────────────────────────────

# C++ 源文件（全部在 src/）
set(CPP_SOURCES
    src/main.cpp
    src/QmlBridge.cpp
    src/ModelFactory.cpp
    src/ModelListModel.cpp
    src/InstanceManager.cpp
    src/SimDataManager.cpp
    src/SimClock.cpp
)

# 头文件（全部在 include/）
set(HEADER_FILES
    include/QmlBridge.h
    include/ModelFactory.h
    include/ModelListModel.h
    include/InstanceManager.h
    include/SimData.h
    include/SimDataManager.h
    include/SimClock.h
    include/AcmiUdpReceiver.h
    include/AcmiTelemetryForwarder.h
    include/StyleHelper.h
    include/TrackHistorySystem.h
    include/TrackNode.h
)

# QML 文件（全部在 qml/）
set(QML_FILES
    qml/TechOverlay.qml
    qml/SciFiButton.qml
    qml/ManagePanel.qml
    qml/ModelListPanel.qml
    qml/TimelineBar.qml
    qml/WinBtn.qml
    qml/WindowControls.qml
)

# 图片资源（全部在 img/）
set(IMG_FILES
    img/top_banner_bg.png
    img/btn_border.png
)

# Python 脚本（全部在 py/）—— 仅供 IDE 浏览，不参与编译
set(PY_FILES
    py/acmi_replay_sender.py
    py/mock_udp.py
)

# QRC 资源
set(QRC_FILES
    resources.qrc
)

# ── 4. 创建可执行文件 ──────────────────────────────────────────────────────────
add_executable(vsgqtviewer
    ${CPP_SOURCES}
    ${HEADER_FILES}
    ${QML_FILES}
    ${IMG_FILES}
    ${PY_FILES}
    ${QRC_FILES}
)

# ── 5. 在 IDE（VS / Qt Creator）中按类型显示虚拟文件夹 ──────────────────────
source_group("Source Files"  FILES ${CPP_SOURCES})
source_group("Header Files"  FILES ${HEADER_FILES})
source_group("QML Files"     FILES ${QML_FILES})
source_group("Image Files"   FILES ${IMG_FILES})
source_group("Python Files"  FILES ${PY_FILES})
source_group("Resources"     FILES ${QRC_FILES})

# ── 6. 链接库 ─────────────────────────────────────────────────────────────────
target_link_libraries(vsgqtviewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Widgets Qt6::QuickControls2 Qt6::Network
    vsg::vsg
    vsgXchange::vsgXchange
    rocky::rocky
    vsgQt
    imgui::imgui
)

# ── 7. 构建后自动同步运行时资源到 bin 目录 ─────────────────────────────────────
# QML 文件
add_custom_command(TARGET vsgqtviewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:vsgqtviewer>/qml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/TechOverlay.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/SciFiButton.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/ManagePanel.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/ModelListPanel.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/TimelineBar.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/WinBtn.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/WindowControls.qml"
        "$<TARGET_FILE_DIR:vsgqtviewer>/qml"
    COMMENT "Copying QML files to bin/qml ..."
)

# 图片文件
add_custom_command(TARGET vsgqtviewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:vsgqtviewer>/img"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/img/top_banner_bg.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/img/btn_border.png"
        "$<TARGET_FILE_DIR:vsgqtviewer>/img"
    COMMENT "Copying image assets to bin/img ..."
)


# data 目录（如有内容时同步）
add_custom_command(TARGET vsgqtviewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:vsgqtviewer>/data"
    COMMENT "Copying data directory to bin/data ..."
)

# ── 8. 同步 assets/ 内置资源到 bin（构建后即可直接运行，无需手动拷贝）────────

# assets/models → bin/models/  (3D 飞机模型 .glb)
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../assets")

add_custom_command(TARGET vsgqtviewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASSETS_DIR}/models"
        "$<TARGET_FILE_DIR:vsgqtviewer>/models"
    COMMENT "Copying 3D model assets to bin/models ..."
)

# assets/rocky → bin/share/rocky/  (Rocky 着色器 + 数据)
add_custom_command(TARGET vsgqtviewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASSETS_DIR}/rocky"
        "$<TARGET_FILE_DIR:vsgqtviewer>/share/rocky"
    COMMENT "Copying Rocky shaders/data to bin/share/rocky ..."
)

# assets/fonts → bin/fonts/  (VSG 字体文件)
add_custom_command(TARGET vsgqtviewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASSETS_DIR}/fonts"
        "$<TARGET_FILE_DIR:vsgqtviewer>/fonts"
    COMMENT "Copying font assets to bin/fonts ..."
)

