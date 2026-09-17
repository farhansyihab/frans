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

// Strip leading '/' supaya "/src/main.py" -> "src/main.py".
std::string normalizeRelativePath(std::string p) {
    std::size_t i = 0;
    while (i < p.size() && p[i] == '/') ++i;
    if (i > 0) p.erase(0, i);
    return p;
}

std::string parentDir(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return {};
    return path.substr(0, slash);
}

} // namespace

ManualImporter::ManualImporter(frans::domain::IFileSystemPort& fs) : fs_(fs) {}

std::string ManualImporter::languageFromPath(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    const auto base_start = (slash == std::string::npos) ? std::size_t{0} : slash + 1;
    const auto dot = path.find_last_of('.');

    if (dot == std::string::npos) return {};
    if (dot < base_start)         return {};
    if (dot == base_start)        return {};

    const std::string ext = toLowerAscii(path.substr(dot + 1));

    if (ext == "py" || ext == "pyw" || ext == "pyi")                  return "python";
    if (ext == "cpp" || ext == "cc" || ext == "cxx"
     || ext == "hpp" || ext == "hxx" || ext == "hh" || ext == "h")    return "cpp";
    if (ext == "ts"  || ext == "tsx")                                 return "typescript";
    if (ext == "js"  || ext == "jsx" || ext == "mjs" || ext == "cjs") return "javascript";
    if (ext == "vue")                                                 return "vue";
    return {};
}

ImportResult ManualImporter::preview(const std::string& text,
                                      const std::string& fallback_path)
{
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
        frans::domain::ImportFilePreview p;
        p.content    = gf.content;
        p.size_bytes = gf.content.size();

        const std::string normalized = normalizeRelativePath(gf.relative_path);
        p.relative_path = normalized;
        if (normalized != gf.relative_path) {
            std::fprintf(stderr,
                "%s   normalize path: '%s' -> '%s'\n",
                kLogPrefix, gf.relative_path.c_str(), normalized.c_str());
        }

        if (!fs_.isPathSafe(p.relative_path)) {
            p.status         = frans::domain::ImportFileStatus::Blocked;
            p.danger         = frans::domain::DangerVerdictLite::Blocked;
            p.blocked_reason = "Path keluar dari sandbox atau tidak valid.";
            std::fprintf(stderr, "%s   BLOCKED (path) %s\n",
                kLogPrefix, p.relative_path.c_str());
            last_preview_.files.push_back(std::move(p));
            ++last_preview_.blocked_count;
            continue;
        }

        const std::string lang = languageFromPath(p.relative_path);
        const auto dc = danger_.checkCode(p.content, lang);

        // Konversi DangerVerdict -> DangerVerdictLite.
        switch (dc.verdict) {
            case DangerVerdict::Safe:         p.danger = frans::domain::DangerVerdictLite::Safe;         break;
            case DangerVerdict::NeedsConfirm: p.danger = frans::domain::DangerVerdictLite::NeedsConfirm; break;
            case DangerVerdict::Blocked:      p.danger = frans::domain::DangerVerdictLite::Blocked;      break;
        }
        p.danger_explanation = dc.explanation;

        if (dc.is_blocked()) {
            p.status         = frans::domain::ImportFileStatus::Blocked;
            p.blocked_reason = dc.explanation;
            std::fprintf(stderr, "%s   BLOCKED (danger) %s — %s\n",
                kLogPrefix, p.relative_path.c_str(), dc.explanation.c_str());
            last_preview_.files.push_back(std::move(p));
            ++last_preview_.blocked_count;
            continue;
        }

        if (fs_.exists(p.relative_path)) {
            p.status = frans::domain::ImportFileStatus::Overwrite;
            ++last_preview_.overwrite_count;
        } else {
            p.status = frans::domain::ImportFileStatus::New;
            ++last_preview_.new_count;
        }
        last_preview_.total_bytes += p.size_bytes;

        if (dc.needs_confirm()) {
            std::fprintf(stderr, "%s   NEEDS-CONFIRM %s — %s\n",
                kLogPrefix, p.relative_path.c_str(), dc.explanation.c_str());
        } else {
            std::fprintf(stderr, "%s   %s %s (%zu byte)\n",
                kLogPrefix,
                (p.status == frans::domain::ImportFileStatus::Overwrite)
                    ? "OVERWRITE" : "NEW",
                p.relative_path.c_str(),
                p.size_bytes);
        }

        last_preview_.files.push_back(std::move(p));
    }

    has_preview_ = true;
    std::fflush(stderr);
    return last_preview_;
}

ImportResult ManualImporter::commit() {
    if (!has_preview_) {
        ImportResult r;
        r.parse_failed = true;
        r.parse_error  = "Belum ada preview yang berhasil.";
        std::fprintf(stderr, "%s commit: no preview\n", kLogPrefix);
        std::fflush(stderr);
        return r;
    }

    ImportResult result = last_preview_;
    result.written_count = 0;
    result.failed_count  = 0;

    for (const auto& p : result.files) {
        if (p.status == frans::domain::ImportFileStatus::Blocked) continue;

        const std::string parent = parentDir(p.relative_path);
        if (!parent.empty()) {
            if (!fs_.createDirectory(parent)) {
                ++result.failed_count;
                std::fprintf(stderr, "%s commit: FAILED mkdir '%s' (file '%s')\n",
                    kLogPrefix, parent.c_str(), p.relative_path.c_str());
                std::fflush(stderr);
                continue;
            }
        }

        if (snapshots_.count(p.relative_path) == 0) {
            snapshots_[p.relative_path] =
                fs_.exists(p.relative_path) ? fs_.readFile(p.relative_path)
                                             : std::string{};
        }

        if (fs_.writeFile(p.relative_path, p.content)) {
            ++result.written_count;
            std::fprintf(stderr, "%s commit: wrote %s (%zu byte)\n",
                kLogPrefix, p.relative_path.c_str(), p.size_bytes);
        } else {
            ++result.failed_count;
            std::fprintf(stderr, "%s commit: FAILED write %s\n",
                kLogPrefix, p.relative_path.c_str());
        }
    }

    std::fflush(stderr);
    std::fprintf(stderr, "%s commit: done — written=%zu failed=%zu blocked=%zu\n",
        kLogPrefix, result.written_count, result.failed_count,
        result.blocked_count);
    std::fflush(stderr);

    has_preview_ = false;
    return result;
}

void ManualImporter::reset() {
    last_preview_ = ImportResult{};
    snapshots_.clear();
    has_preview_ = false;
}

} // namespace frans::application