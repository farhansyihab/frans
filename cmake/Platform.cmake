include_guard(GLOBAL)

if(WIN32)
    include(cmake/PlatformWindows.cmake)
else()
    include(cmake/PlatformLinux.cmake)
endif()