cmake_minimum_required(VERSION 3.15)
project(reaserve VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Generate version header from CMake project version
configure_file(src/version.h.in "${CMAKE_BINARY_DIR}/generated/version.h")

# Main plugin library
add_library(reaper_reaserve MODULE
    src/main.cpp
    src/tcp_server.cpp
    src/json_rpc.cpp
    src/command_queue.cpp
    src/command_registry.cpp
    src/reaper_api.cpp
    src/config.cpp
    src/undo.cpp
    src/commands/ping.cpp
    src/commands/lua.cpp
    src/commands/project.cpp
    src/commands/tracks.cpp
    src/commands/items.cpp
    src/commands/fx.cpp
    src/commands/midi.cpp
    src/commands/transport.cpp
    src/commands/markers.cpp
    src/commands/routing.cpp
    src/commands/envelope.cpp
)

target_include_directories(reaper_reaserve PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/vendor
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

set_target_properties(reaper_reaserve PROPERTIES
    PREFIX ""
    OUTPUT_NAME "reaper_reaserve"
)

if(APPLE)
    set_target_properties(reaper_reaserve PROPERTIES SUFFIX ".dylib")
endif()

target_precompile_headers(reaper_reaserve PRIVATE <nlohmann/json.hpp>)

if(WIN32)
    # Prevent windows.h from pulling in winsock.h (conflicts with winsock2.h)
    target_compile_definitions(reaper_reaserve PRIVATE WIN32_LEAN_AND_MEAN)
    target_link_libraries(reaper_reaserve PRIVATE ws2_32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(reaper_reaserve PRIVATE pthread)
endif()

# Tests
option(REASERVE_BUILD_TESTS "Build unit tests" ON)
if(REASERVE_BUILD_TESTS)
    enable_testing()

    # Test sources that don't need REAPER
    add_executable(test_json_rpc
        tests/test_json_rpc.cpp
        src/json_rpc.cpp
    )
    target_include_directories(test_json_rpc PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/vendor
        ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME test_json_rpc COMMAND test_json_rpc)

    add_executable(test_command_queue
        tests/test_command_queue.cpp
        src/command_queue.cpp
    )
    target_include_directories(test_command_queue PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/vendor
        ${CMAKE_SOURCE_DIR}/src
    )
    if(UNIX)
        target_link_libraries(test_command_queue PRIVATE pthread)
    endif()
    add_test(NAME test_command_queue COMMAND test_command_queue)

    add_executable(test_framing
        tests/test_framing.cpp
        src/tcp_server.cpp
    )
    target_include_directories(test_framing PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/vendor
        ${CMAKE_SOURCE_DIR}/src
    )
    if(WIN32)
        target_link_libraries(test_framing PRIVATE ws2_32)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(test_framing PRIVATE pthread)
    endif()
    add_test(NAME test_framing COMMAND test_framing)
endif()
