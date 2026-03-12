cmake_minimum_required(VERSION 3.10)
project(vsgqtviewer)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(vsg REQUIRED)
find_package(vsgXchange REQUIRED)
find_package(Qt6 COMPONENTS Widgets Core Gui Quick Qml QuickWidgets REQUIRED)
find_package(rocky REQUIRED)

add_executable(vsgqtviewer
    main.cpp
)



target_link_libraries(vsgqtviewer PRIVATE
    vsg::vsg
    vsgQt
    rocky::rocky
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickWidgets
    vsgXchange::vsgXchange
)


