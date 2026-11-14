# SourceCode.md: frans
Dibuat oleh Frans | 2026-11-15 05:33 | 16 file disertakan | 32.1 KB
Permintaan: - baca `BluePrint-v9-Addendum.md` dan `frans-ingest.md`
- `SourceCode.txt` adalah code yang paling terakhir, paling update
- PERHATIKAN root app adalah `/home/farhan/Proyek/AgenAI/proyek/frans/`
- bua
Tanda "*" pada struktur di bawah = isi file disertakan setelah bagian ini.

Directory structure:
└── frans/
    ├── .gitignore
    ├── CMakeLists.txt *
    ├── CMakePresets.json
    ├── README.md
    ├── cmake/
    │   ├── CompilerWarnings.cmake *
    │   ├── Dependencies.cmake *
    │   ├── FLTK.cmake *
    │   ├── GeminiTests.cmake *
    │   ├── Install.cmake *
    │   ├── NlohmannJson.cmake *
    │   ├── Platform.cmake *
    │   ├── PlatformLinux.cmake *
    │   ├── PlatformWindows.cmake *
    │   ├── ProjectOptions.cmake *
    │   ├── Sources.cmake *
    │   └── Tests.cmake *
    ├── config/
    │   ├── app.json
    │   ├── gemini_selector_prompt.txt
    │   └── prompt_template.txt
    ├── include/
    │   └── frans/
    │       └── gemini/
    │           └── ApiKeyProvider.h
    ├── packaging/
    │   └── linux/
    │       ├── frans.desktop.in
    │       └── frans.svg
    ├── prompt_template.txt
    ├── scripts/
    │   ├── README.md
    │   ├── create-shortcut.bat
    │   ├── create-shortcut.ps1
    │   └── install-desktop-user.sh
    ├── src/
    │   ├── application/
    │   │   ├── ai/
    │   │   │   ├── ApiKeyProvider.cpp
    │   │   │   ├── ApiKeyProvider.h
    │   │   │   ├── GeminiClient.cpp
    │   │   │   ├── GeminiClient.h
    │   │   │   ├── GeminiSchema.cpp
    │   │   │   └── GeminiSchema.h
    │   │   ├── build_runner/
    │   │   │   ├── BuildErrorSummary.cpp
    │   │   │   ├── BuildErrorSummary.h
    │   │   │   ├── BuildReport.h
    │   │   │   ├── BuildRunner.cpp
    │   │   │   ├── BuildRunner.h
    │   │   │   ├── BuildSystems/
    │   │   │   │   ├── CMakeBuildSystem.cpp
    │   │   │   │   ├── CMakeBuildSystem.h
    │   │   │   │   ├── MakeBuildSystem.cpp
    │   │   │   │   ├── MakeBuildSystem.h
    │   │   │   │   ├── MesonBuildSystem.cpp
    │   │   │   │   ├── MesonBuildSystem.h
    │   │   │   │   ├── NpmBuildSystem.cpp
    │   │   │   │   ├── NpmBuildSystem.h
    │   │   │   │   ├── PoetryBuildSystem.cpp
    │   │   │   │   ├── PoetryBuildSystem.h
    │   │   │   │   ├── StandaloneBuildSystem.cpp
    │   │   │   │   └── StandaloneBuildSystem.h
    │   │   │   ├── IBuildSystem.h
    │   │   │   ├── ProjectDetector.cpp
    │   │   │   ├── ProjectDetector.h
    │   │   │   └── ProjectInfo.h
    │   │   ├── context/
    │   │   │   ├── ContextSelector.cpp
    │   │   │   ├── ContextSelector.h
    │   │   │   ├── SourceCodeBuilder.cpp
    │   │   │   └── SourceCodeBuilder.h
    │   │   ├── import/
    │   │   │   ├── MultiFileParser.cpp
    │   │   │   └── MultiFileParser.h
    │   │   ├── ingest/
    │   │   │   ├── FileCollector.cpp
    │   │   │   ├── FileCollector.h
    │   │   │   ├── MarkdownRenderer.cpp
    │   │   │   ├── MarkdownRenderer.h
    │   │   │   ├── RepoIngester.cpp
    │   │   │   └── RepoIngester.h
    │   │   ├── manual_import/
    │   │   │   ├── ManualImporter.cpp
    │   │   │   └── ManualImporter.h
    │   │   ├── prompt/
    │   │   │   ├── PromptGenerator.cpp
    │   │   │   └── PromptGenerator.h
    │   │   └── safety/
    │   │       ├── DangerCheckHandler.cpp
    │   │       └── DangerCheckHandler.h
    │   ├── config/
    │   │   ├── AppConfig.cpp
    │   │   ├── AppConfig.h
    │   │   ├── AppPaths.cpp
    │   │   └── AppPaths.h
    │   ├── domain/
    │   │   ├── fs/
    │   │   │   └── IFileSystemPort.h
    │   │   └── import/
    │   │       ├── GeneratedFile.h
    │   │       └── ImportResult.h
    │   ├── infrastructure/
    │   │   ├── fs/
    │   │   │   ├── SandboxedFileSystem.cpp
    │   │   │   └── SandboxedFileSystem.h
    │   │   ├── http/
    │   │   │   ├── CurlTransport.cpp
    │   │   │   ├── CurlTransport.h
    │   │   │   └── IHttpTransport.h
    │   │   └── system/
    │   │       ├── DiffEngine.cpp
    │   │       ├── DiffEngine.h
    │   │       ├── ProcessExecutor.cpp
    │   │       ├── ProcessExecutor.h *
    │   │       ├── ProcessExecutor_posix.cpp *
    │   │       ├── ProcessExecutor_win32.cpp *
    │   │       └── SystemMaintenanceFacade.h
    │   ├── ingest/
    │   │   ├── IngestDialog.cpp
    │   │   ├── IngestDialog.h
    │   │   ├── RepoIngester.cpp
    │   │   └── RepoIngester.h
    │   ├── main.cpp
    │   └── ui/
    │       ├── IngestDialog.cpp
    │       ├── IngestDialog.h
    │       ├── MainWindow.cpp
    │       ├── MainWindow.h
    │       ├── ManualImportDialog.cpp
    │       ├── ManualImportDialog.h
    │       ├── PromptDialog.cpp
    │       ├── PromptDialog.h
    │       └── SessionState.h
    ├── tests/
    │   ├── ApiKeyProviderTest.cpp
    │   ├── GeminiSchemaTest.cpp
    │   ├── README-FaseA-44.md
    │   ├── README.md
    │   ├── ScenarioTest.cpp
    │   ├── SessionStateTest.cpp
    │   ├── ai/
    │   │   ├── FakeHttpTransport.h
    │   │   └── gemini_client_tests.cpp
    │   ├── context/
    │   │   └── source_code_builder_tests.cpp
    │   └── manual/
    │       ├── fixtures-generated/
    │       │   ├── 01-cmake-fail.md
    │       │   ├── 02-cmake-fix.md
    │       │   ├── 03-python-fail.md
    │       │   ├── 04-sleeper.md
    │       │   └── 05-python-ok.md
    │       └── make-fasea44-fixtures.sh
    └── ~/
        └── frans-out/
            ├── SourceCode.md
            └── SourceCode.md.manifest.json

