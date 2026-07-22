cmake_minimum_required(VERSION 3.19)
project(SeltInvertNodePlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Build against the same Qt / MinGW / OpenCV ABI as Selt_Gui.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui)
find_package(OpenCV REQUIRED COMPONENTS core imgproc)

# Point SELT_GUI_ROOT to the Selt_Gui source tree when configuring this project standalone.
if(NOT DEFINED SELT_GUI_ROOT)
    get_filename_component(SELT_GUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

add_library(selt_invert_node SHARED
    invertnodeplugin.cpp
    invertnodeplugin.h
)

target_include_directories(selt_invert_node PRIVATE
    ${SELT_GUI_ROOT}
    ${OpenCV_INCLUDE_DIRS}
)
target_compile_definitions(selt_invert_node PRIVATE SELT_HAS_OPENCV=1)
target_link_libraries(selt_invert_node PRIVATE Qt::Core Qt::Gui ${OpenCV_LIBS})

# Install/copy next to the host: <app>/bin/plugins/nodes/ (matches Selt_Gui host layout)
set_target_properties(selt_invert_node PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/nodes"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/nodes"
)

message(STATUS "Selt invert example plugin: output -> bin/plugins/nodes/")
