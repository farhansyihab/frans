# SourceCode.md: frans
Dibuat oleh Frans | 2026-11-15 09:40 | 17 file disertakan | 57.4 KB
Permintaan: Ada error ketika compile "Fase D: Git" bagian no 70 "GitDialog`: status, Add, Commit, Push (+ konfirmasi, peringatan file mencurigakan)"
fix error
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
    │       ├── GitDialog.cpp *
    │       ├── GitDialog.h *
    │       ├── IngestDialog.cpp
    │       ├── IngestDialog.h
    │       ├── MainWindow.cpp
    │       ├── MainWindow.h
    │       ├── ManualImportDialog.cpp
    │       ├── ManualImportDialog.h
    │       ├── PromptDialog.cpp
    │       ├── PromptDialog.h
    │       ├── PromptDialog_Actions.cpp
    │       ├── PromptDialog_Callbacks.cpp
    │       ├── PromptDialog_Workers.cpp
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
    │   │   ├── DangerCheckGitTest.cpp
    │   │   └── GitFacadeTest.cpp
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
    src/ui/PromptDialog_Actions.cpp
    src/ui/PromptDialog_Workers.cpp
    src/ui/PromptDialog_Callbacks.cpp
    src/ui/IngestDialog.cpp
    src/ui/GitDialog.cpp

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

    # Application -- git (Fase D #67, #68, #70)
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
    #
    # Runner test scenario untuk Manual Import + Danger Check + Sandbox
    # + error parser (traceback frame picking) + PromptGenerator (#42)
    # + FileCollector (#46) + MarkdownRenderer (#48).
    #
    #   frans_scenarios              -> jalankan semua scenario
    #   frans_scenarios --list       -> daftar nama scenario
    #   frans_scenarios --dump NAME  -> print scenario (paste ke GUI)
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
    #
    # Test ringan, TANPA FLTK dan TANPA ProsesExecutor:
    #   - summarizeBuildError()  : isi ringkasan error per phase
    #   - wasCancelledByUser()   : deteksi cancel
    #   - SessionState           : aturan publish / clear / skip-cancel
    #   - integrasi ke PromptGenerator (blok ERROR)
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
    #
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
    #
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
    #
    # SourceCodeBuilder:
    #   - fnv1a_64_hex         : panjang, lowercase, konsistensi, vektor known
    #   - buildManifestJson    : struktur JSON, escape karakter, files kosong
    #   - buildSourceCode      : happy path, unselected excluded, content
    #                           identik dengan disk, empty selection,
    #                           error input kosong, cancel flag
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
    # frans_git_facade_tests  (Fase D #67 + #68)
    #
    # GitFacade:
    #   - parseStatusPorcelain   : parse output git status --porcelain=v1 -b
    #   - suggestCommitMessage   : generate commit message suggestion
    #   - status/addAll/commit/push : operasi git via ProcessExecutor
    #
    # Tanpa FLTK, tanpa jaringan, tanpa git binary (untuk pure tests).
    # Operasi git memerlukan ProcessExecutor (untuk runCommand).
    #------------------------------------------------------------------
    set(FRANS_GIT_FACADE_TEST_SOURCES
        tests/git/GitFacadeTest.cpp
        src/application/git/GitFacade.cpp
    )

    # Fase D #68: GitFacade memanggil runCommand() dari ProcessExecutor,
    # jadi kita perlu link implementasi ProcessExecutor agar tidak error linker.
    if(WIN32)
        list(APPEND FRANS_GIT_FACADE_TEST_SOURCES
            src/infrastructure/system/ProcessExecutor_win32.cpp)
    else()
        list(APPEND FRANS_GIT_FACADE_TEST_SOURCES
            src/infrastructure/system/ProcessExecutor_posix.cpp)
    endif()

    add_executable(frans_git_facade_tests ${FRANS_GIT_FACADE_TEST_SOURCES})
    target_include_directories(frans_git_facade_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(frans_git_facade_tests PRIVATE
        Threads::Threads
        ${FRANS_PLATFORM_LIBS}
        frans_warnings
    )
    if(FRANS_PLATFORM_DEFS)
        target_compile_definitions(frans_git_facade_tests PRIVATE ${FRANS_PLATFORM_DEFS})
    endif()
    add_test(NAME git_facade_tests COMMAND frans_git_facade_tests)

    #------------------------------------------------------------------
    # frans_danger_git_tests  (Fase D #69)
    #
    # DangerCheckHandler: pola git berbahaya (push --force, reset --hard,
    # clean -f, checkout ., restore ., config credential).
    #
    # Tanpa FLTK, tanpa jaringan, tanpa git binary.
    # Hanya test logika pattern matching.
    #------------------------------------------------------------------
    add_executable(frans_danger_git_tests
        tests/git/DangerCheckGitTest.cpp
        src/application/safety/DangerCheckHandler.cpp
    )
    target_include_directories(frans_danger_git_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(frans_danger_git_tests PRIVATE
        frans_warnings
    )
    add_test(NAME danger_git_tests COMMAND frans_danger_git_tests)

endif()
================================================
FILE: src/application/git/GitFacade.cpp
================================================
#include "application/git/GitFacade.h"
#include "infrastructure/system/ProcessExecutor.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace frans::application {

namespace {

std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) ++start;
    if (start == s.end()) return {};
    auto end = s.end();
    do { --end; } while (std::distance(start, end) > 0 &&
                         std::isspace(static_cast<unsigned char>(*end)));
    return std::string(start, end + 1);
}

std::string unquotePath(const std::string& path) {
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
        std::string result;
        result.reserve(path.size() - 2);
        for (size_t i = 1; i + 1 < path.size(); ++i) {
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

infrastructure::ProcessResult runGitCommand(
    const std::string& project_root,
    const std::vector<std::string>& args,
    std::atomic<bool>* cancel,
    int timeout_sec = 30)
{
    std::vector<std::string> argv = {"git"};
    argv.insert(argv.end(), args.begin(), args.end());
    
    std::vector<std::string> env_extra = {"GIT_TERMINAL_PROMPT=0"};
    
    return infrastructure::runCommand(
        argv,
        timeout_sec,
        {},  // stdin_data
        project_root,  // working_dir
        cancel,
        env_extra
    );
}

std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
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
            if (line.size() < 3 || line.substr(0, 3) != "## ") {
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
                    status.upstream = trim(rest.substr(0, bracket_pos));
                    std::string counts = rest.substr(bracket_pos + 1);
                    
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
        change.index_status    = line[0];
        change.worktree_status = line[1];
        
        std::string path_part = line.substr(3);
        
        // Renamed: "R  old -> new" — ambil path baru saja.
        auto arrow_pos = path_part.find(" -> ");
        if (arrow_pos != std::string::npos) {
            path_part = path_part.substr(arrow_pos + 4);
        }
        
        change.path       = unquotePath(trim(path_part));
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
    
    for (const auto& change : st.changes) {
        char s = change.index_status != ' ' ? change.index_status : change.worktree_status;
        switch (s) {
            case 'A': ++added;     break;
            case 'M': ++modified;  break;
            case 'D': ++deleted;   break;
            case 'R': ++renamed;   break;
            case '?': ++untracked; break;
            default: break;
        }
    }
    
    std::ostringstream msg;
    
    if (st.changes.size() == 1) {
        const auto& change = st.changes[0];
        char s = change.index_status != ' ' ? change.index_status : change.worktree_status;
        
        auto last_slash = change.path.find_last_of('/');
        std::string filename = (last_slash != std::string::npos)
            ? change.path.substr(last_slash + 1)
            : change.path;
        
        switch (s) {
            case 'A': msg << "Add "            << filename; break;
            case 'M': msg << "Update "         << filename; break;
            case 'D': msg << "Remove "         << filename; break;
            case 'R': msg << "Rename "         << filename; break;
            case '?': msg << "Add untracked "  << filename; break;
            default:  msg << "Update "         << filename; break;
        }
    } else {
        std::vector<std::string> actions;
        if (added     > 0) actions.push_back(std::to_string(added)     + " added");
        if (modified  > 0) actions.push_back(std::to_string(modified)  + " modified");
        if (deleted   > 0) actions.push_back(std::to_string(deleted)   + " deleted");
        if (renamed   > 0) actions.push_back(std::to_string(renamed)   + " renamed");
        if (untracked > 0) actions.push_back(std::to_string(untracked) + " untracked");
        
        std::ostringstream detail;
        detail << "Update " << st.changes.size() << " files (";
        for (size_t i = 0; i < actions.size(); ++i) {
            if (i > 0) detail << ", ";
            detail << actions[i];
        }
        detail << ")";
        
        // Conventional commit line length ~72 chars.
        if (detail.str().size() > 72) {
            msg << "Update " << st.changes.size() << " files";
        } else {
            msg << detail.str();
        }
    }
    
    return msg.str();
}

// ---------------------------------------------------------------------------
// Operasi git via ProcessExecutor — implementasi Fase D #68
// ---------------------------------------------------------------------------

GitStatus GitFacade::status(std::atomic<bool>* cancel) {
    GitStatus result;
    
    // Jalankan git status --porcelain=v1 -b
    auto cmd_result = runGitCommand(
        project_root_,
        {"status", "--porcelain=v1", "-b"},
        cancel
    );
    
    if (cmd_result.cancelled_by_user) {
        result.error = "Dibatalkan oleh pengguna";
        return result;
    }
    
    if (cmd_result.timed_out) {
        result.error = "Timeout saat menjalankan git status";
        return result;
    }
    
    // Cek exit code dan stderr untuk deteksi error
    if (cmd_result.exit_code != 0) {
        std::string stderr_lower = toLower(cmd_result.stderr_text);
        
        if (stderr_lower.find("not a git repository") != std::string::npos) {
            result.is_repo = false;
            result.git_available = true;
            result.error = "Bukan git repository: " + project_root_;
        } else if (stderr_lower.find("command not found") != std::string::npos ||
                   stderr_lower.find("no such file or directory") != std::string::npos ||
                   cmd_result.exit_code == 127) {
            result.git_available = false;
            result.error = "git tidak terinstal atau tidak ada di PATH";
        } else {
            result.error = "git status gagal: " + cmd_result.stderr_text;
        }
        return result;
    }
    
    // Parse output
    result = parseStatusPorcelain(cmd_result.stdout_text);
    result.git_available = true;
    
    return result;
}

GitOpResult GitFacade::addAll(std::atomic<bool>* cancel) {
    GitOpResult result;
    
    auto cmd_result = runGitCommand(
        project_root_,
        {"add", "-A"},
        cancel
    );
    
    result.exit_code = cmd_result.exit_code;
    result.output = cmd_result.stdout_text;
    result.error = cmd_result.stderr_text;
    result.cancelled = cmd_result.cancelled_by_user;
    
    if (cmd_result.timed_out) {
        result.ok = false;
        result.error = "Timeout saat menjalankan git add";
        return result;
    }
    
    if (cmd_result.cancelled_by_user) {
        result.ok = false;
        result.error = "Dibatalkan oleh pengguna";
        return result;
    }
    
    result.ok = (cmd_result.exit_code == 0);
    
    if (!result.ok && result.error.empty()) {
        result.error = "git add gagal dengan exit code " + std::to_string(cmd_result.exit_code);
    }
    
    return result;
}

GitOpResult GitFacade::commit(const std::string& message, std::atomic<bool>* cancel) {
    GitOpResult result;
    
    // Cek apakah ada perubahan yang di-stage
    auto check_result = runGitCommand(
        project_root_,
        {"diff", "--cached", "--name-only"},
        cancel,
        10
    );
    
    if (check_result.exit_code == 0) {
        std::string output = trim(check_result.stdout_text);
        
        if (output.empty()) {
            result.ok = false;
            result.error = "Tidak ada perubahan yang di-stage untuk di-commit. Gunakan 'git add' terlebih dahulu.";
            return result;
        }
    }
    
    auto cmd_result = runGitCommand(
        project_root_,
        {"commit", "-m", message},
        cancel
    );
    
    result.exit_code = cmd_result.exit_code;
    result.output = cmd_result.stdout_text;
    result.error = cmd_result.stderr_text;
    result.cancelled = cmd_result.cancelled_by_user;
    
    if (cmd_result.timed_out) {
        result.ok = false;
        result.error = "Timeout saat menjalankan git commit";
        return result;
    }
    
    if (cmd_result.cancelled_by_user) {
        result.ok = false;
        result.error = "Dibatalkan oleh pengguna";
        return result;
    }
    
    result.ok = (cmd_result.exit_code == 0);
    
    if (!result.ok && result.error.empty()) {
        result.error = "git commit gagal dengan exit code " + std::to_string(cmd_result.exit_code);
    }
    
    return result;
}

GitOpResult GitFacade::push(std::atomic<bool>* cancel) {
    GitOpResult result;
    
    auto cmd_result = runGitCommand(
        project_root_,
        {"push"},
        cancel,
        60  // push bisa lebih lama karena network
    );
    
    result.exit_code = cmd_result.exit_code;
    result.output = cmd_result.stdout_text;
    result.error = cmd_result.stderr_text;
    result.cancelled = cmd_result.cancelled_by_user;
    
    if (cmd_result.timed_out) {
        result.ok = false;
        result.error = "Timeout saat menjalankan git push";
        return result;
    }
    
    if (cmd_result.cancelled_by_user) {
        result.ok = false;
        result.error = "Dibatalkan oleh pengguna";
        return result;
    }
    
    result.ok = (cmd_result.exit_code == 0);
    
    if (!result.ok) {
        std::string error_lower = toLower(result.error);
        
        // Cek error spesifik
        if (error_lower.find("no upstream") != std::string::npos ||
            error_lower.find("has no upstream") != std::string::npos ||
            error_lower.find("set-upstream") != std::string::npos) {
            result.error = "Branch tidak memiliki upstream. Gunakan: git push -u origin <branch>";
        } else if (error_lower.find("authentication failed") != std::string::npos ||
                   error_lower.find("permission denied") != std::string::npos ||
                   error_lower.find("could not read username") != std::string::npos) {
            result.error = "Autentikasi gagal. Periksa kredensial git Anda (SSH key atau Git Credential Manager).";
        } else if (error_lower.find("rejected") != std::string::npos &&
                   error_lower.find("non-fast-forward") != std::string::npos) {
            result.error = "Push ditolak: branch remote lebih baru. Pull terlebih dahulu atau gunakan force push (tidak disarankan).";
        } else if (error_lower.find("everything up-to-date") != std::string::npos) {
            // Ini sebenarnya bukan error, tapi git mengembalikan exit code 0
            result.ok = true;
            result.error = "Everything up-to-date";
        } else if (result.error.empty()) {
            result.error = "git push gagal dengan exit code " + std::to_string(cmd_result.exit_code);
        }
    }
    
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
FILE: src/ui/GitDialog.cpp
================================================
// src/ui/GitDialog.cpp
#include "ui/GitDialog.h"

#include <FL/fl_ask.H>

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <utility>

namespace frans::ui {

namespace {

constexpr int kMinW = 720;
constexpr int kMinH = 560;

std::string formatStatusLine(const frans::application::GitStatus& st) {
    if (!st.is_repo) {
        return "Not a git repository";
    }
    std::ostringstream oss;
    oss << "Branch: " << (st.branch.empty() ? "(detached)" : st.branch);
    if (!st.upstream.empty()) {
        oss << "  |  Upstream: " << st.upstream;
    } else {
        oss << "  |  Upstream: (none)";
    }
    if (st.ahead > 0 || st.behind > 0) {
        oss << "  |  ahead " << st.ahead << ", behind " << st.behind;
    }
    oss << "  |  " << st.changes.size() << " change(s)";
    return oss.str();
}

char statusChar(char idx, char wt) {
    if (idx != ' ') return idx;
    if (wt != ' ') return wt;
    return ' ';
}

const char* statusColor(char idx, char wt) {
    char s = statusChar(idx, wt);
    switch (s) {
        case 'A': return "green";
        case 'M': return "blue";
        case 'D': return "red";
        case 'R': return "magenta";
        case '?': return "gray";
        default:  return "black";
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void GitDialog::show(const std::string& project_root,
                     SessionState* session,
                     const frans::config::AppConfig& cfg) {
    GitDialog dlg(project_root, session, cfg);
    dlg.createUI();
    dlg.win_->set_modal();
    dlg.win_->show();
    dlg.refreshStatus();
    while (dlg.win_->shown()) {
        Fl::wait();
    }
}

GitDialog::GitDialog(const std::string& project_root,
                     SessionState* session,
                     const frans::config::AppConfig& cfg)
    : project_root_(project_root),
      session_(session),
      cfg_(cfg) {
    git_ = std::make_unique<frans::application::GitFacade>(project_root_);
}

GitDialog::~GitDialog() {
    cancelWorker();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    if (win_) {
        win_->hide();
        delete win_;
        win_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

void GitDialog::createUI() {
    int sx = 0, sy = 0, sw = 0, sh = 0;
    Fl::screen_work_area(sx, sy, sw, sh);
    int ww = std::max(kMinW, std::min(sw - 80, 820));
    int wh = std::max(kMinH, std::min(sh - 80, 640));

    win_ = new Fl_Window(sx + 40, sy + 40, ww, wh, "Git — Frans");
    layout();
    win_->end();
    win_->resizable(win_);
    win_->size_range(kMinW, kMinH);
}

void GitDialog::layout() {
    const int pad = 12;
    const int gap = 8;
    const int ww  = win_->w();
    const int wh  = win_->h();

    int y = pad;

    // ---- Status label ----
    status_label_ = new Fl_Box(pad, y, ww - 2 * pad, 22,
                               "Loading git status...");
    status_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    status_label_->labelfont(FL_COURIER_BOLD);
    status_label_->labelsize(12);
    y += 26;

    // ---- Changes browser ----
    changes_browser_ = new Fl_Browser(pad, y, ww - 2 * pad, 180);
    changes_browser_->type(FL_MULTI_BROWSER);
    changes_browser_->textfont(FL_COURIER);
    changes_browser_->textsize(12);
    changes_browser_->tooltip("Daftar perubahan file. Baris merah = mencurigakan (secret-like).");
    y += 180 + gap;

    // ---- Commit message ----
    {
        Fl_Box* lbl = new Fl_Box(pad, y, 120, 24, "Commit message:");
        lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        lbl->labelsize(12);

        commit_message_input_ = new Fl_Input(pad + 120, y, ww - 2 * pad - 120, 24);
        commit_message_input_->textfont(FL_COURIER);
        commit_message_input_->textsize(12);
        commit_message_input_->tooltip("Pesan commit. Auto-filled dari status; boleh diedit.");
        y += 28;
    }

    // ---- Output area ----
    {
        Fl_Box* lbl = new Fl_Box(pad, y, 120, 20, "Output:");
        lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        lbl->labelsize(11);

        output_ = new Fl_Output(pad, y + 20, ww - 2 * pad, 140);
        output_->textfont(FL_COURIER);
        output_->textsize(11);
        output_->maximum_size(64 * 1024);
        y += 160 + gap;
    }

    // ---- Button row ----
    const int btn_h = 28;
    const int btn_w = 96;
    int x = pad;

    btn_refresh_ = new Fl_Button(x, y, btn_w, btn_h, "Refresh");
    btn_refresh_->callback(cbRefresh, this);
    btn_refresh_->tooltip("Muat ulang status dari git");
    x += btn_w + gap;

    btn_add_all_ = new Fl_Button(x, y, btn_w, btn_h, "Add All");
    btn_add_all_->callback(cbAddAll, this);
    btn_add_all_->tooltip("git add -A (stage semua perubahan)");
    x += btn_w + gap;

    btn_commit_ = new Fl_Button(x, y, btn_w, btn_h, "Commit");
    btn_commit_->callback(cbCommit, this);
    btn_commit_->tooltip("git commit -m <pesan>");
    x += btn_w + gap;

    btn_push_ = new Fl_Button(x, y, btn_w, btn_h, "Push");
    btn_push_->callback(cbPush, this);
    btn_push_->tooltip("git push (dengan konfirmasi)");
    x += btn_w + gap;

    // Spacer
    // Cancel + Close di kanan
    btn_cancel_ = new Fl_Button(ww - pad - 2 * btn_w - gap, y, btn_w, btn_h, "Cancel");
    btn_cancel_->callback(cbCancel, this);
    btn_cancel_->deactivate();

    btn_close_ = new Fl_Button(ww - pad - btn_w, y, btn_w, btn_h, "Close");
    btn_close_->callback(cbClose, this);
}

// ---------------------------------------------------------------------------
// Status / display helpers
// ---------------------------------------------------------------------------

void GitDialog::updateStatusDisplay() {
    if (!status_label_) return;
    if (!current_status_.error.empty() && !current_status_.is_repo) {
        std::string msg = "Error: " + current_status_.error;
        status_label_->copy_label(msg.c_str());
        return;
    }
    std::string line = formatStatusLine(current_status_);
    if (!current_status_.error.empty()) {
        line += "  |  ! " + current_status_.error;
    }
    status_label_->copy_label(line.c_str());
}

void GitDialog::updateChangesBrowser() {
    if (!changes_browser_) return;
    changes_browser_->clear();

    if (!current_status_.is_repo) {
        changes_browser_->add("(not a git repository)");
        return;
    }
    if (current_status_.changes.empty()) {
        changes_browser_->add("(working tree clean)");
        return;
    }

    for (const auto& ch : current_status_.changes) {
        char s = statusChar(ch.index_status, ch.worktree_status);
        std::ostringstream line;
        line << s << "   " << ch.path;
        if (ch.suspicious) {
            line << "   [!]";
        }
        changes_browser_->add(line.str().c_str());
        int idx = changes_browser_->size();
        if (ch.suspicious) {
            changes_browser_->textcolor(idx, FL_RED);
        } else {
            const char* color = statusColor(ch.index_status, ch.worktree_status);
            Fl_Color c = FL_FOREGROUND_COLOR;
            if (std::string(color) == "green")        c = fl_rgb_color(0, 140, 0);
            else if (std::string(color) == "blue")    c = fl_rgb_color(0, 80, 200);
            else if (std::string(color) == "red")     c = fl_rgb_color(200, 0, 0);
            else if (std::string(color) == "magenta") c = fl_rgb_color(160, 0, 160);
            else if (std::string(color) == "gray")    c = fl_rgb_color(120, 120, 120);
            changes_browser_->textcolor(idx, c);
        }
    }
}

void GitDialog::appendOutput(const std::string& text) {
    if (!output_) return;
    output_->insert(text.c_str());
    output_->position(static_cast<int>(output_->size()));
}

void GitDialog::clearOutput() {
    if (!output_) return;
    output_->value("");
}

void GitDialog::setButtonsEnabled(bool enabled) {
    if (btn_refresh_)  enabled ? btn_refresh_->activate()   : btn_refresh_->deactivate();
    if (btn_add_all_)  enabled ? btn_add_all_->activate()   : btn_add_all_->deactivate();
    if (btn_commit_)   enabled ? btn_commit_->activate()    : btn_commit_->deactivate();
    if (btn_push_)     enabled ? btn_push_->activate()      : btn_push_->deactivate();
    if (btn_cancel_)   enabled ? btn_cancel_->deactivate()  : btn_cancel_->activate();
    if (btn_close_)    enabled ? btn_close_->activate()     : btn_close_->deactivate();
}

bool GitDialog::hasSuspiciousFiles() const {
    for (const auto& ch : current_status_.changes) {
        if (ch.suspicious) return true;
    }
    return false;
}

std::string GitDialog::buildSuspiciousWarning() const {
    std::ostringstream oss;
    oss << "Peringatan: ada file yang terdeteksi mencurigakan "
           "(berpotensi secret/credential):\n\n";
    int count = 0;
    for (const auto& ch : current_status_.changes) {
        if (ch.suspicious) {
            oss << "  - " << ch.path << "\n";
            ++count;
            if (count >= 20) {
                oss << "  ... dan " << (current_status_.changes.size() - 20)
                    << " lainnya\n";
                break;
            }
        }
    }
    oss << "\nMelanjutkan 'git add -A' akan men-stage file-file tersebut "
           "dan berpotensi meng-commit secret ke repository.\n\n"
           "Yakin ingin melanjutkan?";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Worker infrastructure
// ---------------------------------------------------------------------------

void GitDialog::startWorker(std::function<void()> task,
                            std::function<void()> on_done) {
    if (worker_running_.load()) return;

    cancel_flag_.store(false);
    worker_running_.store(true);
    worker_callback_ = std::move(on_done);
    setButtonsEnabled(false);

    worker_thread_ = std::thread([this, t = std::move(task)]() {
        try {
            t();
        } catch (const std::exception& e) {
            appendOutput(std::string("\n[worker exception] ") + e.what() + "\n");
        } catch (...) {
            appendOutput("\n[worker exception] unknown\n");
        }
        worker_running_.store(false);
        Fl::awake(pollWorker, this);
    });
}

void GitDialog::cancelWorker() {
    cancel_flag_.store(true);
}

void GitDialog::onWorkerFinished() {
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    setButtonsEnabled(true);
    if (worker_callback_) {
        auto cb = std::move(worker_callback_);
        worker_callback_ = nullptr;
        cb();
    }
}

void GitDialog::pollWorker(void* data) {
    auto* self = static_cast<GitDialog*>(data);
    if (!self) return;
    if (!self->worker_running_.load()) {
        self->onWorkerFinished();
    } else {
        Fl::repeat_timeout(0.1, pollWorker, data);
    }
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void GitDialog::refreshStatus() {
    clearOutput();
    appendOutput("[git status] running...\n");

    startWorker(
        [this]() {
            current_status_ = git_->status(&cancel_flag_);
        },
        [this]() {
            updateStatusDisplay();
            updateChangesBrowser();

            if (!current_status_.is_repo) {
                appendOutput("[git status] " + current_status_.error + "\n");
                return;
            }
            if (!current_status_.git_available) {
                appendOutput("[git status] " + current_status_.error + "\n");
                return;
            }
            if (!current_status_.error.empty()) {
                appendOutput("[git status] warning: " + current_status_.error + "\n");
            } else {
                appendOutput("[git status] OK — " +
                             std::to_string(current_status_.changes.size()) +
                             " change(s)\n");
            }

            // Auto-fill commit message suggestion (only if input is empty
            // or still contains a previous suggestion).
            if (commit_message_input_ && commit_message_input_->size() == 0) {
                std::string suggestion =
                    frans::application::GitFacade::suggestCommitMessage(current_status_);
                commit_message_input_->value(suggestion.c_str());
            }
        }
    );
}

void GitDialog::doAddAll() {
    if (!current_status_.is_repo) {
        fl_message("Bukan git repository. Tidak bisa add.");
        return;
    }
    if (current_status_.changes.empty()) {
        fl_message("Tidak ada perubahan untuk di-stage.");
        return;
    }

    if (hasSuspiciousFiles()) {
        std::string warn = buildSuspiciousWarning();
        int choice = fl_choice(warn.c_str(), "Batal", "Tetap Add All", nullptr);
        if (choice != 1) {
            appendOutput("[git add] dibatalkan oleh pengguna (file mencurigakan).\n");
            return;
        }
    }

    appendOutput("[git add -A] running...\n");
    startWorker(
        [this]() {
            auto res = git_->addAll(&cancel_flag_);
            appendOutput(res.ok ? "[git add -A] OK\n"
                                : "[git add -A] FAILED: " + res.error + "\n");
            if (!res.output.empty()) appendOutput(res.output);
        },
        [this]() { refreshStatus(); }
    );
}

void GitDialog::doCommit() {
    if (!current_status_.is_repo) {
        fl_message("Bukan git repository.");
        return;
    }
    if (!commit_message_input_) return;

    std::string msg = commit_message_input_->value() ? commit_message_input_->value() : "";
    // Trim
    while (!msg.empty() && std::isspace(static_cast<unsigned char>(msg.back()))) msg.pop_back();
    while (!msg.empty() && std::isspace(static_cast<unsigned char>(msg.front()))) msg.erase(msg.begin());

    if (msg.empty()) {
        fl_message("Pesan commit tidak boleh kosong.");
        return;
    }

    appendOutput("[git commit] running...\n");
    startWorker(
        [this, msg]() {
            auto res = git_->commit(msg, &cancel_flag_);
            if (res.ok) {
                appendOutput("[git commit] OK\n");
            } else {
                appendOutput("[git commit] FAILED: " + res.error + "\n");
            }
            if (!res.output.empty()) appendOutput(res.output);
        },
        [this]() {
            refreshStatus();
            // Clear commit message after successful commit
            if (commit_message_input_) commit_message_input_->value("");
        }
    );
}

void GitDialog::doPush() {
    if (!current_status_.is_repo) {
        fl_message("Bukan git repository.");
        return;
    }

    // Build confirmation text
    std::ostringstream oss;
    oss << "Konfirmasi git push\n\n"
        << "  Branch:   " << (current_status_.branch.empty() ? "(detached)" : current_status_.branch) << "\n"
        << "  Upstream: " << (current_status_.upstream.empty() ? "(none)" : current_status_.upstream) << "\n"
        << "  Ahead:    " << current_status_.ahead << " commit(s)\n\n";

    if (current_status_.upstream.empty()) {
        oss << "PERHATIAN: branch ini belum memiliki upstream.\n"
               "Push mungkin gagal. Jika gagal, jalankan manual:\n"
               "  git push -u origin <branch>\n\n";
    }
    if (current_status_.ahead == 0) {
        oss << "Catatan: tidak ada commit ahead — push mungkin tidak melakukan apa-apa.\n\n";
    }
    oss << "Lanjutkan push?";

    int choice = fl_choice(oss.str().c_str(), "Batal", "Push", nullptr);
    if (choice != 1) {
        appendOutput("[git push] dibatalkan oleh pengguna.\n");
        return;
    }

    appendOutput("[git push] running...\n");
    startWorker(
        [this]() {
            auto res = git_->push(&cancel_flag_);
            if (res.ok) {
                appendOutput("[git push] OK\n");
            } else {
                appendOutput("[git push] FAILED: " + res.error + "\n");
            }
            if (!res.output.empty()) appendOutput(res.output);
        },
        [this]() { refreshStatus(); }
    );
}

// ---------------------------------------------------------------------------
// FLTK callbacks
// ---------------------------------------------------------------------------

void GitDialog::cbRefresh(Fl_Widget*, void* data) {
    static_cast<GitDialog*>(data)->refreshStatus();
}

void GitDialog::cbAddAll(Fl_Widget*, void* data) {
    static_cast<GitDialog*>(data)->doAddAll();
}

void GitDialog::cbCommit(Fl_Widget*, void* data) {
    static_cast<GitDialog*>(data)->doCommit();
}

void GitDialog::cbPush(Fl_Widget*, void* data) {
    static_cast<GitDialog*>(data)->doPush();
}

void GitDialog::cbCancel(Fl_Widget*, void* data) {
    static_cast<GitDialog*>(data)->cancelWorker();
}

void GitDialog::cbClose(Fl_Widget*, void* data) {
    auto* self = static_cast<GitDialog*>(data);
    if (self->worker_running_.load()) {
        int c = fl_choice("Operasi git masih berjalan. Batalkan dan tutup?",
                          "Tidak", "Ya", nullptr);
        if (c != 1) return;
        self->cancelWorker();
    }
    if (self->win_) self->win_->hide();
}

} // namespace frans::ui
================================================
FILE: src/ui/GitDialog.h
================================================
// src/ui/GitDialog.h
#pragma once

#include "config/AppConfig.h"
#include "ui/SessionState.h"
#include "application/git/GitFacade.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Window.H>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace frans::ui {

class GitDialog {
public:
    static void show(const std::string& project_root,
                     SessionState* session,
                     const frans::config::AppConfig& cfg);

private:
    GitDialog(const std::string& project_root,
              SessionState* session,
              const frans::config::AppConfig& cfg);
    ~GitDialog();

    GitDialog(const GitDialog&)            = delete;
    GitDialog& operator=(const GitDialog&) = delete;

    void createUI();
    void layout();

    void refreshStatus();
    void doAddAll();
    void doCommit();
    void doPush();

    void setButtonsEnabled(bool enabled);
    void appendOutput(const std::string& text);
    void clearOutput();
    void updateStatusDisplay();
    void updateChangesBrowser();

    bool hasSuspiciousFiles() const;
    std::string buildSuspiciousWarning() const;

    // Worker infrastructure (same pattern as PromptDialog)
    void startWorker(std::function<void()> task,
                     std::function<void()> on_done);
    void cancelWorker();
    void onWorkerFinished();
    static void pollWorker(void* data);

    // FLTK callbacks
    static void cbRefresh(Fl_Widget*, void*);
    static void cbAddAll(Fl_Widget*, void*);
    static void cbCommit(Fl_Widget*, void*);
    static void cbPush(Fl_Widget*, void*);
    static void cbCancel(Fl_Widget*, void*);
    static void cbClose(Fl_Widget*, void*);

    Fl_Window*    win_                 = nullptr;
    Fl_Box*       status_label_        = nullptr;
    Fl_Browser*   changes_browser_     = nullptr;
    Fl_Input*     commit_message_input_= nullptr;
    Fl_Output*    output_              = nullptr;

    Fl_Button*    btn_refresh_         = nullptr;
    Fl_Button*    btn_add_all_         = nullptr;
    Fl_Button*    btn_commit_          = nullptr;
    Fl_Button*    btn_push_            = nullptr;
    Fl_Button*    btn_cancel_          = nullptr;
    Fl_Button*    btn_close_           = nullptr;

    std::string   project_root_;
    SessionState* session_             = nullptr;
    frans::config::AppConfig cfg_;

    std::unique_ptr<frans::application::GitFacade> git_;
    frans::application::GitStatus current_status_;

    std::thread              worker_thread_;
    std::atomic<bool>        cancel_flag_{false};
    std::atomic<bool>        worker_running_{false};
    std::function<void()>    worker_callback_;
};

} // namespace frans::ui
