cmake_minimum_required(VERSION 3.20)
project(vajra_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Find Yosys configuration using yosys-config
# Users can provide -DYOSYS_CONFIG=/path/to/yosys-config or rely on PATH
if(NOT YOSYS_CONFIG)
    find_program(YOSYS_CONFIG yosys-config REQUIRED)
endif()

execute_process(
    COMMAND ${YOSYS_CONFIG} --cxxflags
    OUTPUT_VARIABLE YOSYS_CXXFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND ${YOSYS_CONFIG} --datdir
    OUTPUT_VARIABLE YOSYS_DATDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Found Yosys config: ${YOSYS_CONFIG}")
message(STATUS "Yosys CXXFLAGS: ${YOSYS_CXXFLAGS}")
message(STATUS "Yosys DATDIR: ${YOSYS_DATDIR}")

# Find Qt (support both Qt6 and Qt5)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Gui Core Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Gui Core Svg)
message(STATUS "Using Qt version: ${Qt${QT_VERSION_MAJOR}_VERSION}")

# Collect all plugin sources and headers (for AUTOMOC)
file(GLOB_RECURSE VAJRA_SOURCES
    src/pass/*.cpp
    src/core/*.cpp
    src/liberty/*.cpp
    src/gui/*.cpp
    include/*.hpp
)

add_library(vajra SHARED ${VAJRA_SOURCES})

set_target_properties(vajra PROPERTIES
    PREFIX ""
    OUTPUT_NAME "vajra"
    SUFFIX ".so"
    COMPILE_FLAGS "${YOSYS_CXXFLAGS}"
)

target_include_directories(vajra PRIVATE
    include
    src
    ${YOSYS_DATDIR}/include
)

target_link_libraries(vajra PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Svg
)

# Installation destination: defaults to Yosys system plugins directory,
# or can be set to custom path (e.g. -DPLUGIN_INSTALL_DIR=~/.local/share/yosys/plugins)
if(NOT DEFINED PLUGIN_INSTALL_DIR)
    set(PLUGIN_INSTALL_DIR "${YOSYS_DATDIR}/plugins")
endif()

install(TARGETS vajra DESTINATION "${PLUGIN_INSTALL_DIR}")
