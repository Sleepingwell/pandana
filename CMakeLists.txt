cmake_minimum_required(VERSION 3.16)
project(pandanna LANGUAGES CXX)
include(FetchContent)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Python COMPONENTS Interpreter Development)
add_library(pandana SHARED
    src/accessibility.cpp
    src/cyaccess.pyx
    src/graphalg.cpp
    src/route_state.cpp
    src/transit_extensions.cpp
    src/contraction_hierarchies/src/libch.cpp)
target_include_directories(pandana PRIVATE src)
target_compile_options(pandana PRIVATE -fopenmp)
target_link_libraries(pandana gomp)
