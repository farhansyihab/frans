include_guard(GLOBAL)
option(FRANS_BUILD_TESTS "Build tests" OFF)
if(FRANS_BUILD_TESTS)
enable_testing()
# ------------------------------------------------------------------
# frans_scenarios
# Runner test scenario untuk Manual Import + Danger Check + Sandbox
# + error parser (traceback frame picking) + PromptGenerator (#42)
# + FileCollector (#46) + MarkdownRenderer (#48).
#
# frans_scenarios                -> jalankan semua scenario
# frans_scenarios --list         -> daftar nama scenario
# frans_scenarios --dump NAME    -> print scenario (paste ke GUI)
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
# Fase B #48: MarkdownRenderer (murni, tanpa I/O)
src/application/ingest/MarkdownRenderer.cpp
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
# Test ringan, TANPA FLTK dan TANPA ProsesExecutor:
# - summarizeBuildError()  : isi ringkasan error per phase
# - wasCancelledByUser()   : deteksi cancel
# - SessionState           : aturan publish / clear / skip-cancel
# - integrasi ke PromptGenerator (blok ERROR)
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
# ------------------------------------------------------------------
# frans_apikey_tests  (Fase C #54)
# ApiKeyProvider: env -> file kunci -> default ~/.gemini/api_key,
# trim/BOM/CRLF, izin file longgar, masking 4 karakter terakhir,
# dan pastikan diagnostik tidak membocorkan key.
#
# Test memakai HOME palsu di direktori temp; file kunci asli pengguna
# tidak disentuh. Tanpa FLTK, tanpa jaringan, tanpa subprocess.
# ------------------------------------------------------------------
add_executable(frans_apikey_tests
tests/ApiKeyProviderTest.cpp
src/application/ai/ApiKeyProvider.cpp
)
target_include_directories(frans_apikey_tests PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(frans_apikey_tests PRIVATE
frans_warnings
)
add_test(NAME apikey_provider_tests COMMAND frans_apikey_tests)
# ------------------------------------------------------------------
# frans_gemini_schema_tests  (Fase C #57)
# GeminiSchema: verifikasi struktur JSON schema untuk response Gemini
# dan file prompt gemini_selector_prompt.txt ada serta valid.
#
# Tanpa FLTK, tanpa jaringan, tanpa subprocess.
# ------------------------------------------------------------------
add_executable(frans_gemini_schema_tests
tests/GeminiSchemaTest.cpp
src/application/ai/GeminiSchema.cpp
)
target_include_directories(frans_gemini_schema_tests PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/src
${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(frans_gemini_schema_tests PRIVATE
frans_warnings
)
# Copy config files ke build directory agar test bisa akses
add_custom_command(TARGET frans_gemini_schema_tests POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory
${CMAKE_CURRENT_SOURCE_DIR}/config
${CMAKE_CURRENT_BINARY_DIR}/config
COMMENT "Copying config files for gemini schema tests"
)
add_test(NAME gemini_schema_tests COMMAND frans_gemini_schema_tests)
# ------------------------------------------------------------------
# frans_context_tests  (Fase C #60)
# SourceCodeBuilder:
#   - fnv1a_64_hex         : panjang, lowercase, konsistensi, vektor known
#   - buildManifestJson    : struktur JSON, escape karakter, files kosong
#   - buildSourceCode      : happy path, unselected excluded, content
#                            identik dengan disk, empty selection,
#                            error input kosong, cancel flag
#
# Tanpa FLTK, tanpa jaringan, tanpa subprocess, tanpa git.
# Memakai temp directory sebagai fixture (RAII, auto-cleanup).
# ------------------------------------------------------------------
set(FRANS_CONTEXT_TEST_SOURCES
tests/context/source_code_builder_tests.cpp
src/application/context/SourceCodeBuilder.cpp
src/application/context/ContextSelector.cpp
src/application/ingest/MarkdownRenderer.cpp
src/application/ingest/FileCollector.cpp
src/application/ai/GeminiClient.cpp
src/application/ai/ApiKeyProvider.cpp
src/application/ai/GeminiSchema.cpp
src/infrastructure/fs/SandboxedFileSystem.cpp
src/infrastructure/http/CurlTransport.cpp
)
if(WIN32)
list(APPEND FRANS_CONTEXT_TEST_SOURCES
src/infrastructure/system/ProcessExecutor_win32.cpp)
else()
list(APPEND FRANS_CONTEXT_TEST_SOURCES
src/infrastructure/system/ProcessExecutor_posix.cpp)
endif()
add_executable(frans_context_tests ${FRANS_CONTEXT_TEST_SOURCES})
target_include_directories(frans_context_tests PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/src
${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(frans_context_tests PRIVATE
Threads::Threads
${FRANS_PLATFORM_LIBS}
frans_warnings
)
if(FRANS_PLATFORM_DEFS)
target_compile_definitions(frans_context_tests PRIVATE ${FRANS_PLATFORM_DEFS})
endif()
add_test(NAME source_code_builder_tests COMMAND frans_context_tests)
endif()
