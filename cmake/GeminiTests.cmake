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
