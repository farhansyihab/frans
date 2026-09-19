include_guard(GLOBAL)

option(FRANS_BUILD_TESTS "Build tests" OFF)

if(FRANS_BUILD_TESTS)
    enable_testing()

    # -----------------------------------------------------------------------
    # Scenario runner: Manual Import + Danger Check + Sandbox + error parser.
    #
    # Menyediakan:
    #   frans_scenarios                -> jalankan semua scenario
    #   frans_scenarios --list         -> daftar nama scenario
    #   frans_scenarios --dump NAME    -> print scenario (paste ke GUI)
    # -----------------------------------------------------------------------
    add_executable(frans_scenarios
        tests/ScenarioTest.cpp
        src/application/import/MultiFileParser.cpp
        src/application/safety/DangerCheckHandler.cpp
        src/infrastructure/fs/SandboxedFileSystem.cpp
        src/application/build_runner/BuildSystems/StandaloneBuildSystem.cpp
    )

    target_include_directories(frans_scenarios PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(frans_scenarios PRIVATE
        frans_warnings
    )

    add_test(NAME scenario_tests COMMAND frans_scenarios)

    # -----------------------------------------------------------------------
    # SEMENTARA — test ProcessExecutor di Windows (§9.2 #21).
    # Hapus blok ini setelah validasi selesai.
    # -----------------------------------------------------------------------
    add_executable(test_proc_win
        test_proc_win.cpp
        src/infrastructure/system/ProcessExecutor_win32.cpp
    )

    target_include_directories(test_proc_win PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(test_proc_win PRIVATE
        frans_warnings
    )
endif()