#pragma once

#include "application/import/MultiFileParser.h"
#include "application/build_runner/ProjectInfo.h"

#include <string>
#include <vector>

namespace frans::application {

// Deteksi project root + build system dari marker file.
//
// Walk-up dari direktori file ke parent sampai ketemu marker
// (CMakeLists.txt, package.json, dst).
class ProjectDetector {
public:
    static ProjectInfo detect(
        const std::vector<GeneratedFile>& files_written,
        const std::string& sandbox_root);
};

} // namespace frans::application