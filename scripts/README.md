# Frans — Scripts

Script bantu untuk integrasi desktop. Bukan bagian dari binary
`frans`; cukup dijalankan manual sekali per mesin.

## Linux — user-level install (tanpa sudo)

Pasang `.desktop` + icon ke `~/.local/share`, supaya frans muncul
di menu aplikasi (kategori **Development**) user.

    bash scripts/install-desktop-user.sh

Kandidat binary yang dicek (berurutan):

  1. `build/bin/frans`
  2. `build/linux-release/bin/frans`
  3. `build/linux-debug/bin/frans`

Kalau binary di path lain:

    bash scripts/install-desktop-user.sh /path/ke/frans

Yang dipasang:

  - `~/.local/share/applications/frans.desktop`
  - `~/.local/share/icons/hicolor/scalable/apps/frans.svg`

Test:

    gtk-launch frans

Uninstall:

    rm ~/.local/share/applications/frans.desktop
    rm ~/.local/share/icons/hicolor/scalable/apps/frans.svg

## Linux — system-wide install (`sudo`)

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    sudo cmake --install build --prefix /usr/local

Memasang ke `/usr/local/`:

  - `/usr/local/bin/frans`
  - `/usr/local/share/frans/app.json`
  - `/usr/local/share/applications/frans.desktop`
  - `/usr/local/share/icons/hicolor/scalable/apps/frans.svg`

**Catatan**: `Exec=` di `.desktop` di-resolve pada **configure time**
memakai `CMAKE_INSTALL_PREFIX`. Kalau mau prefix lain, set saat
configure:

    cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/frans

Bukan lewat `--prefix` di `cmake --install`.

## Windows — shortcut Desktop / Start Menu

    powershell -ExecutionPolicy Bypass -File scripts\create-shortcut.ps1

Atau double-click:

    scripts\create-shortcut.bat

Kandidat `frans.exe` yang dicek (berurutan):

  1. `build\bin\Release\frans.exe`
  2. `build\bin\frans.exe`
  3. `build\win-msvc\bin\Release\frans.exe`
  4. `build\win-mingw\bin\frans.exe`
  5. `build\win-msvc\bin\frans.exe`

Path eksplisit:

    powershell -ExecutionPolicy Bypass -File scripts\create-shortcut.ps1 `
        -TargetPath "C:\path\ke\frans.exe"

Hanya salah satu lokasi:

    ... -Location Desktop
    ... -Location StartMenu

Default: **Desktop + Start Menu** (subfolder `Frans`).

Uninstall: hapus shortcut `.lnk` yang dibuat, atau:

    Remove-Item "$([Environment]::GetFolderPath('Desktop'))\Frans.lnk"
    Remove-Item "$([Environment]::GetFolderPath('Programs'))\Frans" -Recurse