================================================
FILE: CMakeLists.txt
================================================
# Root CMakeLists.txt — tipis.
# Detail dipecah ke cmake/*.cmake yang di-include berurutan.
cmake_minimum_required(VERSION 3.16)
project(frans VERSION 1.0.0 LANGUAGES CXX)

include(cmake/ProjectOptions.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Platform.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Sources.cmake)
include(cmake/Tests.cmake)
include(cmake/Install.cmake)
include(cmake/GeminiTests.cmake)
================================================
FILE: cmake/CompilerWarnings.cmake
================================================
include_guard(GLOBAL)

add_library(frans_warnings INTERFACE)

if(MSVC)
    target_compile_options(frans_warnings INTERFACE
        /W4
        /permissive-
    )
else()
    target_compile_options(frans_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
endif()
================================================
FILE: cmake/Dependencies.cmake
================================================
include_guard(GLOBAL)
include(cmake/FLTK.cmake)
include(cmake/NlohmannJson.cmake)
find_package(Threads REQUIRED)
================================================
FILE: cmake/FLTK.cmake
================================================
include_guard(GLOBAL)

find_package(FLTK REQUIRED)

if(NOT TARGET FLTK::FLTK)
    add_library(FLTK::FLTK INTERFACE IMPORTED)
    target_include_directories(FLTK::FLTK INTERFACE ${FLTK_INCLUDE_DIRS})
    target_link_libraries(FLTK::FLTK INTERFACE ${FLTK_LIBRARIES})
endif()
================================================
FILE: cmake/GeminiTests.cmake
================================================
# Fase C #56 -- target test offline untuk GeminiClient (transport palsu).
# Aktifkan dengan menambahkan satu baris di CMakeLists.txt root:
#   include(cmake/GeminiTests.cmake)

find_package(Threads REQUIRED)

# --- Cari nlohmann/json.hpp secara mandiri ---
#
# Urutan prioritas:
#   1. Override manual:  -DFRANS_GEMINI_JSON_INCLUDE_DIR=/path/ke/include-root
#   2. Target CMake dari NlohmannJson.cmake (nlohmann_json::nlohmann_json)
#   3. Pencarian rekursif di source tree + build tree (struktur folder bebas)
#   4. Lokasi sistem (/usr/include, /usr/local/include)

set(_frans_json_include_dirs "")
set(_frans_json_found "")

# (2) Jika NlohmannJson.cmake membuat target, ambil include dir-nya
foreach(_tgt nlohmann_json::nlohmann_json nlohmann_json)
  if(TARGET ${_tgt})
    get_target_property(_tgt_inc ${_tgt} INTERFACE_INCLUDE_DIRECTORIES)
    if(_tgt_inc)
      foreach(_i IN LISTS _tgt_inc)
        # Abaikan generator expression ($<BUILD_INTERFACE:...>) yang tak bisa di-resolve di sini
        if(NOT _i MATCHES "^\\$<" AND EXISTS "${_i}/nlohmann/json.hpp")
          list(APPEND _frans_json_include_dirs "${_i}")
        endif()
      endforeach()
    endif()
  endif()
endforeach()

# (3) Pencarian rekursif: cari SEMUA json.hpp lalu saring yang berada di folder "nlohmann"
file(GLOB_RECURSE _frans_all_json_hpp
  "${CMAKE_SOURCE_DIR}/json.hpp"
  "${CMAKE_BINARY_DIR}/json.hpp"
)
# GLOB_RECURSE hanya mencocokkan nama file di semua subfolder, jadi pola di atas
# menjangkau seluruh pohon. Saring hanya yang berada di .../nlohmann/json.hpp
foreach(_h IN LISTS _frans_all_json_hpp)
  if(_h MATCHES "/nlohmann/json\\.hpp$")
    list(APPEND _frans_json_found "${_h}")
    get_filename_component(_d "${_h}" DIRECTORY)   # .../nlohmann
    get_filename_component(_d "${_d}" DIRECTORY)   # include root
    list(APPEND _frans_json_include_dirs "${_d}")
  endif()
endforeach()

if(_frans_json_include_dirs)
  list(REMOVE_DUPLICATES _frans_json_include_dirs)
endif()

# (1) dan (4): find_path menghormati cache variable FRANS_GEMINI_JSON_INCLUDE_DIR
# jika sudah di-set manual lewat -D.
find_path(FRANS_GEMINI_JSON_INCLUDE_DIR
  NAMES nlohmann/json.hpp
  HINTS ${_frans_json_include_dirs}
  PATHS /usr/include /usr/local/include
)

if(NOT FRANS_GEMINI_JSON_INCLUDE_DIR)
  message(STATUS "[frans] Kandidat json.hpp yang ditemukan:")
  foreach(_h IN LISTS _frans_json_found)
    message(STATUS "  ${_h}")
  endforeach()

  message(FATAL_ERROR
    "[frans] nlohmann/json.hpp tidak ditemukan untuk frans_gemini_tests\n"
    "Hints yang dicoba: ${_frans_json_include_dirs}\n"
    "Solusi: letakkan json.hpp di <folder>/nlohmann/json.hpp di dalam proyek, atau jalankan:\n"
    "  cmake -S . -B build -DFRANS_GEMINI_JSON_INCLUDE_DIR=/path/ke/folder-induk-nlohmann")
endif()

message(STATUS "[frans] frans_gemini_tests: json.hpp di ${FRANS_GEMINI_JSON_INCLUDE_DIR}")

add_executable(frans_gemini_tests
  tests/ai/gemini_client_tests.cpp
  src/application/ai/GeminiClient.cpp
)

target_include_directories(frans_gemini_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}"
  "${FRANS_GEMINI_JSON_INCLUDE_DIR}"
)

