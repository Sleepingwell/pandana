cmake_minimum_required(VERSION 3.16)
project(pandanna LANGUAGES CXX)
include(FetchContent)
include(CTest)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(pandana PRIVATE src)

find_package(Python COMPONENTS Interpreter Development)
add_library(pandana SHARED
    src/accessibility.cpp
    src/cyaccess.pyx
    src/graphalg.cpp
    src/route_state.cpp
    src/transit_extensions.cpp
    src/contraction_hierarchies/src/libch.cpp)
target_compile_options(pandana PRIVATE -fopenmp)
target_link_libraries(pandana gomp)

add_executable(test_transpose tests/cpp/test_transpose.cpp src/route_state.cpp)
add_test(NAME test-transpose COMMAND test_transpose)

add_executable(test_extract_links tests/cpp/test_extract_links.cpp src/route_state.cpp)
add_test(NAME test-extract-links COMMAND test_extract_links)

add_executable(test_routing_state tests/cpp/test_routing_state.cpp src/route_state.cpp)
add_test(NAME test-routing-state COMMAND test_routing_state)

add_executable(test_route_stats
        tests/cpp/test_route_stats.cpp
        src/route_state.cpp
        src/transit_extensions.cpp
        src/accessibility.cpp
        src/graphalg.cpp
        src/contraction_hierarchies/src/libch.cpp)
target_compile_definitions(test_route_stats PRIVATE FORCE_ORIGINAL_ROUTES_FUNCTION)
add_test(NAME test-route-stats COMMAND test_route_stats)
