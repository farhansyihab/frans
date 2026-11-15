// src/application/context/IncludeResolver.cpp
//
// Fase E #74 -- Local include closure implementation.
#include "application/context/IncludeResolver.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace frans::application {
namespace {

constexpr std::size_t kMaxUnresolvedReported = 200;

bool isHSpace(char c) {
    return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

bool isIdentChar(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) != 0 || c == '_';
}

std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string dirOf(const std::string& path) {
    const auto pos = path.rfind('/');
    return pos == std::string::npos ? std::string() : path.substr(0, pos);
}

std::string baseOf(const std::string& path) {
    const auto pos = path.rfind('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string joinRel(const std::string& dir, const std::string& rel) {
    return dir.empty() ? rel : dir + "/" + rel;
}

// Hapus komentar dari satu baris. `in_block` menjaga status komentar /* */
// yang melintasi baris. String dan char literal dihormati supaya
// `"a//b.h"` tidak dianggap komentar.
std::string stripComments(const std::string& line, bool& in_block) {
    enum class State { Code, Str, Chr };
    State state = State::Code;

    std::string out;
    out.reserve(line.size());

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        const char n = (i + 1 < line.size()) ? line[i + 1] : '\0';

        if (in_block) {
            if (c == '*' && n == '/') {
                in_block = false;
                ++i;
                out.push_back(' ');
            }
            continue;
        }

        switch (state) {
        case State::Code:
            if (c == '/' && n == '*') {
                in_block = true;
                ++i;
            } else if (c == '/' && n == '/') {
                return out;
            } else {
                if (c == '"')       state = State::Str;
                else if (c == '\'') state = State::Chr;
                out.push_back(c);
            }
            break;
        case State::Str:
            out.push_back(c);
            if (c == '\\' && i + 1 < line.size()) {
                out.push_back(n);
                ++i;
            } else if (c == '"') {
                state = State::Code;
            }
            break;
        case State::Chr:
            out.push_back(c);
            if (c == '\\' && i + 1 < line.size()) {
                out.push_back(n);
                ++i;
            } else if (c == '\'') {
                state = State::Code;
            }
            break;
        }
    }
    return out;
}

enum class ResolveKind { Found, NotFound, Ambiguous, Skipped };

struct Resolved {
    ResolveKind kind = ResolveKind::NotFound;
    std::string path;     // untuk Found
    std::string reason;   // untuk Ambiguous / Skipped
};

using KnownMap  = std::unordered_map<std::string, const CollectedFile*>;
using BaseIndex = std::unordered_map<std::string, std::vector<const CollectedFile*>>;

Resolved classify(const CollectedFile& f) {
    Resolved r;
    r.path = f.relative_path;
    if (f.skipped || !f.is_text) {
        r.kind   = ResolveKind::Skipped;
        r.reason = "skipped:" + (f.skip_reason.empty() ? std::string("binary")
                                                       : f.skip_reason);
    } else {
        r.kind = ResolveKind::Found;
    }
    return r;
}

Resolved resolveOne(const std::string& includer,
                    const std::string& directive,
                    const KnownMap& known,
                    const BaseIndex& by_base,
                    const std::vector<std::string>& search_dirs,
                    bool allow_suffix)
{
    // 1. Folder file yang meng-include, lalu 2. tiap search dir.
    std::vector<std::string> bases;
    bases.push_back(dirOf(includer));
    for (const auto& d : search_dirs) bases.push_back(d);

    for (const auto& base : bases) {
        const auto cand = normalizeRelPath(joinRel(base, directive));
        if (!cand || cand->empty()) continue;
        const auto it = known.find(*cand);
        if (it != known.end()) return classify(*it->second);
    }

    // 3. Suffix match unik di seluruh repo.
    if (allow_suffix) {
        const auto target = normalizeRelPath(directive);
        if (target && !target->empty()) {
            const auto bi = by_base.find(baseOf(*target));
            if (bi != by_base.end()) {
                const std::string tail = "/" + *target;
                std::vector<const CollectedFile*> matches;
                for (const CollectedFile* f : bi->second) {
                    if (f->relative_path == *target ||
                        endsWith(f->relative_path, tail)) {
                        matches.push_back(f);
                    }
                }
                if (matches.size() == 1) return classify(*matches.front());
                if (matches.size() > 1) {
                    Resolved r;
                    r.kind   = ResolveKind::Ambiguous;
                    r.reason = "ambiguous (" + std::to_string(matches.size()) +
                               " kandidat)";
                    return r;
                }
            }
        }
    }
    return Resolved{};
}

struct Node {
    std::string path;
    int depth = 0;
};

} // namespace

