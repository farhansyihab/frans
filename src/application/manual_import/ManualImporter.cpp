// ~/AgenAI/frans/src/application/manual_import/ManualImporter.cpp
#include "application/manual_import/ManualImporter.h"

#include <cctype>
#include <cstdio>
#include <utility>

namespace frans::application {

namespace {

constexpr const char* kLogPrefix = "[frans][manual_import]";

std::string toLowerAscii(std::string s) {
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace

ManualImporter::ManualImporter(frans::domain::IFileSystemPort& fs,
                                InvalidateFn on_written)
    : fs_(fs), on_written_(std::move(on_written))
{}

// ---------------------------------------------------------------------------
// languageFromPath — minimal, cukup untuk DangerCheckHandler.
//
// DangerCheckHandler Stage 7c hanya punya pattern list Python; bahasa lain
// return Safe. Tapi kita tetap kirim nama bahasa yang benar (bukan "python"
// untuk semua) supaya Stage 10+ bisa extend tanpa mengubah call site.
// ---------------------------------------------------------------------------
std::string ManualImporter::languageFromPath(const std::string& path) {
    const auto slash           = path.find_last_of('/');
    const auto basename_start  = (slash == std::string::npos)
                               ? std::size_t{0}
                               : slash + 1;
    const auto dot             = path.find_last_of('.');

    if (dot == std::string::npos) return {};
    if (dot < basename_start)     return {};   // titik di bagian direktori
    if (dot == basename_start)    return {};   // hidden file ".bashrc"

    const std::string ext = toLowerAscii(path.substr(dot + 1));

    if (ext == "py" || ext == "pyw" || ext == "pyi")                  return "python";
    if (ext == "cpp" || ext == "cc" || ext == "cxx"
     || ext == "hpp" || ext == "hxx" || ext == "hh" || ext == "h")    return "cpp";
    if (ext == "ts"  || ext == "tsx")                                 return "typescript";
    if (ext == "js"  || ext == "jsx" || ext == "mjs" || ext == "cjs") return "javascript";
    if (ext == "vue")                                                 return "vue";
    return {};
}

// ---------------------------------------------------------------------------
// preview — Phase 1
// ---------------------------------------------------------------------------
ImportResult ManualImporter::preview(const std::string& text,
                                      const std::string& fallback_path) {
    // Reset seluruh state — preview berikutnya tidak mewarisi apapun.
    last_preview_ = ImportResult{};
    snapshots_.clear();
    has_preview_ = false;

    if (text.empty()) {
        last_preview_.parse_failed = true;
        last_preview_.parse_error  = "Input kosong — tidak ada yang diparse.";
        std::fprintf(stderr, "%s preview: input kosong\n", kLogPrefix);
        std::fflush(stderr);
        return last_preview_;
    }

    const auto parse = parseModelOutput(text, fallback_path);
    if (!parse.error.empty()) {
        last_preview_.parse_failed = true;
        last_preview_.parse_error  = parse.error;
        std::fprintf(stderr, "%s preview: parse failed — %s\n",
                     kLogPrefix, parse.error.c_str());
        std::fflush(stderr);
        return last_preview_;
    }

    last_preview_.multi_file = parse.multi_file;

    std::fprintf(stderr, "%s preview: parsed %zu file(s), multi_file=%d\n",
                 kLogPrefix, parse.files.size(),
                 parse.multi_file ? 1 : 0);
    std::fflush(stderr);

    for (const auto& gf : parse.files) {
        ImportFilePreview p;
        p.relative_path = gf.relative_path;
        p.content       = gf.content;
        p.size_bytes    = gf.content.size();

        // ---- 1. Sandbox containment ----
        // Ini yang mencegah path escape ("../x", symlink keluar, dll).
        // Path yang ditolak TIDAK akan sampai ke writeFile().
        if (!fs_.isPathSafe(gf.relative_path)) {
            p.status         = ImportFileStatus::Blocked;
            p.danger         = DangerVerdict::Blocked;
            p.blocked_reason = "Path keluar dari sandbox (path escape) "
                               "atau tidak valid.";
            std::fprintf(stderr,
                "%s   BLOCKED (path) %s\n",
                kLogPrefix, gf.relative_path.c_str());
            last_preview_.files.push_back(std::move(p));
            ++last_preview_.blocked_count;
            continue;
        }

        // ---- 2. Danger check ----
        // Untuk bahasa yang belum punya pattern list (Stage 7c hanya
        // Python), checkCode() return Safe — non-blocking, konsisten
        // dengan Agent flow.
        const std::string lang = languageFromPath(gf.relative_path);
        const auto dc = danger_.checkCode(gf.content, lang);
        p.danger             = dc.verdict;
        p.danger_explanation = dc.explanation;

        if (dc.is_blocked()) {
            p.status         = ImportFileStatus::Blocked;
            p.blocked_reason = dc.explanation;
            std::fprintf(stderr,
                "%s   BLOCKED (danger) %s — %s\n",
                kLogPrefix, gf.relative_path.c_str(),
                dc.explanation.c_str());
            last_preview_.files.push_back(std::move(p));
            ++last_preview_.blocked_count;
            continue;
        }

        // ---- 3. New vs Overwrite ----
        if (fs_.exists(gf.relative_path)) {
            p.status = ImportFileStatus::Overwrite;
            ++last_preview_.overwrite_count;
        } else {
            p.status = ImportFileStatus::New;
            ++last_preview_.new_count;
        }

        last_preview_.total_bytes += p.size_bytes;

        // Logging per file: NeedsConfirm dan normal berbeda supaya mudah
        // di-grep (mis. `grep NEEDS-CONFIRM`).
        if (dc.needs_confirm()) {
            std::fprintf(stderr,
                "%s   NEEDS-CONFIRM %s — %s (akan tetap ditulis saat commit)\n",
                kLogPrefix, gf.relative_path.c_str(),
                dc.explanation.c_str());
        } else {
            std::fprintf(stderr,
                "%s   %s %s (%zu byte)\n",
                kLogPrefix,
                (p.status == ImportFileStatus::Overwrite) ? "OVERWRITE" : "NEW",
                gf.relative_path.c_str(),
                p.size_bytes);
        }

        last_preview_.files.push_back(std::move(p));
    }

    has_preview_ = true;
    std::fflush(stderr);
    return last_preview_;
}

// ---------------------------------------------------------------------------
// commit — Phase 2
// ---------------------------------------------------------------------------
ImportResult ManualImporter::commit() {
    if (!has_preview_) {
        ImportResult r;
        r.parse_failed = true;
        r.parse_error  = "Belum ada preview yang berhasil. "
                         "Panggil preview() dulu sebelum commit().";
        std::fprintf(stderr, "%s commit: no preview\n", kLogPrefix);
        std::fflush(stderr);
        return r;
    }

    ImportResult result = last_preview_;
    result.written_count = 0;
    result.failed_count  = 0;

    for (const auto& p : result.files) {
        if (p.status == ImportFileStatus::Blocked) {
            // Sudah dilaporkan di preview, sudah dihitung di blocked_count.
            // Tidak ada logging tambahan di sini supaya tidak spam.
            continue;
        }

        // Snapshot SEBELUM write. Idempotent — kalau sudah ada, tidak
        // ditimpa. Konsisten dengan snapshotBeforeWrite() AgentContext.
        if (snapshots_.count(p.relative_path) == 0) {
            snapshots_[p.relative_path] =
                fs_.exists(p.relative_path) ? fs_.readFile(p.relative_path)
                                             : std::string{};
        }

        if (fs_.writeFile(p.relative_path, p.content)) {
            ++result.written_count;
            std::fprintf(stderr,
                "%s commit: wrote %s (%zu byte)\n",
                kLogPrefix, p.relative_path.c_str(), p.size_bytes);
        } else {
            ++result.failed_count;
            std::fprintf(stderr,
                "%s commit: FAILED write %s\n",
                kLogPrefix, p.relative_path.c_str());
        }
    }

    std::fflush(stderr);

    // Invalidate cache HANYA kalau ada file yang benar-benar tertulis.
    // Commit dengan written_count==0 (semua blocked) tidak mengubah
    // graph → tidak perlu rebuild.
    if (result.written_count > 0 && on_written_) {
        on_written_();
    }

    std::fprintf(stderr,
        "%s commit: done — written=%zu failed=%zu blocked=%zu\n",
        kLogPrefix, result.written_count, result.failed_count,
        result.blocked_count);
    std::fflush(stderr);

    // Commit berikutnya harus preview ulang. Ini mencegah double-write
    // kalau UI tidak sengaja memanggil commit() dua kali.
    has_preview_ = false;

    return result;
}

// ---------------------------------------------------------------------------
// reset
// ---------------------------------------------------------------------------
void ManualImporter::reset() {
    last_preview_ = ImportResult{};
    snapshots_.clear();
    has_preview_ = false;
}

} // namespace frans::application
