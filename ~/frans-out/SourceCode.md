# SourceCode.md: frans
Dibuat oleh Frans | 2026-11-15 06:25 | 16 file disertakan | 37.5 KB
Permintaan: fix error yang terjadi ketika compile "Fase D: Git" bagian no 67 "GitFacade::parseStatusPorcelain` + `suggestCommitMessage` + test"
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
    │   │   ├── git/
    │   │   │   ├── GitFacade.cpp *
    │   │   │   └── GitFacade.h *
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
    │   │       ├── ProcessExecutor.h
    │   │       ├── ProcessExecutor_posix.cpp
    │   │       ├── ProcessExecutor_win32.cpp
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
    ├── test_env_extra.cpp
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
    │   ├── git/
    │   │   └── GitFacadeTest.cpp *
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

    # Application -- git (Fase D #67)
    src/application/git/GitFacade.cpp

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

    #------------------------------------------------------------------
    # frans_scenarios
    # Runner test scenario untuk Manual Import + Danger Check + Sandbox
    # + error parser (traceback frame picking) + PromptGenerator (#42)
    # + FileCollector (#46) + MarkdownRenderer (#48).
    #
    # frans_scenarios                -> jalankan semua scenario
    # frans_scenarios --list         -> daftar nama scenario
    # frans_scenarios --dump NAME    -> print scenario (paste ke GUI)
    #------------------------------------------------------------------
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

    #------------------------------------------------------------------
    # frans_session_tests  (Fase A #44 + #45)
    # Test ringan, TANPA FLTK dan TANPA ProsesExecutor:
    # - summarizeBuildError()  : isi ringkasan error per phase
    # - wasCancelledByUser()   : deteksi cancel
    # - SessionState           : aturan publish / clear / skip-cancel
    # - integrasi ke PromptGenerator (blok ERROR)
    #
    # Hanya 3 file .cpp -> compile cepat di mesin lama.
    #------------------------------------------------------------------
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

    #------------------------------------------------------------------
    # frans_apikey_tests  (Fase C #54)
    # ApiKeyProvider: env -> file kunci -> default ~/.gemini/api_key,
    # trim/BOM/CRLF, izin file longgar, masking 4 karakter terakhir,
    # dan pastikan diagnostik tidak membocorkan key.
    #
    # Test memakai HOME palsu di direktori temp; file kunci asli pengguna
    # tidak disentuh. Tanpa FLTK, tanpa jaringan, tanpa subprocess.
    #------------------------------------------------------------------
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

    #------------------------------------------------------------------
    # frans_gemini_schema_tests  (Fase C #57)
    # GeminiSchema: verifikasi struktur JSON schema untuk response Gemini
    # dan file prompt gemini_selector_prompt.txt ada serta valid.
    #
    # Tanpa FLTK, tanpa jaringan, tanpa subprocess.
    #------------------------------------------------------------------
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

    #------------------------------------------------------------------
    # frans_context_tests  (Fase C #60)
    # SourceCodeBuilder:
    # - fnv1a_64_hex         : panjang, lowercase, konsistensi, vektor known
    # - buildManifestJson    : struktur JSON, escape karakter, files kosong
    # - buildSourceCode      : happy path, unselected excluded, content
    #                          identik dengan disk, empty selection,
    #                          error input kosong, cancel flag
    #
    # Tanpa FLTK, tanpa jaringan, tanpa subprocess, tanpa git.
    # Memakai temp directory sebagai fixture (RAII, auto-cleanup).
    #------------------------------------------------------------------
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

    #------------------------------------------------------------------
    # frans_git_facade_tests  (Fase D #67)
    # GitFacade pure functions:
    # - parseStatusPorcelain   : parse output git status --porcelain=v1 -b
    # - suggestCommitMessage   : generate commit message suggestion
    #
    # Tanpa FLTK, tanpa jaringan, tanpa subprocess, tanpa git.
    #------------------------------------------------------------------
    add_executable(frans_git_facade_tests
        tests/git/GitFacadeTest.cpp
        src/application/git/GitFacade.cpp
    )

    target_include_directories(frans_git_facade_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(frans_git_facade_tests PRIVATE
        frans_warnings
    )

    add_test(NAME git_facade_tests COMMAND frans_git_facade_tests)

endif()
================================================
FILE: src/application/git/GitFacade.cpp
================================================
#include "application/git/GitFacade.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace frans::application {

namespace {

std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) ++start;
    auto end = s.end();
    do { --end; } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

std::string unquotePath(const std::string& path) {
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
        std::string result;
        result.reserve(path.size() - 2);
        for (size_t i = 1; i < path.size() - 1; ++i) {
            if (path[i] == '\\' && i + 1 < path.size() - 1) {
                char next = path[i + 1];
                if (next == '"' || next == '\\' || next == 'n' || next == 't') {
                    if (next == 'n') result += '\n';
                    else if (next == 't') result += '\t';
                    else result += next;
                    ++i;
                    continue;
                }
            }
            result += path[i];
        }
        return result;
    }
    return path;
}

