# Frans — Test Setup

Folder ini berisi test untuk memvalidasi Manual Import + Danger Check +
Sandbox. Test tidak menjalankan build system asli (butuh project fixture).
Untuk build/test/run end-to-end, gunakan GUI + paste manual.

## Build test

Di root project:

    cmake -S . -B build -DFRANS_BUILD_TESTS=ON
    cmake --build build -j

Binary test: `build/bin/frans_scenarios`.

## Menjalankan test otomatis

Jalankan semua scenario:

    ./build/bin/frans_scenarios

Output: PASS/FAIL per scenario + summary. Exit code 0 kalau semua pass.

## Test manual via GUI

Beberapa scenario bisa dijalankan manual dengan paste ke dialog
Manual Import di aplikasi utama. Untuk print isi scenario (paste-ready):

    ./build/bin/frans_scenarios --dump python-standalone
    ./build/bin/frans_scenarios --dump ai-browser-realistic
    ./build/bin/frans_scenarios --dump path-escape
    ./build/bin/frans_scenarios --dump danger-python

Lalu copy output-nya ke paste area di dialog Manual Import.

## Daftar scenario

Lihat:

    ./build/bin/frans_scenarios --list

### 01 python-standalone (#16)

Input: skrip Python `hello.py`, tanpa marker build system apapun.

Ekspektasi:
- Parser: 1 file, no error.
- Danger: Safe.
- Setelah commit → klik Build:
  - Build system terdeteksi: `Standalone`.
  - Language: `Python`.
  - Build phase: skipped (tidak ada kompilasi).
  - Run phase: `python3 hello.py`.

### 02 cmake-multifile (regression)

Input: `CMakeLists.txt` + `src/main.cpp` (multi-file, ada `enable_testing()`).

Ekspektasi:
- Parser: 2 file, no error.
- Danger: Safe untuk keduanya.
- Setelah commit → klik Build:
  - Build system: `CMake`.
  - Test runner: `CTest`.
  - Build → Test sukses.

### 03 path-escape (#18)

Input: `../evil.py` (escape sandbox).

Ekspektasi:
- Parser: 1 file, no error (parser tidak tahu soal sandbox).
- **ManualImporter::preview**: file muncul dengan status **BLOCKED**.
- Tidak ditulis ke disk.
- Cek otomatis: `sandbox-path-safety` di `frans_scenarios` menguji
  langsung `SandboxedFileSystem::isPathSafe("../evil.py") == false`.

### 04 danger-python (#19)

Input: `cleanup.py` dengan `os.system(...)` + `shutil.rmtree(...)`.

Ekspektasi:
- Parser: 1 file, no error.
- Danger: **NeedsConfirm** (status preview: `[!] CONFIRM`).
- File tetap ditulis (user-mediated), tapi saat Build muncul dialog
  konfirmasi (kalau command-nya dianggap berbahaya).

### 05 ai-browser-realistic (#17)

Input: output multi-file realistis dengan prose sebelum & sesudah
blok kode (seperti output DeepSeek/Gemini).

Ekspektasi:
- Parser tetap menemukan 2 file (`utils.py`, `app.py`).
- Prose di luar blok `File: ...` diabaikan.
- Danger: Safe untuk keduanya.

### sandbox-path-safety (#18, terpisah)

Cek langsung `SandboxedFileSystem::isPathSafe()`:
- Input aman (`hello.py`, `src/main.cpp`, `a/b/c/d.py`) → true.
- Input escape (`../evil.py`, `../../etc/passwd`, `/etc/passwd`,
  `src/../../evil.py`) → false.

## Catatan

Test framework minimal — tidak pakai GoogleTest/Catch2 (sesuai
keputusan §11 Blueprint: personal use, hindari dependency ekstra).

Untuk regression test yang lebih luas (unit test C++), lihat
Blueprint §9.4 #26 (`tests/*.cpp` + `cmake/Tests.cmake`).