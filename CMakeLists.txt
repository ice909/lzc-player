cmake_minimum_required(VERSION 3.16)

project(lzc-player VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick OpenGL)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(lzc-player
    main.cpp
    mpvobject.cpp
    mpvobject.h
)

set_target_properties(lzc-player PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)

qt_add_resources(lzc-player qml_resources
    PREFIX "/lzc-player"
    FILES
        Main.qml
        assets/play.svg
        assets/pause.svg
        assets/seek-backward.svg
        assets/seek-forward.svg
        assets/subtitle.svg
        assets/subtitle-hover.svg
        assets/volume.svg
        assets/volume-small.svg
        assets/volume-mute.svg
        assets/fullscreen.svg
        assets/quit-fullscreen.svg
)

target_include_directories(lzc-player PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(lzc-player
    PRIVATE
        LZC_PLAYER_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(lzc-player
    PRIVATE
        PkgConfig::MPV
        Qt6::Core
        Qt6::Gui
        Qt6::OpenGL
        Qt6::Quick
)

set_target_properties(lzc-player PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS lzc-player
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