bool isSuspiciousPath(const std::string& path) {
    static const std::vector<std::string> suspicious_patterns = {
        ".env", ".env.", "credentials", "secret", "password",
        "private_key", "id_rsa", "id_ed25519", ".pem", ".key"
    };
    
    std::string lower_path = path;
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    for (const auto& pattern : suspicious_patterns) {
        if (lower_path.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

GitFacade::GitFacade(std::string project_root)
    : project_root_(std::move(project_root)) {}

GitStatus GitFacade::parseStatusPorcelain(const std::string& text) {
    GitStatus status;
    
    if (text.empty()) {
        status.error = "Empty status output";
        return status;
    }
    
    std::istringstream stream(text);
    std::string line;
    bool first_line = true;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        if (first_line) {
            first_line = false;
            
            if (line.substr(0, 3) != "## ") {
                status.error = "Invalid porcelain format: missing branch header";
                return status;
            }
            
            status.is_repo = true;
            
            std::string branch_info = line.substr(3);
            
            auto dots_pos = branch_info.find("...");
            if (dots_pos != std::string::npos) {
                status.branch = branch_info.substr(0, dots_pos);
                std::string rest = branch_info.substr(dots_pos + 3);
                
                auto bracket_pos = rest.find('[');
                if (bracket_pos != std::string::npos) {
                    status.upstream = rest.substr(0, bracket_pos);
                    std::string counts = rest.substr(bracket_pos + 1);
                    
                    auto ahead_match = std::regex_search(counts, std::regex("ahead (\\d+)"));
                    auto behind_match = std::regex_search(counts, std::regex("behind (\\d+)"));
                    
                    std::smatch match;
                    if (std::regex_search(counts, match, std::regex("ahead (\\d+)"))) {
                        status.ahead = std::stoi(match[1].str());
                    }
                    if (std::regex_search(counts, match, std::regex("behind (\\d+)"))) {
                        status.behind = std::stoi(match[1].str());
                    }
                } else {
                    status.upstream = trim(rest);
                }
            } else {
                status.branch = trim(branch_info);
            }
            
            continue;
        }
        
        if (line.size() < 3) continue;
        
        GitFileChange change;
        change.index_status = line[0];
        change.worktree_status = line[1];
        
        std::string path_part = line.substr(3);
        
        auto arrow_pos = path_part.find(" -> ");
        if (arrow_pos != std::string::npos) {
            path_part = path_part.substr(arrow_pos + 4);
        }
        
        change.path = unquotePath(trim(path_part));
        change.suspicious = isSuspiciousPath(change.path);
        
        status.changes.push_back(std::move(change));
    }
    
    return status;
}

std::string GitFacade::suggestCommitMessage(const GitStatus& st) {
    if (st.changes.empty()) {
        return "No changes to commit";
    }
    
    int added = 0, modified = 0, deleted = 0, renamed = 0, untracked = 0;
    std::vector<std::string> file_names;
    
    for (const auto& change : st.changes) {
        char status = change.index_status != ' ' ? change.index_status : change.worktree_status;
        
        switch (status) {
            case 'A': ++added; break;
            case 'M': ++modified; break;
            case 'D': ++deleted; break;
            case 'R': ++renamed; break;
            case '?': ++untracked; break;
            default: break;
        }
        
        if (file_names.size() < 3) {
            auto last_slash = change.path.find_last_of('/');
            std::string filename = (last_slash != std::string::npos) 
                ? change.path.substr(last_slash + 1) 
                : change.path;
            file_names.push_back(filename);
        }
    }
    
    std::ostringstream msg;
    
    if (st.changes.size() == 1) {
        const auto& change = st.changes[0];
        char status = change.index_status != ' ' ? change.index_status : change.worktree_status;
        
        auto last_slash = change.path.find_last_of('/');
        std::string filename = (last_slash != std::string::npos) 
            ? change.path.substr(last_slash + 1) 
            : change.path;
        
        switch (status) {
            case 'A': msg << "Add " << filename; break;
            case 'M': msg << "Update " << filename; break;
            case 'D': msg << "Remove " << filename; break;
            case 'R': msg << "Rename " << filename; break;
            case '?': msg << "Add untracked " << filename; break;
            default:  msg << "Update " << filename; break;
        }
    } else {
        std::vector<std::string> actions;
        if (added > 0) actions.push_back(std::to_string(added) + " added");
        if (modified > 0) actions.push_back(std::to_string(modified) + " modified");
        if (deleted > 0) actions.push_back(std::to_string(deleted) + " deleted");
        if (renamed > 0) actions.push_back(std::to_string(renamed) + " renamed");
        if (untracked > 0) actions.push_back(std::to_string(untracked) + " untracked");
        
        msg << "Update " << st.changes.size() << " files (";
        for (size_t i = 0; i < actions.size(); ++i) {
            if (i > 0) msg << ", ";
            msg << actions[i];
        }
        msg << ")";
        
        if (msg.str().size() > 72) {
            msg.str("");
            msg << "Update " << st.changes.size() << " files";
        }
    }
    
    return msg.str();
}

GitStatus GitFacade::status(std::atomic<bool>* /*cancel*/) {
    GitStatus result;
    result.error = "Not implemented in Fase D #67 (pure functions only)";
    return result;
}

GitOpResult GitFacade::addAll(std::atomic<bool>* /*cancel*/) {
    GitOpResult result;
    result.error = "Not implemented in Fase D #67";
    return result;
}

GitOpResult GitFacade::commit(const std::string& /*message*/, std::atomic<bool>* /*cancel*/) {
    GitOpResult result;
    result.error = "Not implemented in Fase D #67";
    return result;
}

GitOpResult GitFacade::push(std::atomic<bool>* /*cancel*/) {
    GitOpResult result;
    result.error = "Not implemented in Fase D #67";
    return result;
}

} // namespace frans::application
================================================
FILE: src/application/git/GitFacade.h
================================================
#pragma once
#include <atomic>
#include <string>
#include <vector>

namespace frans::application {

struct GitFileChange {
    char index_status = ' ';
    char worktree_status = ' ';
    std::string path;
    bool suspicious = false;
};

struct GitStatus {
    bool is_repo = false;
    bool git_available = true;
    std::string branch;
    std::string upstream;
    std::string error;
    int ahead = 0;
    int behind = 0;
    std::vector<GitFileChange> changes;
    
    bool clean() const { return is_repo && changes.empty(); }
};

struct GitOpResult {
    bool ok = false;
    int exit_code = -1;
    bool cancelled = false;
    std::string output;
    std::string error;
};

class GitFacade {
public:
    explicit GitFacade(std::string project_root);
    
    GitStatus   status(std::atomic<bool>* cancel = nullptr);
    GitOpResult addAll(std::atomic<bool>* cancel = nullptr);
    GitOpResult commit(const std::string& message, std::atomic<bool>* cancel = nullptr);
    GitOpResult push(std::atomic<bool>* cancel = nullptr);
    
    static GitStatus   parseStatusPorcelain(const std::string& text);
    static std::string suggestCommitMessage(const GitStatus& st);

private:
    std::string project_root_;
};

} // namespace frans::application
================================================
FILE: tests/git/GitFacadeTest.cpp
================================================
#include "application/git/GitFacade.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace frans::application;

void test_parseStatusPorcelain_clean() {
    std::string input = "## main...origin/main\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(status.is_repo);
    assert(status.branch == "main");
    assert(status.upstream == "origin/main");
    assert(status.ahead == 0);
    assert(status.behind == 0);
    assert(status.changes.empty());
    assert(status.clean());
    
    std::cout << "PASS: test_parseStatusPorcelain_clean\n";
}

void test_parseStatusPorcelain_ahead_behind() {
    std::string input = "## feature...origin/feature [ahead 3, behind 2]\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(status.is_repo);
    assert(status.branch == "feature");
    assert(status.upstream == "origin/feature");
    assert(status.ahead == 3);
    assert(status.behind == 2);
    
    std::cout << "PASS: test_parseStatusPorcelain_ahead_behind\n";
}

void test_parseStatusPorcelain_no_upstream() {
    std::string input = "## new-branch\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(status.is_repo);
    assert(status.branch == "new-branch");
    assert(status.upstream.empty());
    
    std::cout << "PASS: test_parseStatusPorcelain_no_upstream\n";
}

void test_parseStatusPorcelain_modified() {
    std::string input = 
        "## main\n"
        " M src/main.cpp\n"
        "M  src/util.cpp\n";
    
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(status.is_repo);
    assert(status.changes.size() == 2);
    
    assert(status.changes[0].index_status == ' ');
    assert(status.changes[0].worktree_status == 'M');
    assert(status.changes[0].path == "src/main.cpp");
    
    assert(status.changes[1].index_status == 'M');
    assert(status.changes[1].worktree_status == ' ');
    assert(status.changes[1].path == "src/util.cpp");
    
    std::cout << "PASS: test_parseStatusPorcelain_modified\n";
}

void test_parseStatusPorcelain_added() {
    std::string input = 
        "## main\n"
        "A  new_file.cpp\n"
        "?? untracked.txt\n";
    
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(status.changes.size() == 2);
    
    assert(status.changes[0].index_status == 'A');
    assert(status.changes[0].path == "new_file.cpp");
    
    assert(status.changes[1].index_status == '?');
    assert(status.changes[1].worktree_status == '?');
    assert(status.changes[1].path == "untracked.txt");
    
    std::cout << "PASS: test_parseStatusPorcelain_added\n";
}

void test_parseStatusPorcelain_deleted() {
    std::string input = 
        "## main\n"
        "D  old_file.cpp\n";
    
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(status.changes.size() == 1);
    assert(status.changes[0].index_status == 'D');
    assert(status.changes[0].path == "old_file.cpp");
    
    std::cout << "PASS: test_parseStatusPorcelain_deleted\n";
}

void test_parseStatusPorcelain_renamed() {
    std::string input = 
        "## main\n"
        "R  old_name.cpp -> new_name.cpp\n";
    
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(status.changes.size() == 1);
    assert(status.changes[0].index_status == 'R');
    assert(status.changes[0].path == "new_name.cpp");
    
    std::cout << "PASS: test_parseStatusPorcelain_renamed\n";
}

void test_parseStatusPorcelain_quoted_path() {
    std::string input = 
        "## main\n"
        " M \"file with spaces.cpp\"\n";
    
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(status.changes.size() == 1);
    assert(status.changes[0].path == "file with spaces.cpp");
    
    std::cout << "PASS: test_parseStatusPorcelain_quoted_path\n";
}

void test_parseStatusPorcelain_suspicious() {
    std::string input = 
        "## main\n"
        "A  .env\n"
        "A  credentials.json\n"
        "A  normal_file.cpp\n";
    
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(status.changes.size() == 3);
    assert(status.changes[0].suspicious == true);
    assert(status.changes[1].suspicious == true);
    assert(status.changes[2].suspicious == false);
    
    std::cout << "PASS: test_parseStatusPorcelain_suspicious\n";
}

void test_parseStatusPorcelain_empty() {
    std::string input = "";
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(!status.is_repo);
    assert(!status.error.empty());
    
    std::cout << "PASS: test_parseStatusPorcelain_empty\n";
}

void test_parseStatusPorcelain_invalid() {
    std::string input = "Not a git repo\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    
    assert(!status.is_repo);
    assert(!status.error.empty());
    
    std::cout << "PASS: test_parseStatusPorcelain_invalid\n";
}

void test_suggestCommitMessage_clean() {
    GitStatus status;
    status.is_repo = true;
    
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg == "No changes to commit");
    
    std::cout << "PASS: test_suggestCommitMessage_clean\n";
}

