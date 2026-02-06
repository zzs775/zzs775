cmake_minimum_required(VERSION 3.10)
project(vsgqtviewer)

find_package(vsg REQUIRED)
find_package(Qt6 COMPONENTS Widgets Core REQUIRED)
find_package(rocky REQUIRED)

add_executable(vsgqtviewer main.cpp)


target_link_libraries(vsgqtviewer PRIVATE
    vsg::vsg
    vsgQt
    rocky::rocky
    Qt6::Widgets
    Qt6::Core
)


