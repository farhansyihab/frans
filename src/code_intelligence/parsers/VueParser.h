// ~/AgenAI/frans/src/code_intelligence/parsers/VueParser.h
#pragma once

#include "code_intelligence/parsers/ILanguageParser.h"

#include <memory>
#include <string>

namespace frans::code_intelligence {

class TypeScriptParser;
class JavaScriptParser;

// Blueprint Stage 10c — ILanguageParser untuk Vue Single File
// Component (.vue).
//
// Desain: TIDAK memakai grammar tree-sitter-vue. Ekstraksi <script>
// block dilakukan via string manipulation (SFC format sangat
// sederhana), lalu didelegasikan ke TypeScriptParser atau
// JavaScriptParser dari Stage 10b.
//
// Line number snippet di-adjust dengan offset posisi <script> block
// di file .vue, supaya snippet menunjuk lokasi ASLI yang user lihat
// di editor (bukan lokasi relatif di dalam <script>).
//
// RAII: internal parser instances di-construct sekali di constructor
// dan di-reuse lintas panggilan parse(). Non-copyable.
//
// Tidak thread-safe: satu instance = satu thread (TSParser* internal
// tidak thread-safe). Kalau perlu parse paralel, buat instance
// terpisah per worker.
class VueParser : public ILanguageParser {
public:
    VueParser();
    ~VueParser() override;

    VueParser(const VueParser&)            = delete;
    VueParser& operator=(const VueParser&) = delete;

    std::string language() const override { return "vue"; }

    ParsedFile parse(const std::string& file_path,
                     const std::string& source) override;

private:
    // Hasil ekstraksi <script> block dari SFC.
    //
    // `start_line` = nomor baris (1-based) di file .vue tempat
    // karakter non-whitespace PERTAMA di dalam <script> berada.
    // `content`   = isi <script> block (setelah leading whitespace
    //               di-strip), siap diteruskan ke TS/JS parser.
    // `lang`      = nilai atribut `lang` (mis. "ts", "tsx", "js").
    //               Kosong = default JavaScript (Vue convention).
    struct ScriptBlock {
        bool        found      = false;
        int         start_line = 0;
        std::string content;
        std::string lang;
    };

    // Scan source untuk <script ...> ... </script>.
    //
    // - Case-insensitive pada tag name.
    // - Handle `<script>`, `<script lang="ts">`, `<script setup>`,
    //   `<script lang="ts" setup>`, dst.
    // - Multiple <script> blocks: pakai yang PERTAMA, log warning
    //   ke stderr (Vue SFC spec mengizinkan hanya satu).
    // - Malformed (tidak ada `>` atau `</script>`): return
    //   `found=false` + log warning.
    ScriptBlock findScriptBlock(const std::string& source) const;

    // Pilih parser berdasarkan block.lang:
    //   "ts"         → TypeScriptParser (Variant::Ts)
    //   "tsx"        → TypeScriptParser (Variant::Tsx)
    //   "js"/"javascript"/"" → JavaScriptParser
    // Return pointer non-null selalu (default = JS parser).
    ILanguageParser* selectParser(const std::string& lang);

    // Internal parser instances — reused antar panggilan parse().
    std::unique_ptr<TypeScriptParser> ts_parser_;    // Variant::Ts
    std::unique_ptr<TypeScriptParser> tsx_parser_;   // Variant::Tsx
    std::unique_ptr<JavaScriptParser> js_parser_;
};

} // namespace frans::code_intelligence
