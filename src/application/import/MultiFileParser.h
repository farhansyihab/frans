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
//      Stage 11b-fix: leading ASCII whitespace (spasi, tab) SEBELUM marker
//      diabaikan — supaya paste dengan indentasi (dari web browser, markdown
//      list, terminal) tetap terdeteksi. UTF-8 BOM di awal string juga
//      di-strip. Ini tidak menimbulkan false-positive signifikan: baris yang
//      setelah trim berbentuk "File: <path>" memang hampir pasti marker.
//   2. Setelah marker, fenced block pertama = isi file.
//   3. Fence penutup ``` mengakhiri file. Teks antara file diabaikan.
//   4. Kalau TIDAK ADA marker `File:` sama sekali → fallback single-file:
//      ambil fenced block pertama, pakai `fallback_path`.
//   5. Kalau `fallback_path` kosong DAN tidak ada marker → return dengan
//      `error` terisi.
//
// Stage 11b-fix — diagnostic: kalau input mengandung substring "File:"
// tapi tidak ada marker yang match (mis. karena format prefix aneh,
// bullet "- File:", atau karakter Unicode pengganti), fungsi ini akan
// dump 5 baris pertama ke stderr dengan prefix [frans][multi_file_parser].
//
// Deterministik. Tidak mengakses filesystem.
MultiFileParseResult parseModelOutput(const std::string& output,
                                       const std::string& fallback_path);

} // namespace frans::application
