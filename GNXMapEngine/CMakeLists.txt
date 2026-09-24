cmake_minimum_required(VERSION 3.5)

project(GNXMapEngine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
target_link_libraries(GNXMapEngine PRIVATE GNXEngine)

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
