// ~/AgenAI/frans/src/application/project/ProjectGraphCache.cpp
#include "application/project/ProjectGraphCache.h"

#include "code_intelligence/builder/TreeSitterGraphBuilder.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <set>
#include <vector>

namespace frans::application {

namespace fs = std::filesystem;

namespace {

// Stage 10d — direktori yang di-skip saat discovery. Sama dengan yang
// di TreeSitterGraphBuilder.cpp (tidak di-share supaya modul ini
// tetap standalone kalau builder berubah — blueprint §7.10 sengaja
// duplikat; hanya extension list yang di-share, bukan skip list).
bool isSkippedDir(const std::string& name) {
    return name == ".git"
        || name == "venv"  || name == ".venv"
        || name == "__pycache__"
        || name == "build"
        || name == "node_modules"
        || name == "dist"  || name == ".next" || name == "out"
        || name == "target"
        || name == "bin"   || name == "obj";
}

// Stage 10d — walk `root` recursively, kumpulkan semua file dengan
// ekstensi yang di-support oleh build ini. Hasil di-sort deterministik.
//
// List ekstensi di-query dari `TreeSitterGraphBuilder::supportedExtensions()`
// — bukan hardcode — supaya selalu sinkron dengan builder. Kalau
// grammar tidak di-clone (mis. C++ parser off), ekstensi .cpp tidak
// akan dimonitor cache, konsisten dengan builder yang juga skip.
std::vector<std::string> discoverSourceFiles(const std::string& root) {
    std::vector<std::string> out;
    std::error_code ec;

    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return out;
    }

    const auto exts =
        frans::code_intelligence::TreeSitterGraphBuilder::supportedExtensions();
    const std::set<std::string> ext_set(exts.begin(), exts.end());

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
            if (isSkippedDir(p.filename().string())) {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (ext_set.count(p.extension().string()) > 0) {
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

    // Rekam mtimes file yang baru di-discover (multi-bahasa).
    mtimes_.clear();
    for (const auto& f : discoverSourceFiles(project_root)) {
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
    const auto current = discoverSourceFiles(root);

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