target_compile_features(frans_gemini_tests PRIVATE cxx_std_17)
target_link_libraries(frans_gemini_tests PRIVATE Threads::Threads)

if(MSVC)
  target_compile_options(frans_gemini_tests PRIVATE /W4)
else()
  target_compile_options(frans_gemini_tests PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME gemini_client COMMAND frans_gemini_tests)
set_tests_properties(gemini_client PROPERTIES TIMEOUT 60)

message(STATUS "[frans] frans_gemini_tests aktif")
================================================
FILE: cmake/Install.cmake
================================================
include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Binary + config inti.
# ---------------------------------------------------------------------------

install(TARGETS frans RUNTIME DESTINATION bin)
install(FILES config/app.json DESTINATION share/frans)

# ---------------------------------------------------------------------------
# Linux: .desktop + icon supaya muncul di menu aplikasi.
#
# CATATAN: `Exec=` di-resolve pada CONFIGURE time memakai
# ${CMAKE_INSTALL_PREFIX}. Kalau install pakai prefix lain, set saat
# configure:
#     cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/frans
# Bukan lewat `cmake --install --prefix` (itu akan bikin Exec salah).
#
# Kalau user pakai install user-level (tanpa sudo), pakai script
# `scripts/install-desktop-user.sh` — script itu menulis Exec dengan
# path absolut binary hasil build.
# ---------------------------------------------------------------------------

