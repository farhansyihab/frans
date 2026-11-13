include_guard(GLOBAL)
option(FRANS_BUILD_TESTS "Build tests" OFF)
if(FRANS_BUILD_TESTS)
enable_testing()
# ------------------------------------------------------------------
# frans_scenarios
# Runner test scenario untuk Manual Import + Danger Check + Sandbox
# + error parser (traceback frame picking) + PromptGenerator (#42)
# + FileCollector (#46).
#
#   frans_scenarios                -> jalankan semua scenario
#   frans_scenarios --list         -> daftar nama scenario
#   frans_scenarios --dump NAME    -> print scenario (paste ke GUI)
# ------------------------------------------------------------------
set(FRANS_SCENARIO_SOURCES
tests/ScenarioTest.cpp
src/application/import/MultiFileParser.cpp
src/application/safety/DangerCheckHandler.cpp
src/infrastructure/fs/SandboxedFileSystem.cpp
src/application/build_runner/BuildSystems/StandaloneBuildSystem.cpp
src/application/prompt/PromptGenerator.cpp
# Fase B #46: FileCollector + ProcessExecutor (untuk git ls-files fallback)
src/application/ingest/FileCollector.cpp
)
if(WIN32)
list(APPEND FRANS_SCENARIO_SOURCES
src/infrastructure/system/ProcessExecutor_win32.cpp)
else()
list(APPEND FRANS_SCENARIO_SOURCES
src/infrastructure/system/ProcessExecutor_posix.cpp)
endif()
add_executable(frans_scenarios ${FRANS_SCENARIO_SOURCES})
target_include_directories(frans_scenarios PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(frans_scenarios PRIVATE
Threads::Threads
${FRANS_PLATFORM_LIBS}
frans_warnings
)
if(FRANS_PLATFORM_DEFS)
target_compile_definitions(frans_scenarios PRIVATE ${FRANS_PLATFORM_DEFS})
endif()
add_test(NAME scenario_tests COMMAND frans_scenarios)
# ------------------------------------------------------------------
# frans_session_tests  (Fase A #44 + #45)
#
# Test ringan, TANPA FLTK dan TANPA ProsesExecutor:
#   - summarizeBuildError()  : isi ringkasan error per phase
#   - wasCancelledByUser()   : deteksi cancel
#   - SessionState           : aturan publish / clear / skip-cancel
#   - integrasi ke PromptGenerator (blok ERROR)
#
# Hanya 3 file .cpp -> compile cepat di mesin lama.
# ------------------------------------------------------------------
add_executable(frans_session_tests
tests/SessionStateTest.cpp
src/application/build_runner/BuildErrorSummary.cpp
src/application/prompt/PromptGenerator.cpp
)
target_include_directories(frans_session_tests PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(frans_session_tests PRIVATE
frans_warnings
)
add_test(NAME session_state_tests COMMAND frans_session_tests)
endif()
