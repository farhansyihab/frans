#!/usr/bin/env bash
# Buat fixture paste-ready untuk uji manual Fase A #44 + #45.
#
# Pemakaian:
#   bash tests/manual/make-fasea44-fixtures.sh [output_dir]
#
# Default output: tests/manual/fixtures-generated/
#
# Fixture ditulis dengan fence tiga-backtick yang dibangun lewat printf
# supaya script ini sendiri tidak memuat literal tiga-backtick.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${1:-$SCRIPT_DIR/fixtures-generated}"
FENCE="$(printf '\140\140\140')"

mkdir -p "$OUT_DIR"

# ---------------------------------------------------------------------------
# 01: CMake project dengan compile error -> phase BUILD gagal.
# ---------------------------------------------------------------------------
cat > "$OUT_DIR/01-cmake-fail.md" <<EOF
File: CMakeLists.txt
${FENCE}cmake
cmake_minimum_required(VERSION 3.16)
project(demo_a44 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
add_executable(demo src/main.cpp)
${FENCE}

File: src/main.cpp
${FENCE}cpp
#include <iostream>

int main() {
    // Sengaja salah: identifier tidak dideklarasikan.
    std::cout << undeclared_variable << std::endl;
    return 0;
}
${FENCE}
EOF

# ---------------------------------------------------------------------------
# 02: perbaikan main.cpp -> build sukses (untuk uji clear last_build_error).
# ---------------------------------------------------------------------------
cat > "$OUT_DIR/02-cmake-fix.md" <<EOF
File: src/main.cpp
${FENCE}cpp
#include <iostream>

int main() {
    std::cout << "Halo dari demo_a44" << std::endl;
    return 0;
}
${FENCE}
EOF

# ---------------------------------------------------------------------------
# 03: Python runtime error -> phase RUN gagal (traceback + path:line).
# ---------------------------------------------------------------------------
cat > "$OUT_DIR/03-python-fail.md" <<EOF
File: app.py
${FENCE}python
#!/usr/bin/env python3
"""Fixture Fase A #44: gagal di phase RUN."""


def main() -> None:
    raise ValueError("sengaja gagal untuk uji Fase A #44")


if __name__ == "__main__":
    main()
${FENCE}
EOF

# ---------------------------------------------------------------------------
# 04: sleep 60 detik -> untuk uji Cancel (tidak boleh menimpa error lama).
# ---------------------------------------------------------------------------
cat > "$OUT_DIR/04-sleeper.md" <<EOF
File: sleeper.py
${FENCE}python
import time


def main() -> None:
    print("Mulai tidur 60 detik...", flush=True)
    time.sleep(60)
    print("Selesai tidur", flush=True)


if __name__ == "__main__":
    main()
${FENCE}
EOF

# ---------------------------------------------------------------------------
# 05: Python sukses -> bersihkan last_build_error di root Python.
# ---------------------------------------------------------------------------
cat > "$OUT_DIR/05-python-ok.md" <<EOF
File: hello.py
${FENCE}python
#!/usr/bin/env python3
"""Fixture Fase A #44: build/run sukses."""


def main() -> None:
    print("Halo, Dunia!")


if __name__ == "__main__":
    main()
${FENCE}
EOF

echo "[frans] fixture ditulis ke: $OUT_DIR"
ls -1 "$OUT_DIR"
echo ""
echo "Langkah uji: lihat tests/README-FaseA-44.md"