// ~/AgenAI/frans/src/application/agent/MultiFileParser.h
#pragma once

#include <string>
#include <vector>

namespace frans::application {

// Satu file hasil generasi model.
//
// `relative_path` selalu relatif terhadap sandbox root (bukan absolut).
// `content` sudah di-trim trailing newline (konsisten dengan
// extractFencedCodeBlock).
struct GeneratedFile {
    std::string relative_path;
    std::string content;
};

struct MultiFileParseResult {
    std::vector<GeneratedFile> files;
    bool                       multi_file = false;   // true kalau ada marker "File:"
    std::string                error;                 // terisi kalau parsing gagal total
};

// Parse output LLM menjadi daftar file.
//
// Format yang dikenali (convention §5.7 extension Stage 7e):
//
//   File: relative/path.py
//   ```python
//   <content>
//   ```
//
//   File: another.py
//   ```python
//   <content>
//   ```
//
// Aturan:
//   1. Marker `File:` case-insensitive ("File:", "file:", "FILE:" semua OK).
//   2. Setelah marker, fenced block pertama = isi file.
//   3. Fence penutup ``` mengakhiri file. Teks antara file diabaikan.
//   4. Kalau TIDAK ADA marker `File:` sama sekali → fallback single-file:
//      ambil fenced block pertama, pakai `fallback_path`.
//   5. Kalau `fallback_path` kosong DAN tidak ada marker → return dengan
//      `error` terisi.
//
// Deterministik. Tidak mengakses filesystem.
MultiFileParseResult parseModelOutput(const std::string& output,
                                       const std::string& fallback_path);

} // namespace frans::application
