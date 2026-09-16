// ~/AgenAI/frans/src/application/build_runner/CppBuildStrategy.cpp
#include "application/build_runner/CppBuildStrategy.h"

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace frans::application {

namespace {

bool hasSuffix(const std::string& s, const char* suffix) {
    const std::size_t n = std::char_traits<char>::length(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool isCppSource(const std::string& p) {
    return hasSuffix(p, ".cpp") || hasSuffix(p, ".cc") || hasSuffix(p, ".cxx");
}

bool isHeader(const std::string& p) {
    return hasSuffix(p, ".h")   || hasSuffix(p, ".hpp")
        || hasSuffix(p, ".hxx") || hasSuffix(p, ".hh");
}

bool hasMainFunction(const std::string& content) {
    return content.find("int main(")  != std::string::npos
        || content.find("int main (") != std::string::npos;
}

// Helper: apend `-I <path>` ke argv. Skip kalau path kosong.
void appendIncludePath(std::vector<std::string>& argv,
                        const std::string& path) {
    if (path.empty()) return;
    argv.push_back("-I");
    argv.push_back(path);
}

std::string stripSandboxPrefix(const std::string& raw,
                                const std::string& root) {
    if (root.empty() || raw.empty()) return raw;
    std::string prefix = root;
    if (prefix.back() != '/') prefix += '/';
    if (raw.size() > prefix.size() && raw.rfind(prefix, 0) == 0) {
        return raw.substr(prefix.size());
    }
    return raw;
}

int parseIntSafe(const std::string& s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}

std::string tailString(const std::string& s, std::size_t max_chars) {
    if (s.size() <= max_chars) return s;
    return std::string("...(truncated)...\n")
         + s.substr(s.size() - max_chars);
}

// Hint yang ditambahkan ke error_message kalau error khas dependensi
// eksternal terdeteksi. Mencegah user bingung "kenapa header <FL/...>
// tidak ketemu".
std::string externalDepHint(const std::string& stderr_text) {
    // Pola umum: "#include <FL/..." atau "<tree_sitter/..." atau
    // "<nlohmann/..." muncul di pesan g++.
    static const char* kSignatures[] = {
        "FL/",
        "tree_sitter/",
        "nlohmann/",
        "X11/",
        "GL/",
    };
    for (const char* sig : kSignatures) {
        if (stderr_text.find(sig) != std::string::npos) {
            return "\n\nHint: file ini butuh dependensi eksternal (mis. FLTK, "
                   "tree-sitter, X11). Build/Manual Import hanya menyediakan "
                   "header lokal + standard library. Untuk build project "
                   "lengkap dengan dependensi, gunakan cmake/make di terminal "
                   "(mis. `cmake --build build`).";
        }
    }
    return {};
}

} // namespace

// ---------------------------------------------------------------------------
std::string CppBuildStrategy::languageName() const {
    return "C++";
}

// ---------------------------------------------------------------------------
std::string CppBuildStrategy::detectEntryPoint(
    const std::vector<GeneratedFile>& files,
    frans::domain::IFileSystemPort& /*fs*/,
    const std::string& /*sandbox_root*/)
{
    std::vector<std::string> cpp_files;
    cpp_files.reserve(files.size());

    for (const auto& f : files) {
        if (isCppSource(f.relative_path)) {
            cpp_files.push_back(f.relative_path);
        }
    }
    if (cpp_files.empty()) return {};

    // Prioritas 1: file dengan `int main(`.
    for (const auto& f : files) {
        if (!isCppSource(f.relative_path)) continue;
        if (hasMainFunction(f.content)) return f.relative_path;
    }

    // Prioritas 2: alfabetis pertama — supaya syntax-check mode tetap
    // punya `entry_point` yang berarti untuk di-display di UI.
    std::sort(cpp_files.begin(), cpp_files.end());
    return cpp_files.front();
}

// ---------------------------------------------------------------------------
std::vector<std::string> CppBuildStrategy::buildCommand(
    const std::string& entry_point,
    const std::vector<GeneratedFile>& files,
    const std::string& sandbox_root)
{
    if (entry_point.empty()) return {};

    // Kumpulkan semua .cpp/.cc/.cxx + deteksi apakah ada `int main`.
    std::vector<std::string> sources;
    bool has_main = false;
    for (const auto& f : files) {
        if (!isCppSource(f.relative_path)) continue;
        sources.push_back(f.relative_path);
        if (hasMainFunction(f.content)) has_main = true;
    }
    if (sources.empty()) return {};

    std::vector<std::string> argv;
    argv.push_back("g++");
    argv.push_back("-std=c++20");

    // Include path — untuk resolve `#include "ui/..."` /
    // `#include "application/..."` di struktur project.
    appendIncludePath(argv, sandbox_root);
    if (!sandbox_root.empty()) {
        appendIncludePath(argv, sandbox_root + "/src");
        appendIncludePath(argv, sandbox_root + "/include");
    }

    if (has_main) {
        // ---- Mode A: full compile + link ----
        argv.push_back("-o");
        argv.push_back("a.out");
        for (const auto& s : sources) argv.push_back(s);
    } else {
        // ---- Mode B: syntax check only ----
        // Tidak link, tidak butuh symbol resolution. User tetap dapat
        // feedback kalau ada syntax error / include error.
        argv.push_back("-fsyntax-only");
        for (const auto& s : sources) argv.push_back(s);
    }
    (void)isHeader;   // header tidak di-pass ke g++ (di-handle via #include)

    return argv;
}

// ---------------------------------------------------------------------------
BuildReport CppBuildStrategy::parseOutput(
    int exit_code,
    bool timed_out,
    const std::string& stdout_text,
    const std::string& stderr_text,
    double duration_sec,
    const std::string& sandbox_root)
{
    BuildReport r;
    r.language     = "C++";
    r.exit_code    = exit_code;
    r.timed_out    = timed_out;
    r.duration_sec = duration_sec;
    r.stdout_tail  = tailString(stdout_text, 4096);
    r.stderr_tail  = tailString(stderr_text, 4096);

    if (timed_out) {
        r.success = false;
        r.error_message = "Timeout — g++ tidak selesai dalam batas waktu.";
        return r;
    }

    if (exit_code == 0) {
        r.success = true;
        return r;
    }

    if (exit_code == 127) {
        r.success = false;
        r.error_message = "g++ tidak ditemukan di PATH. "
                          "Install: sudo apt install build-essential";
        return r;
    }

    r.success = false;

    const std::string& src =
        !stderr_text.empty() ? stderr_text : stdout_text;

    // Format GCC/Clang: `path:line:col: error|warning|note: message`.
    static const std::regex re(
        R"re(([^:\n]+):(\d+):(\d+):\s+(error|warning|note):\s+([^\n]+))re");

    std::string primary_path;
    int         primary_line = 0;
    std::string primary_msg;
    std::string first_any_msg;

    for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
         it != std::sregex_iterator(); ++it)
    {
        const auto& m = *it;
        const std::string sev = m[4].str();
        const std::string msg = m[5].str();
        if (first_any_msg.empty()) first_any_msg = msg;

        if (sev == "error" && primary_path.empty()) {
            primary_path = stripSandboxPrefix(m[1].str(), sandbox_root);
            primary_line = parseIntSafe(m[2].str(), 0);
            primary_msg  = msg;
            break;
        }
    }

    if (!primary_path.empty()) {
        RelatedFile rf;
        rf.path = primary_path;
        rf.line = primary_line;
        rf.role = "PRIMARY_ERROR";
        rf.note = primary_msg;
        r.related_files.push_back(std::move(rf));
        r.error_message = primary_msg;
    } else if (!first_any_msg.empty()) {
        r.error_message = first_any_msg;
    } else {
        // Kalau g++ error sebelum titik yang match regex (mis. fatal error
        // "ui/ManualImportDialog.h: No such file or directory"), pesan
        // penting ada di baris terakhir stderr. Ambil baris non-empty
        // terakhir sebagai fallback.
        std::string last_line;
        std::istringstream iss(src);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) last_line = line;
        }
        if (!last_line.empty()) {
            r.error_message = last_line;
        } else {
            r.error_message = "g++ exited with code "
                            + std::to_string(exit_code);
        }
    }

    // Hint untuk dependensi eksternal.
    r.error_message += externalDepHint(src);

    return r;
}

} // namespace frans::application