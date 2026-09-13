// ~/AgenAI/frans/src/code_intelligence/builder/TreeSitterGraphBuilder.cpp
#include "code_intelligence/builder/TreeSitterGraphBuilder.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace frans::code_intelligence {

namespace fs = std::filesystem;

std::vector<std::string> TreeSitterGraphBuilder::discoverPythonFiles(
    const std::string& root) const
{
    std::vector<std::string> out;
    std::error_code ec;

    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return out;
    }

    for (auto it = fs::recursive_directory_iterator(
             root,
             fs::directory_options::skip_permission_denied,
             ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec))
    {
        if (ec) break;
        const auto& p = it->path();

        // Free function, bukan method: fs::is_directory(path, ec).
        const bool is_dir = fs::is_directory(p, ec);

        if (is_dir) {
            const auto fname = p.filename().string();
            if (fname == ".git" || fname == "venv" || fname == ".venv"
             || fname == "__pycache__" || fname == "build"
             || fname == "node_modules") {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (p.extension() == ".py") {
            out.push_back(p.string());
        }
    }

    std::sort(out.begin(), out.end());
    return out;
}

bool TreeSitterGraphBuilder::parseAndAdd(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        errors_.push_back("cannot open: " + path);
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string src = ss.str();

    ParsedFile pf = parser_.parse(path, src);

    // File valid tapi tanpa snippet tetap ditambahkan — berguna untuk
    // file kosong / file yang cuma berisi docstring.
    graph_.addFile(pf);
    return true;
}

void TreeSitterGraphBuilder::build(const std::string& project_root) {
    graph_.clear();
    errors_.clear();

    const auto files = discoverPythonFiles(project_root);
    for (const auto& f : files) {
        parseAndAdd(f);
    }
}

void TreeSitterGraphBuilder::updateFile(const std::string& path) {
    // File mungkin baru dihapus → hapus dari graph.
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        graph_.removeFile(path);
        return;
    }
    // removeFile dulu lalu addFile — supaya entry lama benar-benar
    // hilang sebelum di-replace (incremental update pattern).
    graph_.removeFile(path);
    parseAndAdd(path);
}

} // namespace frans::code_intelligence
