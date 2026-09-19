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