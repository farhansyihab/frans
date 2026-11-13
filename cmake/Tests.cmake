include_guard(GLOBAL)
option(FRANS_BUILD_TESTS "Build tests" OFF)
if(FRANS_BUILD_TESTS)
enable_testing()
# Runner test scenario untuk Manual Import + Danger Check + Sandbox
# + error parser (traceback frame picking) + PromptGenerator (Fase A #42).
#
# Menyediakan:
#   frans_scenarios                -> jalankan semua scenario
#   frans_scenarios --list         -> daftar nama scenario
#   frans_scenarios --dump NAME    -> print scenario (paste ke GUI)
#
# Build: cmake -S . -B build -DFRANS_BUILD_TESTS=ON && cmake --build build -j
add_executable(frans_scenarios
tests/ScenarioTest.cpp
src/application/import/MultiFileParser.cpp
src/application/safety/DangerCheckHandler.cpp
src/infrastructure/fs/SandboxedFileSystem.cpp
src/application/build_runner/BuildSystems/StandaloneBuildSystem.cpp
src/application/prompt/PromptGenerator.cpp
)
target_include_directories(frans_scenarios PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(frans_scenarios PRIVATE
frans_warnings
)
add_test(NAME scenario_tests COMMAND frans_scenarios)
endif()