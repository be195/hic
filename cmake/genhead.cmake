if(NOT DOUTPUT)
    message(FATAL_ERROR "Variable DOUTPUT is empty in script!")
endif()

message(STATUS "Writing to: ${DOUTPUT}")

set(SINGLE_HEADER "// hic version ${PROJECT_VERSION}\n#pragma once\n\n")

if(NOT HIC_BUILD_SHARED)
    string(APPEND SINGLE_HEADER "// static build\n#define HIC_API\n")
endif()

foreach(header IN LISTS HIC_HEADER_INPUTS)
    if(EXISTS ${header})
        file(READ ${header} content)
        string(REGEX REPLACE "#pragma[ \t]+once[ \t]*\n" "" content "${content}")
        string(REGEX REPLACE "#include[ \t]*\"[^\"]+\"[ \t]*\n" "" content "${content}")

        string(APPEND SINGLE_HEADER "\n// --- ${header} ---\n${content}\n")
    endif()
endforeach()

file(WRITE "${DOUTPUT}" "${SINGLE_HEADER}")
