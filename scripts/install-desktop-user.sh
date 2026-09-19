#!/usr/bin/env bash
# Install frans.desktop + icon ke direktori user-level (~/.local/share),
# supaya frans muncul di menu aplikasi tanpa perlu sudo.
#
# Pemakaian:
#   bash scripts/install-desktop-user.sh                  # pakai build/bin/frans
#   bash scripts/install-desktop-user.sh /path/ke/frans   # binary kustom
#
# Script idempoten: aman dijalankan berulang kali.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Kandidat binary, dicek berurutan.
if [[ $# -ge 1 ]]; then
    BINARY="$1"
else
    candidates=(
        "$REPO_ROOT/build/bin/frans"
        "$REPO_ROOT/build/linux-release/bin/frans"
        "$REPO_ROOT/build/linux-debug/bin/frans"
    )
    BINARY=""
    for c in "${candidates[@]}"; do
        if [[ -x "$c" ]]; then BINARY="$c"; break; fi
    done
fi

if [[ -z "$BINARY" || ! -x "$BINARY" ]]; then
    echo "[frans] binary tidak ditemukan / tidak executable." >&2
    echo "        build dulu: cmake -S . -B build && cmake --build build -j" >&2
    echo "        atau berikan path eksplisit sebagai argumen ke-1." >&2
    exit 1
fi

BINARY_ABS="$(readlink -f "$BINARY")"

APPS_DIR="$HOME/.local/share/applications"
ICONS_DIR="$HOME/.local/share/icons/hicolor/scalable/apps"
mkdir -p "$APPS_DIR" "$ICONS_DIR"

# Tulis .desktop dengan Exec = path absolut binary aktual.
cat > "$APPS_DIR/frans.desktop" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Frans
GenericName=AI Code Importer
Comment=Import kode multi-file dari AI chat browser, lalu build / test / run
Exec=$BINARY_ABS
Icon=frans
Terminal=false
Categories=Development;IDE;
Keywords=ai;code;import;build;cmake;npm;
StartupNotify=true
StartupWMClass=frans
EOF

chmod 644 "$APPS_DIR/frans.desktop"

if [[ -f "$REPO_ROOT/packaging/linux/frans.svg" ]]; then
    cp "$REPO_ROOT/packaging/linux/frans.svg" "$ICONS_DIR/frans.svg"
    chmod 644 "$ICONS_DIR/frans.svg"
else
    echo "[frans] warning: packaging/linux/frans.svg tidak ditemukan," >&2
    echo "        icon mungkin tidak muncul di menu." >&2
fi

# Refresh cache kalau tool-nya tersedia (tidak fatal kalau tidak ada).
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPS_DIR" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" \
        >/dev/null 2>&1 || true
fi

echo "[frans] .desktop dipasang:"
echo "        $APPS_DIR/frans.desktop  ->  $BINARY_ABS"
echo "        $ICONS_DIR/frans.svg"
echo ""
echo "Cek menu aplikasi (kategori Development), atau jalankan:"
echo "  gtk-launch frans"