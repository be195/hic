cmake_minimum_required(VERSION 3.20)
project(hic VERSION 1.0.0)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.0
    OPTIONS
        "SDL_SHARED ON"
        "SDL_STATIC OFF"
)

find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)

set(CMAKE_CXX_STANDARD 20)

function(read_file_content filepath output_var)
    file(READ ${filepath} content)
    set(${output_var} "${content}" PARENT_SCOPE)
endfunction()

function(remove_includes content output_var)
    string(REGEX REPLACE "#include[ \t]*[\"].+[\"][ \t]*\n" "" result "${content}")
    string(REGEX REPLACE "#pragma[ \t]+once[ \t]*\n" "" result "${result}")
    set(${output_var} "${result}" PARENT_SCOPE)
endfunction()

set(SINGLE_HEADER "// hic\n")
string(APPEND SINGLE_HEADER "#pragma once\n\n")

set(HIC_HEADERS
    utils/logging.hpp
    utils/events.hpp
    utils/geometry.hpp
    utils/easing.hpp
    utils/animation.hpp
    basecomponent.hpp
    container.hpp
)

foreach(header ${HIC_HEADERS})
    set(header_path "${CMAKE_CURRENT_SOURCE_DIR}/src/${header}")

    if(EXISTS ${header_path})
        read_file_content(${header_path} header_content)
        remove_includes("${header_content}" cleaned_content)

        string(APPEND SINGLE_HEADER "\n// ${header}\n")
        string(APPEND SINGLE_HEADER "${cleaned_content}\n")
    endif()
endforeach()

file(WRITE "${CMAKE_BINARY_DIR}/include/hic.hpp" "${SINGLE_HEADER}")

install(FILES "${CMAKE_BINARY_DIR}/include/hic.hpp"
    DESTINATION include)

add_library(hic STATIC
    src/basecomponent.cpp
    src/container.cpp
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/hic.hpp DESTINATION include)

target_include_directories(hic PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(hic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(hic PUBLIC
    SDL3::SDL3
    OpenGL::GL
)

set_target_properties(hic PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "include/hic.hpp"
)

add_executable(hicex example/main.cpp)

target_link_libraries(hicex PUBLIC
    SDL3::SDL3
    OpenGL::GL
    hic
)

target_include_directories(hicex PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/example
)

if(WIN32)
    add_custom_command(TARGET hicex
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:hicex>
        $<TARGET_FILE_DIR:hicex>
        COMMAND_EXPAND_LISTS
    )
endif()
