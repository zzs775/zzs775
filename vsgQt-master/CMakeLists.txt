cmake_minimum_required(VERSION 3.10)

project(vsgQt
    VERSION 0.5.0
    DESCRIPTION "Qt integration with VulkanSceneGraph"
    LANGUAGES CXX
)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VSGQT_BUILD_EXAMPLES "Build examples" ON)

set(VSGQT_SOVERSION 3)
set(VSGQT_RELEASE_CANDIDATE 0)

set(VSGQT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "Root source directory of vsgQt")
set(VSGQT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "Root binary directory of vsgQt")

set(QT_PACKAGE_NAME Qt6 CACHE STRING "Set Qt package name, i.e. Qt5 or Qt6.")

# 设置 Rocky 和 VSG 的路径
list(APPEND CMAKE_PREFIX_PATH "C:/Users/cfh12/Desktop/rocky_qt/rocky-main (1)/install")
list(APPEND CMAKE_PREFIX_PATH "C:/Users/cfh12/Desktop/rocky_qt/rocky-main (1)/build/vcpkg_installed/x64-windows/share")

find_package(vsg 1.1.13)

find_package(rocky REQUIRED)

vsg_setup_dir_vars()
vsg_setup_build_vars()

# if Qt5 then we need 5.10 or later
find_package(${QT_PACKAGE_NAME} COMPONENTS Widgets REQUIRED)

vsg_add_target_clang_format(
    FILES
        include/*/*.h
        src/*/*.cpp
)
vsg_add_target_clobber()
vsg_add_target_cppcheck(
    FILES
        include/*/*.h
        src/*/*.cpp
)
vsg_add_target_docs(
    FILES
        src
        include/*/*.h
        src/*/*.cpp
)
vsg_add_target_uninstall()

# only provide custom targets if not building as a submodule/FetchContent
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

    vsg_add_option_maintainer(
        PREFIX v
        RCLEVEL ${VSGQT_RELEASE_CANDIDATE}
    )

endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32)
    add_definitions(-DNOMINMAX)
    set(MODE WIN32)
    # 添加 MSVC 特定选项，解决 Qt 6.10.1 的编译问题
    if(MSVC)
        add_compile_options(/Zc:__cplusplus)
        add_compile_options(/permissive-)
        # 禁用一些可能导致问题的警告
        add_compile_options(/wd4251 /wd4275 /wd4996)
    endif()
endif()

add_subdirectory(src/vsgQt)
if (VSGQT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

vsg_add_feature_summary()
