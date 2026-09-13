// ~/AgenAI/frans/src/code_intelligence/parsers/ILanguageParser.h
#pragma once

#include "code_intelligence/parsers/ParsedFile.h"

#include <string>

namespace frans::code_intelligence {

// Blueprint §4 — Strategy. Satu implementasi per bahasa.
// Stage 5: hanya Python. Stage 10: C++, TS, JS, Vue.
//
// Kontrak:
//   - parse() TIDAK boleh throw. Kalau source tidak valid → ParsedFile kosong.
//   - snippet di ParsedFile.snippets WAJIB sudah di-sort by (path, start_line).
class ILanguageParser {
public:
    virtual ~ILanguageParser() = default;

    virtual std::string language() const = 0;

    virtual ParsedFile parse(const std::string& file_path,
                             const std::string& source) = 0;
};

} // namespace frans::code_intelligence
