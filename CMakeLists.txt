cmake_minimum_required(VERSION 3.22)
project(photo_viewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

find_package(exiv2 CONFIG QUIET)
if(TARGET exiv2lib)
    set(EXIV2_TARGET exiv2lib)
elseif(TARGET Exiv2::exiv2)
    set(EXIV2_TARGET Exiv2::exiv2)
else()
    find_package(Exiv2 REQUIRED)
    if(TARGET Exiv2::exiv2lib)
        set(EXIV2_TARGET Exiv2::exiv2lib)
    elseif(TARGET Exiv2::exiv2)
        set(EXIV2_TARGET Exiv2::exiv2)
    else()
        message(FATAL_ERROR "Could not determine Exiv2 target name")
    endif()
endif()

qt_standard_project_setup()

add_executable(photo-viewer
    src/main.cpp
    src/MainWindow.cpp
    src/ImageModel.cpp
    src/ImageViewerWidget.cpp
    src/ExifDialog.cpp
)

target_include_directories(photo-viewer PRIVATE src)

target_link_libraries(photo-viewer
    PRIVATE
        Qt6::Widgets
        Qt6::Concurrent
    ${EXIV2_TARGET}
)

install(TARGETS photo-viewer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/photo-viewer.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/photo-viewer.desktop
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/photo-viewer.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    @ONLY
)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)
