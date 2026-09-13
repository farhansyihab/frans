// ~/AgenAI/frans/src/application/agent/CodeExtraction.h
#pragma once

#include <string>

namespace frans::application {

// Ekstrak isi fenced code block pertama (``` ... ```) dari output model.
//
// Kontrak:
//   - Return false kalau tidak ada "```" pembuka, atau tidak ada newline
//     setelah fence pembuka, atau tidak ada "```" penutup.
//   - Trailing newline di dalam block dipangkas (trim dari belakang).
//   - Bahasa (```python) di fence pembuka diabaikan — yang diambil isi
//     setelah newline pertama.
//
// Dipakai bersama oleh GenerateState (Stage 7b) dan FixState (Stage 7d).
// Header-only inline supaya tidak perlu masuk ke library khusus.
inline bool extractFencedCodeBlock(const std::string& modelOutput,
                                   std::string& outCode)
{
    const std::size_t start = modelOutput.find("```");
    if (start == std::string::npos) return false;

    const std::size_t lineEnd = modelOutput.find('\n', start);
    if (lineEnd == std::string::npos) return false;

    const std::size_t contentStart = lineEnd + 1;
    const std::size_t end = modelOutput.find("```", contentStart);
    if (end == std::string::npos) return false;

    outCode = modelOutput.substr(contentStart, end - contentStart);

    // Trim trailing newlines.
    while (!outCode.empty() && outCode.back() == '\n') {
        outCode.pop_back();
    }
    return true;
}

} // namespace frans::application
