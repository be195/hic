#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef HIC_BUILD_SHARED
        #ifdef HIC_EXPORTS
            #define HIC_API __declspec(dllexport)
        #else
            #define HIC_API __declspec(dllimport)
        #endif
    #else
        #define HIC_API
    #endif
#else
    #define HIC_API __attribute__((visibility("default")))
#endif