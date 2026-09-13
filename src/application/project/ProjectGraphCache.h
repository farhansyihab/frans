// ~/AgenAI/frans/src/application/project/ProjectGraphCache.h
#pragma once

#include "code_intelligence/graph/DependencyGraph.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

namespace frans::application {

// Blueprint Stage 7f — cache DependencyGraph per project root.
//
// Single-slot: kalau project root berganti, cache lama dibuang TOTAL
// (bukan multi-root map). Ini keputusan sadar, cocok untuk pola "satu
// project per sesi kerja" yang jadi asumsi dasar personal tool ini.
// Kalau nanti butuh multi-root, ganti internal jadi map<string, Entry>
// — signature getOrBuild(root) tidak berubah.
//
// Thread-safe: getOrBuild() dan status() boleh dipanggil dari thread
// mana pun. Caller TIDAK boleh menyimpan reference ke graph yang
// dikembalikan melewati lifetime cache atau setelah getOrBuild(root_B)
// dipanggil dengan root_B != root_A (graph_ akan di-replace).
class ProjectGraphCache {
public:
    ProjectGraphCache() = default;

    ProjectGraphCache(const ProjectGraphCache&)            = delete;
    ProjectGraphCache& operator=(const ProjectGraphCache&) = delete;

    // Return graph untuk `project_root`. Build kalau:
    //   a) belum pernah build sama sekali, ATAU
    //   b) project_root BERBEDA dari cached_root_ (full rebuild, cache
    //      lama dibuang total — single-slot), ATAU
    //   c) needs_rebuild(project_root) == true
    //
    // needs_rebuild melakukan FULL SCAN direktori lalu bandingkan:
    //   1. Set nama file .py SEKARANG vs keys(mtimes_) — kalau BEDA
    //      (file baru ATAU file hilang) → rebuild.
    //   2. Kalau set sama, baru cek mtime tiap file — kalau ada yang
    //      berubah → rebuild.
    //
    // Langkah 1 wajib ada supaya file yang DIHAPUS dari project juga
    // terdeteksi — membandingkan mtime saja tidak cukup karena entri
    // yang hilang tidak akan pernah "berbeda mtime"-nya, dia cuma
    // tidak ada.
    const frans::code_intelligence::DependencyGraph& getOrBuild(
        const std::string& project_root);

    // Paksa rebuild pada panggilan berikutnya (tanpa hapus mtimes).
    void invalidate();

    struct Status {
        bool        valid           = false;
        std::string root;
        std::size_t file_count      = 0;
        std::size_t symbol_count    = 0;
        std::size_t edge_count      = 0;
        double      last_build_sec  = 0.0;
        int         build_count     = 0;
    };
    Status status() const;

private:
    bool needs_rebuild(const std::string& root) const;

    mutable std::mutex                                     mtx_;
    std::string                                            cached_root_;
    frans::code_intelligence::DependencyGraph              graph_;
    std::map<std::string, std::filesystem::file_time_type> mtimes_;
    Status                                                 status_;
};

} // namespace frans::application
