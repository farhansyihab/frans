# ~/AgenAI/frans/cmake/Dependencies.cmake
#
# find_package untuk semua dependency eksternal.
# Set variabel global:
#   FRANS_HTTPLIB_DEFS    — CPPHTTPLIB_OPENSSL_SUPPORT (kosong kalau no OpenSSL)
#   FRANS_OPENSSL_LIBS    — OpenSSL::SSL + OpenSSL::Crypto (kosong kalau no)
include_guard(GLOBAL)

find_package(FLTK REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# OpenSSL (opsional) — Stage 8 + 12
# ---------------------------------------------------------------------------
# Diperlukan untuk DeepSeekProvider + GeminiProvider (HTTPS).
# Kalau tidak ada, build tetap sukses, tapi kedua provider akan fail
# runtime saat memanggil HTTPS endpoint.
find_package(OpenSSL QUIET)

set(FRANS_HTTPLIB_DEFS)
set(FRANS_OPENSSL_LIBS)

if(OpenSSL_FOUND)
    message(STATUS "OpenSSL ditemukan — HTTPS (DeepSeekProvider + GeminiProvider) enabled")
    list(APPEND FRANS_HTTPLIB_DEFS CPPHTTPLIB_OPENSSL_SUPPORT)
    list(APPEND FRANS_OPENSSL_LIBS OpenSSL::SSL OpenSSL::Crypto)
else()
    message(WARNING
        "OpenSSL TIDAK ditemukan — DeepSeekProvider/GeminiProvider (HTTPS) akan fail runtime.\n"
        "  Install: sudo apt install libssl-dev\n"
        "  Lalu reconfigure & rebuild.")
endif()