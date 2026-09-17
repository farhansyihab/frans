#pragma once

#include <string>

namespace frans::domain {

// Satu file hasil generasi model.
//
// `relative_path` selalu relatif terhadap sandbox root (bukan absolut).
// `content` sudah di-trim trailing newline.
struct GeneratedFile {
    std::string relative_path;
    std::string content;
};

} // namespace frans::domain