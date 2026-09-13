# tree-sitter submodules

Stage 5 membutuhkan dua submodule yang di-vendor di `third_party/`:

    cd ~/AgenAI/frans/third_party
    git clone --depth=1 https://github.com/tree-sitter/tree-sitter.git
    git clone --depth=1 https://github.com/tree-sitter/tree-sitter-python.git

Tidak perlu install ke sistem — CMakeLists meng-compile langsung sebagai
static library (tree_sitter, tree_sitter_python). Kalau submodule tidak ada,
CMakeLists akan skip `frans_code_intel` + `frans_graph_test` dengan warning,
sehingga build UI yang sudah ada tetap jalan.

Struktur yang diharapkan setelah clone:

    third_party/tree-sitter/lib/src/lib.c
    third_party/tree-sitter/lib/include/tree_sitter/api.h
    third_party/tree-sitter-python/src/parser.c
    third_party/tree-sitter-python/src/scanner.c
