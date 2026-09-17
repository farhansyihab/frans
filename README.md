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