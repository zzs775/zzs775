cmake_minimum_required(VERSION 3.10)
project(vsgqtviewer)

find_package(vsg REQUIRED)
find_package(Qt6 COMPONENTS Widgets Core REQUIRED)
find_package(rocky QUIET)

add_executable(vsgqtviewer main.cpp)


set(ROCKY_INCLUDE_DIR "C:/Users/cfh12/Desktop/rocky_qt/rocky-main (rebulid)/install/include")


target_include_directories(vsgqtviewer PRIVATE ${ROCKY_INCLUDE_DIR})

target_link_directories(vsgqtviewer PRIVATE
    "C:/Users/cfh12/Desktop/rocky_qt/rocky-main (rebulid)/install/lib"
    "C:/Users/cfh12/Desktop/rocky_qt/rocky-main (rebulid)/build/vcpkg_installed/x64-windows/lib"
)


target_link_libraries(vsgqtviewer PRIVATE
    vsg::vsg
    vsgQt
    rocky  # 链接 rocky.lib
    Qt6::Widgets
    Qt6::Core
)