if(UNIX AND NOT APPLE)
    set(FRANS_INSTALL_EXEC "${CMAKE_INSTALL_PREFIX}/bin/frans")

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/frans.desktop.in"
        "${CMAKE_CURRENT_BINARY_DIR}/frans.desktop"
        @ONLY)

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/frans.desktop"
            DESTINATION share/applications)

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/frans.svg"
            DESTINATION share/icons/hicolor/scalable/apps
            RENAME frans.svg)
endif()

# ---------------------------------------------------------------------------
# Windows: tidak ada installer / shortcut otomatis.
# User jalankan `scripts/create-shortcut.ps1` manual setelah build.
# Alasan: personal use (§9.5), git clone + build workflow.
# ---------------------------------------------------------------------------
================================================
FILE: cmake/NlohmannJson.cmake
================================================
# cmake/NlohmannJson.cmake
#
# Fase C #53 -- vendor nlohmann/json (single header).
#
# Pola disamakan dengan cmake/FLTK.cmake:
#   - Buat INTERFACE IMPORTED target `nlohmann_json::nlohmann_json`.
#   - Include directory = <repo>/third_party (bukan third_party/nlohmann),
#     supaya kode bisa tulis `#include <nlohmann/json.hpp>`.
#   - Warning saat configure kalau file belum di-download.
#   - Tidak ada dependensi sistem, jalan offline setelah file ada.

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Lokasi vendor header.
# ---------------------------------------------------------------------------
set(NLOHMANN_JSON_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(NLOHMANN_JSON_HEADER     "${NLOHMANN_JSON_VENDOR_DIR}/nlohmann/json.hpp")

# ---------------------------------------------------------------------------
# Cek keberadaan file. Warning (bukan error) saat configure supaya
# CMakePresets / IDE tetap bisa load project. Compile akan gagal lewat
# #error di placeholder kalau file belum di-download.
# ---------------------------------------------------------------------------
if(NOT EXISTS "${NLOHMANN_JSON_HEADER}")
    message(WARNING
        "[frans] nlohmann/json.hpp TIDAK DITEMUKAN di:\n"
        "    ${NLOHMANN_JSON_HEADER}\n"
        "    Build akan GAGAL sampai file di-download.\n"
        "    Lihat: third_party/README.md\n"
        "    Quick fix:\n"
        "      curl -L -o third_party/nlohmann/json.hpp \\\n"
        "        https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp")
else()
    # Cek ukuran kasar -- placeholder < 5 KB, file asli ~800 KB.
    file(SIZE "${NLOHMANN_JSON_HEADER}" _nj_size)
    if(_nj_size LESS 10000)
        message(WARNING
            "[frans] nlohmann/json.hpp terdeteksi sebagai PLACEHOLDER "
            "(${_nj_size} bytes). File asli ~800 KB.\n"
            "    Download versi asli dari: "
            "https://github.com/nlohmann/json/releases")
    else()
        message(STATUS
            "[frans] nlohmann/json.hpp OK (${_nj_size} bytes)")
    endif()
    unset(_nj_size)
endif()

# ---------------------------------------------------------------------------
# Definisikan target INTERFACE IMPORTED.
# Idempoten: kalau target sudah ada (mis. dari parent scope), skip.
# ---------------------------------------------------------------------------
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)

    # Include path = third_party/ (bukan third_party/nlohmann/),
    # supaya #include <nlohmann/json.hpp> bekerja.
    target_include_directories(nlohmann_json::nlohmann_json
        INTERFACE
            "${NLOHMANN_JSON_VENDOR_DIR}"
    )

    # nlohmann/json butuh C++17. Proyek ini sudah set C++20 di
    # ProjectOptions.cmake, tapi kita tegaskan di target supaya
    # kalau nanti di- reuse di tempat lain, requirement-nya eksplisit.
    target_compile_features(nlohmann_json::nlohmann_json
        INTERFACE
            cxx_std_17
    )
