cmake_minimum_required(VERSION 3.20)
project(vajra_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Find Yosys configuration using yosys-config
find_program(YOSYS_CONFIG yosys-config HINTS /eda/yosys/bin /usr/local/bin /usr/bin REQUIRED)
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

# Find Qt5 dependencies
find_package(Qt5 COMPONENTS Widgets Gui Core Svg REQUIRED)

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
    /eda/yosys
    /eda/yosys/share/yosys/include
)

target_link_libraries(vajra PRIVATE
    Qt5::Widgets
    Qt5::Gui
    Qt5::Core
    Qt5::Svg
)

install(TARGETS vajra DESTINATION "${YOSYS_DATDIR}/plugins")
