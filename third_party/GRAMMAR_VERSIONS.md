# Grammar Tree-sitter — Versi Tercatat

File ini mencatat commit hash grammar tree-sitter yang di-clone untuk
reproduksibilitas build. Kalau grammar di-update dan muncul regresi,
commit hash di sini memudahkan rollback.

## Cara mencatat

Setelah setiap `git clone --depth=1 <repo>` di `third_party/`, jalankan:

```bash
cd third_party/tree-sitter-<name>
git log -1 --format='%H  %ci  %s' | tee -a ../GRAMMAR_VERSIONS.md
```
## Grammar yang digunakan Stage 10

| Grammar | Repo | Status | Commit hash |
|---|---|---|---|
| tree-sitter (core) | https://github.com/tree-sitter/tree-sitter.git | dibutuhkan Stage 5 | (sudah di-clone) |
| tree-sitter-python | https://github.com/tree-sitter/tree-sitter-python.git | dibutuhkan Stage 5 | (sudah di-clone) |
| tree-sitter-cpp | https://github.com/tree-sitter/tree-sitter-cpp.git | dibutuhkan Stage 10a |8b5b49eb196bec7040441bee33b2c9a4838d6967  2026-02-24 18:13:31 -0600  chore: temporarily disable node binding test |
| tree-sitter-javascript | https://github.com/tree-sitter/tree-sitter-javascript.git | dibutuhkan Stage 10b | 58404d8cf191d69f2674a8fd507bd5776f46cb11  2025-09-14 23:42:24 -0400  ci: bump tree-sitter/parser-test-action from 2 to 3 |
| tree-sitter-typescript | https://github.com/tree-sitter/tree-sitter-typescript.git | dibutuhkan Stage 10b | 75b3874edb2dc714fb1fd77a32013d0f8699989f  2025-01-30 14:11:39 -0800  Include LICENSE file (#325) |
