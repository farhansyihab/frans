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