#pragma once

#include "domain/import/GeneratedFile.h"

#include <string>
#include <vector>

namespace frans::application {

using GeneratedFile = frans::domain::GeneratedFile;

struct MultiFileParseResult {
    std::vector<GeneratedFile> files;
    bool                       multi_file = false;
    std::string                error;
};

// Parse output LLM menjadi daftar file.
//
// Format:
//   File: relative/path.py
//   [fenced code block]
//   <content>
//
// Kalau tidak ada marker "File:", fallback ke single-file (butuh
// `fallback_path`). Kalau fallback_path kosong DAN tidak ada marker,
// return dengan error terisi.
//
// Deterministik. Tidak mengakses filesystem.
MultiFileParseResult parseModelOutput(const std::string& output,
                                       const std::string& fallback_path);

} // namespace frans::application