void test_suggestCommitMessage_single_added() {
    GitStatus status;
    status.is_repo = true;
    status.changes.push_back({'A', ' ', "new_file.cpp", false});
    
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg == "Add new_file.cpp");
    
    std::cout << "PASS: test_suggestCommitMessage_single_added\n";
}

void test_suggestCommitMessage_single_modified() {
    GitStatus status;
    status.is_repo = true;
    status.changes.push_back({'M', ' ', "src/main.cpp", false});
    
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg == "Update main.cpp");
    
    std::cout << "PASS: test_suggestCommitMessage_single_modified\n";
}

void test_suggestCommitMessage_single_deleted() {
    GitStatus status;
    status.is_repo = true;
    status.changes.push_back({'D', ' ', "old_file.cpp", false});
    
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg == "Remove old_file.cpp");
    
    std::cout << "PASS: test_suggestCommitMessage_single_deleted\n";
}

void test_suggestCommitMessage_multiple() {
    GitStatus status;
    status.is_repo = true;
    status.changes.push_back({'A', ' ', "file1.cpp", false});
    status.changes.push_back({'M', ' ', "file2.cpp", false});
    status.changes.push_back({'D', ' ', "file3.cpp", false});
    
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg.find("Update 3 files") != std::string::npos);
    assert(msg.find("1 added") != std::string::npos);
    assert(msg.find("1 modified") != std::string::npos);
    assert(msg.find("1 deleted") != std::string::npos);
    
    std::cout << "PASS: test_suggestCommitMessage_multiple\n";
}

