cmake_minimum_required(VERSION 3.5)

project(GNXMapEngine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        metalwindow.h
        metalwindow.mm
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(GNXMapEngine
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET GNXMapEngine APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(GNXMapEngine SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(GNXMapEngine
            ${PROJECT_SOURCES}
        )
    endif()
endif()

set_property(TARGET GNXMapEngine PROPERTY AUTOUIC ON)
set_property(TARGET GNXMapEngine PROPERTY AUTOMOC ON)
set_property(TARGET GNXMapEngine PROPERTY AUTORCC ON)

if(APPLE)
    target_link_libraries(GNXMapEngine PRIVATE "-framework CoreGraphics" "-framework CoreVideo" "-framework QuartzCore"
        "-framework GameController" "-framework Metal" "-framework ImageIO" "-framework OpenGL" "-framework AppKit")
    #target_compile_options(GNXMapEngine PRIVATE "-fobjc-arc")
    #set_target_properties(GNXMapEngine PROPERTIES LINK_FLAGS "-fobjc-arc")
endif()

target_link_libraries(GNXMapEngine PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    baselib imagecodec mathutil rendercore rendersystem shadercompiler spirv_reflect)

set_target_properties(GNXMapEngine PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

add_subdirectory(../GNXEngine/project/cmake engine)

install(TARGETS GNXMapEngine
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(GNXMapEngine)
endif()