endif()
================================================
FILE: cmake/Platform.cmake
================================================
include_guard(GLOBAL)

if(WIN32)
    include(cmake/PlatformWindows.cmake)
else()
    include(cmake/PlatformLinux.cmake)
endif()
================================================
FILE: cmake/PlatformLinux.cmake
================================================
include_guard(GLOBAL)

set(FRANS_PLATFORM_LIBS pthread)
set(FRANS_PLATFORM_DEFS)
================================================
FILE: cmake/PlatformWindows.cmake
================================================
include_guard(GLOBAL)

set(FRANS_PLATFORM_LIBS ws2_32)
set(FRANS_PLATFORM_DEFS
    _WIN32_WINNT=0x0A00
    NOMINMAX
    WIN32_LEAN_AND_MEAN)
================================================
FILE: cmake/ProjectOptions.cmake
================================================
include_guard(GLOBAL)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(FRANS_VERSION="${PROJECT_VERSION}")
================================================
FILE: cmake/Sources.cmake
================================================
include_guard(GLOBAL)
set(FRANS_SOURCES
src/main.cpp
# UI
src/ui/MainWindow.cpp
src/ui/ManualImportDialog.cpp
src/ui/PromptDialog.cpp
src/ui/IngestDialog.cpp
# Application -- import
src/application/import/MultiFileParser.cpp
# Application -- manual_import
src/application/manual_import/ManualImporter.cpp
# Application -- safety
src/application/safety/DangerCheckHandler.cpp
# Application -- prompt (Fase A #41)
src/application/prompt/PromptGenerator.cpp
# Application -- ai (Fase C #54, #56, #57, #58)
src/application/ai/ApiKeyProvider.cpp
src/application/ai/GeminiClient.cpp
src/application/ai/GeminiSchema.cpp
# Application -- context (Fase C #59, #60)
src/application/context/ContextSelector.cpp
src/application/context/SourceCodeBuilder.cpp
# Application -- build_runner
src/application/build_runner/BuildRunner.cpp
src/application/build_runner/ProjectDetector.cpp
# Fase A #44: ringkasan error untuk SessionState (murni, tanpa FLTK)
src/application/build_runner/BuildErrorSummary.cpp
src/application/build_runner/BuildSystems/CMakeBuildSystem.cpp
src/application/build_runner/BuildSystems/MakeBuildSystem.cpp
src/application/build_runner/BuildSystems/MesonBuildSystem.cpp
src/application/build_runner/BuildSystems/NpmBuildSystem.cpp
src/application/build_runner/BuildSystems/PoetryBuildSystem.cpp
src/application/build_runner/BuildSystems/StandaloneBuildSystem.cpp
# Application -- ingest (Fase B #46, #48, #50)
src/application/ingest/FileCollector.cpp
src/application/ingest/MarkdownRenderer.cpp
src/application/ingest/RepoIngester.cpp
# Infrastructure
src/infrastructure/fs/SandboxedFileSystem.cpp
src/infrastructure/system/DiffEngine.cpp
# Infrastructure -- http (Fase C #55)
src/infrastructure/http/CurlTransport.cpp
# Config
src/config/AppConfig.cpp
src/config/AppPaths.cpp
)
if(WIN32)
list(APPEND FRANS_SOURCES
src/infrastructure/system/ProcessExecutor_win32.cpp
)
else()
list(APPEND FRANS_SOURCES
src/infrastructure/system/ProcessExecutor_posix.cpp
)
endif()
add_executable(frans ${FRANS_SOURCES})
target_include_directories(frans PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(frans PRIVATE
FLTK::FLTK
Threads::Threads
nlohmann_json::nlohmann_json
${FRANS_PLATFORM_LIBS}
frans_warnings
)
if(FRANS_PLATFORM_DEFS)
target_compile_definitions(frans PRIVATE ${FRANS_PLATFORM_DEFS})
endif()
if(WIN32)
set_target_properties(frans PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
================================================
FILE: cmake/Tests.cmake
================================================
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
================================================
FILE: src/infrastructure/system/ProcessExecutor.h
================================================
#pragma once

#include <atomic>
#include <string>
#include <vector>

namespace frans::infrastructure {

struct ProcessResult {
    int  exit_code = -1;
    bool timed_out = false;
    bool cancelled_by_user = false;
    std::string stdout_text;
    std::string stderr_text;
};

// Jalankan argv (argv[0] di-resolve via PATH) dan tunggu selesai.
//
// - timeout_sec : >0 = batas detik; kalau lewat, proses di-kill.
// - stdin_data  : kalau tidak kosong, ditulis ke stdin child.
// - working_dir : kalau tidak kosong, child chdir ke sini SEBELUM exec.
// - cancel_flag : kalau non-null dan bernilai true, kill child.
ProcessResult runCommand(const std::vector<std::string>& argv,
                         int timeout_sec = 30,
                         const std::string& stdin_data = {},
                         const std::string& working_dir = {},
                         std::atomic<bool>* cancel_flag = nullptr);

} // namespace frans::infrastructure
================================================
FILE: src/infrastructure/system/ProcessExecutor_posix.cpp
================================================
#include "infrastructure/system/ProcessExecutor.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace frans::infrastructure {

namespace {

void close_if_valid(int fd) {
    if (fd >= 0) ::close(fd);
}

std::string argvToStr(const std::vector<std::string>& argv) {
    std::string s;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) s += ' ';
        s += argv[i];
    }
    return s;
}

} // namespace