void test_suggestCommitMessage_many_files() {
    GitStatus status;
    status.is_repo = true;
    for (int i = 0; i < 20; ++i) {
        status.changes.push_back({'M', ' ', "file" + std::to_string(i) + ".cpp", false});
    }
    
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg == "Update 20 files");
    
    std::cout << "PASS: test_suggestCommitMessage_many_files\n";
}

int main() {
    std::cout << "Running GitFacade tests...\n\n";
    
    test_parseStatusPorcelain_clean();
    test_parseStatusPorcelain_ahead_behind();
    test_parseStatusPorcelain_no_upstream();
    test_parseStatusPorcelain_modified();
    test_parseStatusPorcelain_added();
    test_parseStatusPorcelain_deleted();
    test_parseStatusPorcelain_renamed();
    test_parseStatusPorcelain_quoted_path();
    test_parseStatusPorcelain_suspicious();
    test_parseStatusPorcelain_empty();
    test_parseStatusPorcelain_invalid();
    
    test_suggestCommitMessage_clean();
    test_suggestCommitMessage_single_added();
    test_suggestCommitMessage_single_modified();
    test_suggestCommitMessage_single_deleted();
    test_suggestCommitMessage_multiple();
    test_suggestCommitMessage_many_files();
    
    std::cout << "\nAll tests passed!\n";
    return 0;
}
