#pragma once

#include "domain/import/GeneratedFile.h"

#include <cstddef>
#include <string>
#include <vector>

namespace frans::domain {

// Status satu file dalam preview.
enum class ImportFileStatus {
    New,
    Overwrite,
    Blocked,
};

// Verdict danger — disimpan di sini supaya ImportResult tidak perlu
// include DangerCheckHandler.h (menghindari dependensi sirkular).
enum class DangerVerdictLite {
    Safe,
    NeedsConfirm,
    Blocked,
};

struct ImportFilePreview {
    std::string       relative_path;   // sudah dinormalisasi (tanpa leading '/')
    std::string       content;
    std::size_t       size_bytes = 0;
    ImportFileStatus  status     = ImportFileStatus::New;

    DangerVerdictLite danger     = DangerVerdictLite::Safe;
    std::string       danger_explanation;
    std::string       blocked_reason;
};

struct ImportResult {
    std::vector<ImportFilePreview> files;

    bool        parse_failed = false;
    std::string parse_error;
    bool        multi_file   = false;

    std::size_t total_bytes     = 0;
    std::size_t new_count       = 0;
    std::size_t overwrite_count = 0;
    std::size_t blocked_count   = 0;

    std::size_t written_count = 0;
    std::size_t failed_count  = 0;

    bool has_blocked()  const { return blocked_count > 0; }
    bool has_writable() const { return new_count + overwrite_count > 0; }
    bool is_empty()     const { return files.empty(); }
};

} // namespace frans::domain