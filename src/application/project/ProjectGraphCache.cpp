// ~/AgenAI/frans/src/application/project/ProjectGraphCache.cpp
#include "application/project/ProjectGraphCache.h"

#include "code_intelligence/builder/TreeSitterGraphBuilder.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace frans::application {

namespace fs = std::filesystem;

namespace {

// Walk `root` recursively, kumpulkan semua file .py. Hasil di-sort
// supaya deterministik. Skip direktori yang jelas tidak relevan.
//
// Duplikasi kecil dari TreeSitterGraphBuilder::discoverPythonFiles
// (private). Duplikasi sengaja: meng-expose method itu jadi public
// akan bocorkan detail internal builder yang tidak perlu diketahui
// modul lain. Fungsi ini juga dipakai needs_rebuild() yang tidak butuh
// builder.
std::vector<std::string> discoverPythonFiles(const std::string& root) {
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

} // namespace

const frans::code_intelligence::DependencyGraph&
ProjectGraphCache::getOrBuild(const std::string& project_root) {
    std::lock_guard<std::mutex> lk(mtx_);

    const bool root_changed = (cached_root_ != project_root);
    const bool need_build = root_changed
                         || !status_.valid
                         || needs_rebuild(project_root);

    if (!need_build) {
        return graph_;
    }

    std::fprintf(stderr,
        "[frans][graph_cache] rebuild: root=%s (root_changed=%d, valid=%d)\n",
        project_root.c_str(), root_changed ? 1 : 0, status_.valid ? 1 : 0);
    std::fflush(stderr);

    const auto t0 = std::chrono::steady_clock::now();

    frans::code_intelligence::TreeSitterGraphBuilder builder;
    builder.build(project_root);
    graph_ = std::move(builder.graph());

    const auto t1 = std::chrono::steady_clock::now();
    const double secs = std::chrono::duration<double>(t1 - t0).count();

    // Rekam mtimes file yang baru di-discover.
    mtimes_.clear();
    for (const auto& f : discoverPythonFiles(project_root)) {
        std::error_code ec;
        auto t = fs::last_write_time(f, ec);
        if (!ec) mtimes_[f] = t;
    }

    cached_root_ = project_root;

    status_.valid          = true;
    status_.root           = project_root;
    status_.file_count     = graph_.fileCount();
    status_.symbol_count   = graph_.symbolCount();
    status_.edge_count     = graph_.edgeCount();
    status_.last_build_sec = secs;
    ++status_.build_count;

    std::fprintf(stderr,
        "[frans][graph_cache] built: files=%zu symbols=%zu edges=%zu in %.3fs\n",
        status_.file_count, status_.symbol_count,
        status_.edge_count, secs);
    std::fflush(stderr);

    return graph_;
}

void ProjectGraphCache::invalidate() {
    std::lock_guard<std::mutex> lk(mtx_);
    status_.valid = false;
}

ProjectGraphCache::Status ProjectGraphCache::status() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return status_;
}

bool ProjectGraphCache::needs_rebuild(const std::string& root) const {
    if (!status_.valid) return true;
    if (cached_root_ != root) return true;

    // ---- Full scan (Step 1: deteksi file baru / hilang) ----
    const auto current = discoverPythonFiles(root);

    if (current.size() != mtimes_.size()) return true;
    for (const auto& f : current) {
        if (mtimes_.count(f) == 0) return true;
    }

    // ---- Step 2: cek mtime individual ----
    for (const auto& f : current) {
        std::error_code ec;
        auto t = fs::last_write_time(f, ec);
        if (ec) return true;   // file tak terbaca → rebuild
        auto it = mtimes_.find(f);
        if (it == mtimes_.end()) return true;
        if (it->second != t) return true;
    }
    return false;
}

} // namespace frans::application