// ---------------------------------------------------------------------------
// Helper publik (murni)
// ---------------------------------------------------------------------------

std::vector<std::string> parseQuotedIncludes(const std::string& content) {
    std::vector<std::string> out;
    bool in_block = false;

    std::size_t pos = 0;
    while (pos < content.size()) {
        std::size_t eol = content.find('\n', pos);
        if (eol == std::string::npos) eol = content.size();

        std::string line = content.substr(pos, eol - pos);
        pos = eol + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const std::string code = stripComments(line, in_block);

        std::size_t i = 0;
        while (i < code.size() && isHSpace(code[i])) ++i;
        if (i >= code.size() || code[i] != '#') continue;
        ++i;
        while (i < code.size() && isHSpace(code[i])) ++i;

        static const std::string kKeyword = "include";
        if (code.compare(i, kKeyword.size(), kKeyword) != 0) continue;
        i += kKeyword.size();
        // Tolak `include_next`, `includefoo`, dst.
        if (i < code.size() && isIdentChar(code[i])) continue;

        while (i < code.size() && isHSpace(code[i])) ++i;
        if (i >= code.size() || code[i] != '"') continue;   // <...> / macro: abaikan

        const std::size_t close = code.find('"', i + 1);
        if (close == std::string::npos) continue;

        std::string target = code.substr(i + 1, close - i - 1);
        if (!target.empty()) out.push_back(std::move(target));
    }
    return out;
}

std::optional<std::string> normalizeRelPath(const std::string& path) {
    if (path.empty()) return std::nullopt;

    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');

    if (p.front() == '/') return std::nullopt;   // absolut
    if (p.size() >= 2 && p[1] == ':' &&
        std::isalpha(static_cast<unsigned char>(p[0])) != 0) {
        return std::nullopt;                      // drive letter Windows
    }

    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= p.size()) {
        std::size_t end = p.find('/', start);
        if (end == std::string::npos) end = p.size();

        const std::string seg = p.substr(start, end - start);
        if (seg.empty() || seg == ".") {
            // lewati
        } else if (seg == "..") {
            if (parts.empty()) return std::nullopt;   // keluar dari root
            parts.pop_back();
        } else {
            parts.push_back(seg);
        }
        start = end + 1;
    }

    std::string joined;
    for (std::size_t k = 0; k < parts.size(); ++k) {
        if (k != 0) joined += '/';
        joined += parts[k];
    }
    return joined;
}

