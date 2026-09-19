# Frans — AI Code Importer & Build Runner

Tool ringan (C++20 + FLTK) untuk:
1. Paste output multi-file dari AI chat browser (DeepSeek / Gemini / Claude / ChatGPT).
2. Preview status tiap file (New / Overwrite / Blocked).
3. Tulis ke project root dengan safety layer (sandbox + danger check).
4. Deteksi build system dari marker file (CMake / Make / Meson / npm / Poetry / Standalone).
5. Build → Test → Run, dengan error file:line.

## Build

### Lubuntu
    sudo apt install build-essential cmake libfltk1.3-dev
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j

### Windows (MSVC)
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release

## Jalankan

    ./build/bin/frans        # Linux
    build\bin\Release\frans  # Windows

## Format paste yang didukung

    File: relative/path.py
    [fenced code block]
    <content>

    File: another.py
    [fenced code block]
    <content>

## Integrasi Desktop

### Linux — user-level (tanpa sudo)

Pasang `.desktop` + icon ke `~/.local/share` supaya frans muncul di
menu aplikasi (kategori **Development**):

    bash scripts/install-desktop-user.sh

Binary kandidat yang dicek: `build/bin/frans`, `build/linux-release/bin/frans`,
`build/linux-debug/bin/frans`. Path lain:

    bash scripts/install-desktop-user.sh /path/ke/frans

Test:

    gtk-launch frans

### Linux — system-wide (sudo)

    sudo cmake --install build --prefix /usr/local

**Catatan**: `Exec=` di `.desktop` di-resolve pada **configure time**.
Kalau mau prefix lain, set `-DCMAKE_INSTALL_PREFIX` saat configure:

    cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/frans
    cmake --build build -j
    sudo cmake --install build

### Windows — shortcut Desktop / Start Menu

    powershell -ExecutionPolicy Bypass -File scripts\create-shortcut.ps1

Atau double-click `scripts\create-shortcut.bat`.

Path eksplisit:

    powershell -ExecutionPolicy Bypass -File scripts\create-shortcut.ps1 `
        -TargetPath "C:\path\ke\frans.exe"

Hanya salah satu lokasi: `-Location Desktop` atau `-Location StartMenu`.
Default: **Desktop + Start Menu** (subfolder `Frans`).

Detail lengkap: lihat `scripts/README.md`.