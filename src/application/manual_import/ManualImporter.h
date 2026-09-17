#pragma once

#include "application/import/MultiFileParser.h"
#include "application/safety/DangerCheckHandler.h"
#include "domain/fs/IFileSystemPort.h"
#include "domain/import/ImportResult.h"

#include <map>
#include <string>

namespace frans::application {

using frans::domain::ImportFilePreview;
using frans::domain::ImportFileStatus;
using frans::domain::ImportResult;

// 2-phase commit untuk Manual Import.
//
//   preview(text) -> parse + normalize + validasi. TIDAK menulis apapun.
//   commit()      -> tulis file status != Blocked. Snapshot before write.
//   reset()       -> buang state.
//
// Path handling:
//   - Leading '/' di-strip (AI output sering tulis "/src/main.py").
//   - Parent directory auto-create saat commit.
//   - Path escape ("../") ditolak sandbox.
//
// Danger verdict NeedsConfirm TIDAK memblokir commit — user-mediated.
// Verdict Blocked memblokir mutlak.
class ManualImporter {
public:
    explicit ManualImporter(frans::domain::IFileSystemPort& fs);

    ManualImporter(const ManualImporter&)            = delete;
    ManualImporter& operator=(const ManualImporter&) = delete;

    ImportResult preview(const std::string& text,
                         const std::string& fallback_path = "");

    ImportResult commit();

    void reset();

    bool hasPreview() const { return has_preview_; }

    const std::map<std::string, std::string>& snapshots() const {
        return snapshots_;
    }

private:
    static std::string languageFromPath(const std::string& path);

    frans::domain::IFileSystemPort& fs_;
    DangerCheckHandler              danger_;

    ImportResult                       last_preview_;
    std::map<std::string, std::string> snapshots_;
    bool                               has_preview_ = false;
};

} // namespace frans::application