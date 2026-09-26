cmake_minimum_required(VERSION 3.5)

project(GNXMapEngine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>")
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:/utf-8>"
                        "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
endif()

if(MSVC)
    add_compile_options(/wd4819 /wd4828)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

macro(SETUP_GROUPS src_files)
    foreach(FILE ${src_files})
        get_filename_component(PARENT_DIR "${FILE}" PATH)
        set(GROUP "${PARENT_DIR}")
        string(REPLACE "/" "\\" GROUP "${GROUP}")
        source_group("${GROUP}" FILES "${FILE}")
    endforeach()
endmacro()

set(PROJECT_SOURCES
    main.cpp
    MapApplication.cpp
    MapApplication.h
    MapRenderer.cpp
    MapRenderer.h
    ScreenshotUtil.cpp
    ScreenshotUtil.h
    WebMercator.cpp
    WebMercator.h
    earthCore/BoundingRegion.h
    earthCore/BoundingRegion.cpp
    earthCore/GlobeRectangle.h
    earthCore/GlobeRectangle.cpp
    earthCore/EarthEngineDefine.h
    earthCore/Ellipsoid.h
    earthCore/Ellipsoid.cpp
    earthCore/EllipsoidTangentPlane.h
    earthCore/EllipsoidTangentPlane.cpp
    earthCore/Geodetic3D.h
    earthCore/Geodetic3D.cpp
    earthCore/GeoGridTessellator.h
    earthCore/GeoGridTessellator.cpp
    earthCore/GeoMath.h
    earthCore/GeoMath.cpp
    earthCore/GeoTransform.h
    earthCore/GeoTransform.cpp
    earthCore/SubdivisionUtility.h
    earthCore/SubdivisionUtility.cpp
    earthCore/EarthNode.h
    earthCore/EarthNode.cpp
    earthCore/EarthCamera.h
    earthCore/EarthCamera.cpp
    earthCore/EarthCameraPose.h
    earthCore/EarthCameraPose.cpp
    earthCore/IntersectionTests.h
    earthCore/IntersectionTests.cpp
    earthCore/QuadTree.h
    earthCore/QuadTree.cpp
    earthCore/QuadTileID.h
    earthCore/QuadTileID.cpp
    earthCore/LayerBase.h
    earthCore/LayerBase.cpp
    earthCore/TileDataSource.h
    earthCore/TileDataSource.cpp
    earthCore/TileLoadTask.h
    earthCore/TileLoadTask.cpp
    earthCore/TileLoadState.h
    earthCore/EarthRenderer.h
    earthCore/EarthRenderer.cpp
    earthCore/DEMMeshData.h
    earthCore/DEMMeshData.cpp
)

add_executable(GNXMapEngine ${PROJECT_SOURCES})
SETUP_GROUPS("${PROJECT_SOURCES}")

add_subdirectory(../GNXEngine engine)
# imgui_headers 只提供头文件；ImGui 符号从 GNXEngine 共享库导入。
# 不要直接链接 imgui 静态库，否则进程内会出现第二份 GImGui 上下文。
target_link_libraries(GNXMapEngine PRIVATE GNXEngine imgui_headers)

set_target_properties(GNXMapEngine PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.gnxengine.map
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
)

install(TARGETS GNXMapEngine
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib)

# ==================== 测试 ====================
# 方位角/俯仰角数学的「无窗口」数值验证：
# 只编译不依赖渲染设备的少量源文件（EarthCamera.cpp 的构造需要渲染设备，故排除），
# 这样无需窗口/GPU 即可运行断言。
option(GNX_MAP_BUILD_TESTS "Build GNXMapEngine numerical tests" ON)

if(GNX_MAP_BUILD_TESTS)
    add_executable(TestEarthCameraAngles
        test/TestEarthCameraAngles.cpp
        earthCore/EarthCameraPose.h
        earthCore/EarthCameraPose.cpp
        earthCore/Ellipsoid.h
        earthCore/Ellipsoid.cpp
        earthCore/Geodetic3D.h
        earthCore/Geodetic3D.cpp
        earthCore/GeoTransform.h
        earthCore/GeoTransform.cpp
        earthCore/IntersectionTests.cpp
    )

    target_include_directories(TestEarthCameraAngles PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(TestEarthCameraAngles PRIVATE GNXEngine)

    set_property(TARGET TestEarthCameraAngles PROPERTY FOLDER "test")

    enable_testing()
    add_test(NAME EarthCameraAngles COMMAND TestEarthCameraAngles)
endif()