ProcessResult runCommand(const std::vector<std::string>& argv,
                         int timeout_sec,
                         const std::string& stdin_data,
                         const std::string& working_dir,
                         std::atomic<bool>* cancel_flag) {
    ProcessResult result;
    if (argv.empty()) {
        result.stderr_text = "runCommand: argv kosong";
        return result;
    }

    const std::string cmd_str = argvToStr(argv);
    std::fprintf(stderr,
        "[frans] runCommand START: %s (cwd=%s, timeout=%ds)\n",
        cmd_str.c_str(), working_dir.c_str(), timeout_sec);
    std::fflush(stderr);

    int in_pipe[2]  = {-1, -1};
    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};

    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        result.stderr_text = std::string("pipe: ") + std::strerror(errno);
        return result;
    }
    if (!stdin_data.empty() && pipe(in_pipe) != 0) {
        result.stderr_text = std::string("pipe(stdin): ") + std::strerror(errno);
        close_if_valid(out_pipe[0]); close_if_valid(out_pipe[1]);
        close_if_valid(err_pipe[0]); close_if_valid(err_pipe[1]);
        return result;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        result.stderr_text = std::string("fork: ") + std::strerror(errno);
        close_if_valid(in_pipe[0]);  close_if_valid(in_pipe[1]);
        close_if_valid(out_pipe[0]); close_if_valid(out_pipe[1]);
        close_if_valid(err_pipe[0]); close_if_valid(err_pipe[1]);
        return result;
    }

    if (pid == 0) {
        if (!stdin_data.empty()) {
            ::dup2(in_pipe[0], STDIN_FILENO);
            close_if_valid(in_pipe[0]);
            close_if_valid(in_pipe[1]);
        } else {
            const int devnull = ::open("/dev/null", O_RDONLY);
            if (devnull >= 0) { ::dup2(devnull, STDIN_FILENO); ::close(devnull); }
        }

        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::dup2(err_pipe[1], STDERR_FILENO);
        close_if_valid(out_pipe[0]); close_if_valid(out_pipe[1]);
        close_if_valid(err_pipe[0]); close_if_valid(err_pipe[1]);

        if (!working_dir.empty()) {
            if (::chdir(working_dir.c_str()) != 0) {
                std::fprintf(stderr, "chdir(%s): %s\n",
                             working_dir.c_str(), std::strerror(errno));
                ::_exit(126);
            }
        }

        std::vector<char*> c_argv;
        c_argv.reserve(argv.size() + 1);
        for (const auto& s : argv) c_argv.push_back(const_cast<char*>(s.c_str()));
        c_argv.push_back(nullptr);

        ::execvp(c_argv[0], c_argv.data());

        std::fprintf(stderr, "execvp(%s): %s\n", c_argv[0], std::strerror(errno));
        ::_exit(127);
    }

    close_if_valid(out_pipe[1]); out_pipe[1] = -1;
    close_if_valid(err_pipe[1]); err_pipe[1] = -1;

    if (!stdin_data.empty()) {
        close_if_valid(in_pipe[0]); in_pipe[0] = -1;
        const char* p = stdin_data.data();
        std::size_t remaining = stdin_data.size();
        while (remaining > 0) {
            const ssize_t w = ::write(in_pipe[1], p, remaining);
            if (w < 0) { if (errno == EINTR) continue; break; }
            p += w;
            remaining -= static_cast<std::size_t>(w);
        }
        close_if_valid(in_pipe[1]); in_pipe[1] = -1;
    }

    (void)::fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    (void)::fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

    const int effective_timeout = timeout_sec > 0 ? timeout_sec : 30;
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::seconds(effective_timeout);

    bool out_open = true;
    bool err_open = true;
    bool killed = false;
    char buf[4096];

    while (out_open || err_open) {
        if (!killed && cancel_flag && cancel_flag->load()) {
            result.cancelled_by_user = true;
            ::kill(pid, SIGKILL);
            killed = true;
        }

        if (!killed && std::chrono::steady_clock::now() > deadline) {
            result.timed_out = true;
            ::kill(pid, SIGKILL);
            killed = true;
        }

        struct pollfd fds[2];
        int nfds = 0;
        int idx_out = -1, idx_err = -1;
        if (out_open) { fds[nfds].fd = out_pipe[0]; fds[nfds].events = POLLIN; idx_out = nfds++; }
        if (err_open) { fds[nfds].fd = err_pipe[0]; fds[nfds].events = POLLIN; idx_err = nfds++; }

        const int p = ::poll(fds, static_cast<nfds_t>(nfds), 200);
        if (p < 0) { if (errno == EINTR) continue; break; }
        if (p == 0) continue;

        for (int i = 0; i < nfds; ++i) {
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            const ssize_t r = ::read(fds[i].fd, buf, sizeof(buf));
            if (r > 0) {
                if (i == idx_out)      result.stdout_text.append(buf, static_cast<std::size_t>(r));
                else if (i == idx_err) result.stderr_text.append(buf, static_cast<std::size_t>(r));
            } else if (r == 0) {
                if (i == idx_out)      out_open = false;
                else if (i == idx_err) err_open = false;
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    if (i == idx_out)      out_open = false;
                    else if (i == idx_err) err_open = false;
                }
            }
        }
    }

    close_if_valid(out_pipe[0]);
    close_if_valid(err_pipe[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}

    if (WIFEXITED(status))         result.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))  result.exit_code = 128 + WTERMSIG(status);
    else                           result.exit_code = -1;

    std::fprintf(stderr, "[frans] runCommand DONE: %s -> exit=%d%s%s\n",
                 cmd_str.c_str(), result.exit_code,
                 result.timed_out ? " (TIMED OUT)" : "",
                 result.cancelled_by_user ? " (CANCELLED)" : "");
    std::fflush(stderr);

    return result;
}

} // namespace frans::infrastructure
================================================
FILE: src/infrastructure/system/ProcessExecutor_win32.cpp
================================================
// Implementasi Windows pakai CreateProcess + pipe.
// Catatan: belum diuji di Windows asli; perlu verifikasi saat test.
#include "infrastructure/system/ProcessExecutor.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace frans::infrastructure {

