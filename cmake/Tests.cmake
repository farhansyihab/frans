include_guard(GLOBAL)

option(FRANS_BUILD_TESTS "Build tests" OFF)

if(FRANS_BUILD_TESTS)
    enable_testing()
    # TODO: daftarkan test executable di sini.
endif()