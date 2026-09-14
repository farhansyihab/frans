// ~/AgenAI/frans/src/infrastructure/system/DiffEngine.h
#pragma once

#include <string>
#include <vector>

namespace frans::infrastructure {

// Satu baris dalam hasil diff.
struct DiffLine {
    enum class Kind { Unchanged, Added, Removed };
    Kind        kind = Kind::Unchanged;
    std::string text;
};

// Hasil diff satu file.
struct FileDiff {
    std::string           path;
    std::vector<DiffLine> lines;
    int                   added_count   = 0;
    int                   removed_count = 0;
    bool                  is_new_file   = false;   // old kosong & new non-kosong
    bool                  is_deleted    = false;   // new kosong & old non-kosong
};

// LCS-based line diff. O(n*m) time & space.
// Untuk file < ~2000 baris (personal project) ini tidak terasa.
// Kalau nanti butuh file besar, upgrade ke Myers tanpa ubah interface.
FileDiff computeDiff(const std::string& path,
                     const std::string& old_content,
                     const std::string& new_content);

// Render sebagai unified-diff sederhana:
//   === path ===
//     unchanged line
//   - removed line
//   + added line
std::string formatDiff(const FileDiff& diff);

} // namespace frans::infrastructure
