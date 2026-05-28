cmake_minimum_required(VERSION 3.16)
project(GravityGPU
    VERSION 0.1.0
    DESCRIPTION "Metal to Vulkan GPU Paravirtualization for macOS KVM"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ═══════════════════════════════════════════════════════════════════════
# Options
# ═══════════════════════════════════════════════════════════════════════

option(GRAVITY_BUILD_TESTS "Build test suite" ON)
option(GRAVITY_BUILD_TOOLS "Build debug tools" ON)
option(GRAVITY_VULKAN_VALIDATION "Enable Vulkan validation by default" OFF)

# ═══════════════════════════════════════════════════════════════════════
# Find Dependencies
# ═══════════════════════════════════════════════════════════════════════

find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)

message(STATUS "Vulkan include: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan library: ${Vulkan_LIBRARIES}")

# ═══════════════════════════════════════════════════════════════════════
# Protocol Library (shared between guest and host)
# ═══════════════════════════════════════════════════════════════════════

add_library(gravity-protocol STATIC
    protocol/src/gravity_protocol.c
    protocol/src/gravity_formats.c
)
target_include_directories(gravity-protocol PUBLIC
    protocol/include
)

# ═══════════════════════════════════════════════════════════════════════
# Host Daemon (gravityd)
# ═══════════════════════════════════════════════════════════════════════

add_executable(gravityd
    host-daemon/src/main.c
    host-daemon/src/vk_device.c
    host-daemon/src/resource_tracker.c
    host-daemon/src/cmd_processor.c
    host-daemon/src/metal_to_vulkan.c
    host-daemon/src/framebuffer.c
    host-daemon/src/shader_compiler.c
    host-daemon/src/shader_cache.c
)
target_include_directories(gravityd PRIVATE
    protocol/include
    host-daemon/src
    ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(gravityd PRIVATE
    gravity-protocol
    ${Vulkan_LIBRARIES}
    Threads::Threads
    rt
    m
)
target_compile_definitions(gravityd PRIVATE
    $<$<BOOL:${GRAVITY_VULKAN_VALIDATION}>:GRAVITY_ENABLE_VALIDATION=1>
)

# ═══════════════════════════════════════════════════════════════════════
# Tests
# ═══════════════════════════════════════════════════════════════════════

if(GRAVITY_BUILD_TESTS)
    enable_testing()

    # Protocol tests
    add_executable(test_protocol tests/test_protocol.c)
    target_link_libraries(test_protocol PRIVATE gravity-protocol)
    target_include_directories(test_protocol PRIVATE protocol/include)
    add_test(NAME protocol_tests COMMAND test_protocol)

    # Ring buffer tests
    add_executable(test_ring_buffer tests/test_ring_buffer.c)
    target_link_libraries(test_ring_buffer PRIVATE gravity-protocol)
    target_include_directories(test_ring_buffer PRIVATE protocol/include)
    add_test(NAME ring_buffer_tests COMMAND test_ring_buffer)
endif()

# ═══════════════════════════════════════════════════════════════════════
# Tools
# ═══════════════════════════════════════════════════════════════════════

if(GRAVITY_BUILD_TOOLS)
    add_executable(gravity-trace tools/gravity-trace.c)
    target_link_libraries(gravity-trace PRIVATE gravity-protocol)
    target_include_directories(gravity-trace PRIVATE protocol/include)
endif()

# ═══════════════════════════════════════════════════════════════════════
# Install
# ═══════════════════════════════════════════════════════════════════════

install(TARGETS gravityd RUNTIME DESTINATION bin)
if(GRAVITY_BUILD_TOOLS)
    install(TARGETS gravity-trace RUNTIME DESTINATION bin)
endif()

# ═══════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════

message(STATUS "")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "  GravityGPU Build Configuration")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "  Version        : ${PROJECT_VERSION}")
message(STATUS "  Build tests    : ${GRAVITY_BUILD_TESTS}")
message(STATUS "  Build tools    : ${GRAVITY_BUILD_TOOLS}")
message(STATUS "  Vulkan         : ${Vulkan_LIBRARIES}")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "")
message(STATUS "NOTE: The macOS guest driver (guest-driver/) must be built")
message(STATUS "      separately on macOS using Xcode / xcodebuild.")
message(STATUS "")
message(STATUS "NOTE: The QEMU device (qemu-patches/) must be integrated")
message(STATUS "      into a QEMU source tree and built with QEMU's meson.")