namespace {

std::string joinArgv(const std::vector<std::string>& argv) {
    std::string s;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) s += ' ';
        // Quote sederhana; tidak menangani embedded quote.
        if (argv[i].find(' ') != std::string::npos) {
            s += '"' + argv[i] + '"';
        } else {
            s += argv[i];
        }
    }
    return s;
}

std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
        static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
        static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
        static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
        static_cast<int>(s.size()), out.data(), n);
    return out;
}

} // namespace

ProcessResult runCommand(const std::vector<std::string>& argv,
                         int timeout_sec,
                         const std::string& stdin_data,
                         const std::string& working_dir,
                         std::atomic<bool>* cancel_flag) {
    ProcessResult result;
    if (argv.empty()) {
        result.stderr_text = "runCommand: argv kosong";
        return result;
    }

    const std::string cmd_str = joinArgv(argv);
    std::fprintf(stderr, "[frans] runCommand START: %s (cwd=%s)\n",
        cmd_str.c_str(), working_dir.c_str());
    std::fflush(stderr);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE out_r = nullptr, out_w = nullptr;
    HANDLE err_r = nullptr, err_w = nullptr;
    HANDLE in_r  = nullptr, in_w  = nullptr;

    if (!::CreatePipe(&out_r, &out_w, &sa, 0)) {
        result.stderr_text = "CreatePipe(stdout) gagal";
        return result;
    }
    if (!::CreatePipe(&err_r, &err_w, &sa, 0)) {
        ::CloseHandle(out_r); ::CloseHandle(out_w);
        result.stderr_text = "CreatePipe(stderr) gagal";
        return result;
    }
    if (!stdin_data.empty()) {
        if (!::CreatePipe(&in_r, &in_w, &sa, 0)) {
            ::CloseHandle(out_r); ::CloseHandle(out_w);
            ::CloseHandle(err_r); ::CloseHandle(err_w);
            result.stderr_text = "CreatePipe(stdin) gagal";
            return result;
        }
    }

    ::SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
    if (in_w) ::SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = out_w;
    si.hStdError  = err_w;
    si.hStdInput  = in_r ? in_r : ::GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

    std::wstring cmdw = utf8ToWide(cmd_str);
    std::vector<wchar_t> cmd_buf(cmdw.begin(), cmdw.end());
    cmd_buf.push_back(L'\0');

    std::wstring cwdw = working_dir.empty() ? std::wstring{}
                                             : utf8ToWide(working_dir);

    const BOOL ok = ::CreateProcessW(
        nullptr, cmd_buf.data(),
        nullptr, nullptr, TRUE, 0,
        nullptr,
        cwdw.empty() ? nullptr : cwdw.c_str(),
        &si, &pi);

    ::CloseHandle(out_w);
    ::CloseHandle(err_w);
    if (in_r) ::CloseHandle(in_r);

    if (!ok) {
        ::CloseHandle(out_r); ::CloseHandle(err_r);
        if (in_w) ::CloseHandle(in_w);
        result.stderr_text = "CreateProcess gagal";
        return result;
    }

    // Kirim stdin kalau ada.
    if (!stdin_data.empty() && in_w) {
        DWORD written = 0;
        ::WriteFile(in_w, stdin_data.data(),
                    static_cast<DWORD>(stdin_data.size()), &written, nullptr);
        ::CloseHandle(in_w);
    }

    // Baca stdout + stderr.
    auto readPipe = [](HANDLE h, std::string& out) {
        char buf[4096];
        DWORD r = 0;
        while (::ReadFile(h, buf, sizeof(buf), &r, nullptr) && r > 0) {
            out.append(buf, r);
        }
    };

    std::thread t_out([&]{ readPipe(out_r, result.stdout_text); });
    std::thread t_err([&]{ readPipe(err_r, result.stderr_text); });

    const DWORD timeout_ms = timeout_sec > 0
        ? static_cast<DWORD>(timeout_sec) * 1000u
        : 30000u;

    const auto start = std::chrono::steady_clock::now();
    bool killed = false;

    for (;;) {
        const DWORD w = ::WaitForSingleObject(pi.hProcess, 200);
        if (w == WAIT_OBJECT_0) break;

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (static_cast<DWORD>(elapsed) >= timeout_ms) {
            result.timed_out = true;
            ::TerminateProcess(pi.hProcess, 1);
            killed = true;
            break;
        }
        if (cancel_flag && cancel_flag->load()) {
            result.cancelled_by_user = true;
            ::TerminateProcess(pi.hProcess, 1);
            killed = true;
            break;
        }
    }

    if (!killed) ::WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD ec = 0;
    if (::GetExitCodeProcess(pi.hProcess, &ec)) {
        result.exit_code = static_cast<int>(ec);
    }

    // Tutup pipe supaya thread pembaca selesai.
    ::CloseHandle(out_r);
    ::CloseHandle(err_r);
    t_out.join();
    t_err.join();

    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);

    std::fprintf(stderr, "[frans] runCommand DONE: %s -> exit=%d\n",
        cmd_str.c_str(), result.exit_code);
    std::fflush(stderr);
    return result;
}

} // namespace frans::infrastructure
