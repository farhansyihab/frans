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
    ./build/bin/frans_scenarios --dump cancel-sleep
    ./build/bin/frans_scenarios --dump npm-basic
    ./build/bin/frans_scenarios --dump npm-typescript
    ./build/bin/frans_scenarios --dump standalone-js

Lalu copy output-nya ke paste area di dialog Manual Import.

## Daftar scenario

Lihat:

    ./build/bin/frans_scenarios --list

### 01 python-standalone (§9.2 #16)

Input: skrip Python `hello.py`, tanpa marker build system apapun.

Ekspektasi:
- Parser: 1 file, no error. Danger: Safe.
- Setelah commit → klik Build:
  - Build system: `Standalone`. Language: `Python`.
  - Build phase: skipped (tidak ada kompilasi).
  - Run phase: `python3 hello.py`.
  - Build output menampilkan `stdout: Halo, Dunia!`

### 02 cmake-multifile (regression §9.1 #12)

Input: `CMakeLists.txt` + `src/main.cpp` (multi-file, ada `enable_testing()`).

Ekspektasi:
- Parser: 2 file, no error. Danger: Safe.
- Build system: `CMake`. Test runner: `CTest`.
- Build → Test sukses.

### 03 path-escape (§9.2 #18)

Input: `../evil.py` (escape sandbox).

Ekspektasi:
- Parser: 1 file, no error (parser tidak tahu soal sandbox).
- ManualImporter::preview: file **BLOCKED**.
- Tidak ditulis ke disk.
- Cek otomatis: `sandbox-path-safety`.

### 04 danger-python (§9.2 #19)

Input: `cleanup.py` dengan `os.system(...)` + `shutil.rmtree(...)`.

Ekspektasi:
- Danger: **NeedsConfirm** (`[!] CONFIRM`).
- File tetap ditulis (user-mediated).
- Saat dijalankan, stderr berisi traceback, related_files menunjuk
  frame user (`cleanup.py:9`), bukan stdlib.

### 05 ai-browser-realistic (§9.2 #17)

Input: output multi-file realistis dengan prose sebelum & sesudah
blok kode (seperti output DeepSeek/Gemini).

Ekspektasi:
- Parser: 2 file (`utils.py`, `app.py`). Prose diabaikan.
- Danger: Safe.

### 06 cancel-sleep (§9.3 #23)

Input: script Python `sleeper.py` yang `time.sleep(60)`.

**Test manual via GUI:**

1. `mkdir -p /tmp/frans_cancel && rm -rf /tmp/frans_cancel/*`
2. `./build/bin/frans`
3. Buka Manual Import Dialog. Project root: `/tmp/frans_cancel`.
4. Paste `frans_scenarios --dump cancel-sleep`.
5. Klik **[Parse & Preview]** → `[+] NEW sleeper.py`.
6. Klik **[Write to Disk]**.
7. Klik **[Build / Test / Run]**.
8. **Dalam 2 detik**, klik **[Cancel]**.

Ekspektasi:
- Status bar: `Cancelling...` lalu `CANCELLED oleh user — Standalone @ ...`.
- Build output area `[RUN]`: