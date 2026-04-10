cmake_minimum_required(VERSION 3.20)

project(edge_probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
    edge_probe_core
    src/collectors.cpp
    src/telemetry_sender.cpp)

target_include_directories(edge_probe_core PUBLIC include)

target_compile_options(
    edge_probe_core
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic)

find_package(CURL QUIET)

if(CURL_FOUND)
    add_library(edge_probe_curl_transport src/curl_http_transport.cpp)
    target_include_directories(edge_probe_curl_transport PUBLIC include)
    target_link_libraries(edge_probe_curl_transport PUBLIC edge_probe_core CURL::libcurl)
    target_compile_options(
        edge_probe_curl_transport
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic)

    add_executable(edge_probe_vm_fixture_sender src/vm_fixture_sender_main.cpp)
    target_link_libraries(
        edge_probe_vm_fixture_sender
        PRIVATE edge_probe_core edge_probe_curl_transport)
    target_compile_options(
        edge_probe_vm_fixture_sender
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic)
endif()

enable_testing()

add_executable(edge_probe_tests tests/test_main.cpp)
target_link_libraries(edge_probe_tests PRIVATE edge_probe_core)
target_compile_definitions(
    edge_probe_tests
    PRIVATE EDGE_PROBE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_options(
    edge_probe_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic)

add_test(NAME edge_probe_tests COMMAND edge_probe_tests)
