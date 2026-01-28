cmake_minimum_required(VERSION 3.20)
project(hic VERSION 1.0.0)

include(cmake/CPM.cmake)

option(HIC_BUILD_SHARED "Build hic as a shared library" OFF)
set(HIC_LIB_TYPE STATIC)
if(HIC_BUILD_SHARED)
    set(HIC_LIB_TYPE SHARED)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
endif()

CPMAddPackage(
        NAME SDL3
        GITHUB_REPOSITORY libsdl-org/SDL
        GIT_TAG release-3.4.0
        OPTIONS "SDL_SHARED ON" "SDL_STATIC OFF"
)

CPMAddPackage(
        NAME ogg
        GITHUB_REPOSITORY xiph/ogg
        GIT_TAG v1.3.6
        OPTIONS "BUILD_TESTING OFF" "INSTALL_DOCS OFF"
)

CPMAddPackage(
        NAME opus
        GITHUB_REPOSITORY xiph/opus
        GIT_TAG v1.5.2
        OPTIONS "OPUS_BUILD_TESTING OFF" "OPUS_BUILD_PROGRAMS OFF"
)

CPMAddPackage(
        NAME opusfile
        GITHUB_REPOSITORY xiph/opusfile
        GIT_TAG main
        OPTIONS "OP_DISABLE_HTTP ON" "OP_DISABLE_EXAMPLES ON" "OP_DISABLE_DOCS ON"
)

CPMAddPackage(
        NAME nlohmann_json
        GITHUB_REPOSITORY nlohmann/json
        GIT_TAG v3.12.0
)

add_library(hic ${HIC_LIB_TYPE}
        src/assets/audio.cpp
        src/assets/image.cpp
        src/assets/spritesheet.cpp
        src/assets/bitmap.cpp
        src/assets/manager.cpp
        src/audio/stream.cpp
        src/audio/bus.cpp
        src/audio/manager.cpp
        src/basecomponent.cpp
        src/container.cpp
        src/watchdog.cpp
)

target_compile_features(hic PUBLIC cxx_std_20)

if(HIC_BUILD_SHARED)
    target_compile_definitions(hic PRIVATE HIC_EXPORTS)
    target_compile_definitions(hic PUBLIC HIC_BUILD_SHARED)
endif()

set(HIC_BASE_HEADERS
        utils/hicapi.hpp
        utils/logging.hpp
        utils/events.hpp
        utils/geometry.hpp
        utils/easing.hpp
        utils/animation.hpp
        utils/util.hpp
        utils/bmfont.hpp
        assets/base.hpp
        assets/audio.hpp
        assets/image.hpp
        assets/spritesheet.hpp
        assets/bitmap.hpp
        assets/manager.hpp
        audio/const.hpp
        audio/effect.hpp
        audio/stream.hpp
        audio/bus.hpp
        audio/manager.hpp
        basecomponent.hpp
        container.hpp
        watchdog.hpp
)

set(HIC_HEADER_INPUTS)
foreach(header ${HIC_BASE_HEADERS})
    if(NOT HIC_BUILD_SHARED AND header STREQUAL "utils/hicapi.hpp")
        continue()
    endif()
    list(APPEND HIC_HEADER_INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/src/${header}")
endforeach()

set(HIC_GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/hic.hpp")

add_custom_command(
        OUTPUT "${HIC_GENERATED_HEADER}"
        COMMAND ${CMAKE_COMMAND}
            "-DDOUTPUT=${HIC_GENERATED_HEADER}"
            "-DHIC_HEADER_INPUTS=${HIC_HEADER_INPUTS}"
            "-DPROJECT_VERSION=${PROJECT_VERSION}"
            "-DHIC_BUILD_SHARED=${HIC_BUILD_SHARED}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/genhead.cmake"
        DEPENDS ${HIC_HEADER_INPUTS}
        COMMENT "Bundling hic.hpp..."
        VERBATIM
)

target_sources(hic PUBLIC "${HIC_GENERATED_HEADER}")

target_include_directories(hic
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(hic PUBLIC
        SDL3::SDL3
        opusfile
        opus
        ogg
        nlohmann_json::nlohmann_json
)

# example build
add_executable(hicex WIN32 example/main.cpp)
target_sources(hicex PUBLIC "${HIC_GENERATED_HEADER}") # fucker
target_link_libraries(hicex PRIVATE hic)

if(WIN32)
    add_custom_command(TARGET hicex POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:hicex>
            $<TARGET_FILE_DIR:hicex>
            COMMAND_EXPAND_LISTS
    )
endif()