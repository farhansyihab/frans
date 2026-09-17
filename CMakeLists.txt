# ~/AgenAI/frans/CMakeLists.txt
#
# Root CMakeLists.txt — sangat tipis.
# Semua detail dipecah ke cmake/*.cmake yang di-include() di bawah.
# include() dipakai (bukan add_subdirectory()) supaya semua target
# dan variabel (FRANS_HAS_*_PARSER, FRANS_CODE_INTEL_ENABLED,
# FRANS_HTTPLIB_DEFS, FRANS_OPENSSL_LIBS, dst.) tetap satu scope.
#
# URUTAN include PENTING (CMake eksekusi top-to-bottom):
#   1. Dependencies        — find_package + OpenSSL detection
#   2. TreeSitter          — deteksi grammar, set FRANS_HAS_*_PARSER
#   3. Libraries           — static lib (frans_fs, frans_routing, ...)
#   4. ParserTests         — test executable + CTest (butuh frans_code_intel)
#   5. AgentTests          — test executable + CTest (butuh frans_project)
#   6. StandaloneTests     — test executable + CTest
#   7. ProviderTests       — test executable + CTest (label network)
#   8. MainExecutable      — frans_ai (butuh semua library di atas)
#
# Stage history:
#   5    — tree-sitter Python
#   7a-g — sandbox, diff, agent loop, snapshot, cancel
#   8    — DeepSeekProvider
#   9    — ProviderRouter + EscalationPolicy + DailyBudgetGuard
#   10a-d — parser C++/TS/JS/Vue + multi-language integration
#   11a-d — ManualImporter + ManualImportDialog + BuildRunner
#   11d-revisi — BuildRunner by marker (CMake/Make/Meson/npm/Poetry/Standalone)
#   12   — GeminiProvider (3-tier: Local → Gemini → DeepSeek)
#   refactor-cmake — split monolith CMakeLists.txt ke cmake/ + CTest

cmake_minimum_required(VERSION 3.16)
project(frans_ai LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Standar bahasa
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Build type default
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Output layout
# ---------------------------------------------------------------------------
# Semua executable ke build/bin/ — dipakai BuildRunner (StandaloneBuildSystem
# pakai path relative, dan ctest resolve via RUNTIME_OUTPUT_DIRECTORY).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# compile_commands.json di build/ — dipakai clangd / ccls.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# CTest
# ---------------------------------------------------------------------------
# enable_testing() HARUS di top-level CMakeLists.txt (CMake docs).
# Membuat file CTestTestfile.cmake di build/ supaya `ctest` bisa jalan.
enable_testing()

# ---------------------------------------------------------------------------
# Include semua modul
# ---------------------------------------------------------------------------
include(cmake/Dependencies.cmake)
include(cmake/TreeSitter.cmake)
include(cmake/Libraries.cmake)
include(cmake/ParserTests.cmake)
include(cmake/AgentTests.cmake)
include(cmake/StandaloneTests.cmake)
include(cmake/ProviderTests.cmake)
include(cmake/MainExecutable.cmake)

# ---------------------------------------------------------------------------
# Hint penggunaan CTest
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== Cara menjalankan test ===")
message(STATUS "  Semua test (butuh API key + llama-server):")
message(STATUS "    cd build && ctest --output-on-failure")
message(STATUS "")
message(STATUS "  Offline saja (skip network + llama) — RECOMMENDED:")
message(STATUS "    cd build && ctest -LE \"network|requires-llama\" --output-on-failure")
message(STATUS "")
message(STATUS "  Hanya test tertentu:")
message(STATUS "    cd build && ctest -R build_runner --output-on-failure")
message(STATUS "")