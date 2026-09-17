#pragma once

#include <string>
#include <vector>

namespace frans::infrastructure {

struct DiffLine {
    enum class Kind { Unchanged, Added, Removed };
    Kind        kind = Kind::Unchanged;
    std::string text;
};

struct FileDiff {
    std::string           path;
    std::vector<DiffLine> lines;
    int                   added_count   = 0;
    int                   removed_count = 0;
    bool                  is_new_file   = false;
    bool                  is_deleted    = false;
};

FileDiff computeDiff(const std::string& path,
                     const std::string& old_content,
                     const std::string& new_content);

std::string formatDiff(const FileDiff& diff);

} // namespace frans::infrastructure