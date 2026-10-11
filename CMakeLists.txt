# thebigs2 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(thebigs2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(THEBIGS2_SOURCES
    src/main.cpp
    src/hooks.cpp
    src/renderdoc_capture.cpp
)

if(WIN32)
    add_executable(thebigs2 WIN32 ${THEBIGS2_SOURCES})
else()
    add_executable(thebigs2 ${THEBIGS2_SOURCES})
endif()

# GPU emulation is opt-in on the development branch; ship the Xenos plugin.
rexglue_setup_target(thebigs2 GPU_PLUGINS xenos)

# The SDK sets a $ORIGIN rpath on Linux but (unlike Windows/macOS) never stages
# librexruntime.so / libTracyClient.so next to the binary, so it won't start.
if(UNIX AND NOT APPLE)
    foreach(_lib rex::runtime rexruntime rex::TracyClient TracyClient)
        if(TARGET ${_lib})
            add_custom_command(TARGET thebigs2 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:${_lib}> $<TARGET_FILE_DIR:thebigs2>
                VERBATIM)
        endif()
    endforeach()
endif()