bool isIncludeScannable(const std::string& path) {
    const auto dot   = path.rfind('.');
    const auto slash = path.rfind('/');
    if (dot == std::string::npos) return false;
    if (slash != std::string::npos && dot < slash) return false;

    const std::string ext = toLower(path.substr(dot));
    static const char* const kExts[] = {
        ".c", ".cc", ".cpp", ".cxx", ".c++",
        ".h", ".hh", ".hpp", ".hxx", ".h++",
        ".inl", ".ipp", ".tpp"
    };
    for (const char* e : kExts) {
        if (ext == e) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Closure
// ---------------------------------------------------------------------------

IncludeClosureResult resolveIncludeClosureWith(
    const IncludeFileReader& reader,
    const std::vector<std::string>& seed_paths,
    const std::vector<CollectedFile>& all_files,
    const IncludeClosureOptions& opt,
    std::atomic<bool>* cancel)
{
    IncludeClosureResult res;

    if (!opt.enabled) {
        res.ok = true;
        return res;
    }
    if (!reader) {
        res.error = "pembaca file kosong";
        return res;
    }

    // Indeks file yang boleh dipakai (sumber kebenaran = collectFiles).
    KnownMap known;
    BaseIndex by_base;
    for (const auto& f : all_files) {
        known[f.relative_path] = &f;
        by_base[baseOf(f.relative_path)].push_back(&f);
    }

    // Search dir: dinormalisasi, tanpa duplikat; root proyek selalu dicoba terakhir.
    std::vector<std::string> dirs;
    auto addDir = [&dirs](const std::string& d) {
        if (std::find(dirs.begin(), dirs.end(), d) == dirs.end()) dirs.push_back(d);
    };
    for (const auto& d : opt.search_dirs) {
        if (d.empty()) { addDir(std::string()); continue; }
        const auto n = normalizeRelPath(d);
        if (n && !n->empty()) addDir(*n);
    }
    addDir(std::string());

    // BFS dari file terpilih.
    std::unordered_set<std::string> seen;
    std::deque<Node> queue;
    for (const auto& s : seed_paths) {
        const auto n = normalizeRelPath(s);
        if (!n || n->empty()) continue;
        if (!seen.insert(*n).second) continue;
        queue.push_back({*n, 0});
    }

    std::unordered_set<std::string> unresolved_seen;

    while (!queue.empty()) {
        if (cancel && cancel->load()) {
            res.ok = false;
            res.error = "dibatalkan";
            return res;
        }

        const Node node = queue.front();
        queue.pop_front();

        if (node.depth >= opt.max_depth) continue;
        if (!isIncludeScannable(node.path)) continue;

        std::string content;
        bool got = false;
        try {
            got = reader(node.path, content);
        } catch (...) {
            got = false;
        }
        if (!got) continue;

        for (const std::string& directive : parseQuotedIncludes(content)) {
            const Resolved r = resolveOne(node.path, directive, known, by_base,
                                          dirs, opt.allow_suffix_match);

            if (r.kind != ResolveKind::Found) {
                if (res.unresolved.size() >= kMaxUnresolvedReported) continue;
                const std::string key = node.path + "\n" + directive;
                if (!unresolved_seen.insert(key).second) continue;

                IncludeUnresolved u;
                u.included_by = node.path;
                u.directive   = directive;
                u.reason      = (r.kind == ResolveKind::NotFound) ? "not_found"
                                                                  : r.reason;
                res.unresolved.push_back(std::move(u));
                continue;
            }

            if (!seen.insert(r.path).second) continue;   // sudah dipilih / sudah ditambah / siklus

            const CollectedFile& f = *known.at(r.path);
            if (res.additions.size() >= opt.max_added_files ||
                res.added_bytes + f.size_bytes > opt.max_added_bytes) {
                res.dropped_budget.push_back(r.path);
                continue;
            }

            IncludeAddition add;
            add.path        = r.path;
            add.included_by = node.path;
            add.directive   = directive;
            add.depth       = node.depth + 1;
            add.size_bytes  = f.size_bytes;
            res.added_bytes += f.size_bytes;
            res.additions.push_back(std::move(add));

            queue.push_back({r.path, node.depth + 1});
        }
    }

    res.ok = true;
    return res;
}

IncludeClosureResult resolveIncludeClosure(
    const std::string& project_root,
    const std::vector<std::string>& seed_paths,
    const std::vector<CollectedFile>& all_files,
    const IncludeClosureOptions& opt,
    std::atomic<bool>* cancel)
{
    if (!opt.enabled) {
        IncludeClosureResult r;
        r.ok = true;
        return r;
    }
    if (project_root.empty()) {
        IncludeClosureResult r;
        r.error = "project_root kosong";
        return r;
    }

    infrastructure::SandboxedFileSystem fs(project_root);
    const IncludeFileReader reader =
        [&fs](const std::string& rel, std::string& out) {
            out = fs.readFile(rel);
            // Sama seperti SourceCodeBuilder: kosong + tidak ada = tidak ada.
            return !(out.empty() && !fs.exists(rel));
        };

    return resolveIncludeClosureWith(reader, seed_paths, all_files, opt, cancel);
}

std::string summarizeIncludeClosure(const IncludeClosureResult& r) {
    if (!r.ok) {
        return "Include closure gagal: " + (r.error.empty() ? std::string("tidak diketahui")
                                                            : r.error);
    }

    std::ostringstream oss;
    oss << "Include closure: +" << r.additions.size() << " file ("
        << std::fixed << std::setprecision(1)
        << (static_cast<double>(r.added_bytes) / 1024.0) << " KB)";
    if (!r.unresolved.empty()) {
        oss << ", " << r.unresolved.size() << " include tidak terselesaikan";
    }
    if (!r.dropped_budget.empty()) {
        oss << ", " << r.dropped_budget.size() << " dibuang (batas)";
    }
    return oss.str();
}

} // namespace frans::application