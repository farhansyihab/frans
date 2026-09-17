#pragma once

#include "domain/fs/IFileSystemPort.h"

#include <filesystem>
#include <string>

namespace frans::infrastructure {

class SandboxedFileSystem : public frans::domain::IFileSystemPort {
public:
    explicit SandboxedFileSystem(std::string root_path);

    SandboxedFileSystem(const SandboxedFileSystem&)            = delete;
    SandboxedFileSystem& operator=(const SandboxedFileSystem&) = delete;

    bool        writeFile(const std::string& path,
                          const std::string& content) override;
    std::string readFile(const std::string& path) override;
    bool        createDirectory(const std::string& path) override;
    bool        exists(const std::string& path) override;
    bool        deleteFile(const std::string& path) override;
    bool        isPathSafe(const std::string& path) const override;

    const std::filesystem::path& root() const { return canonical_root_; }

private:
    std::filesystem::path resolveSafe(const std::string& path) const;
    std::filesystem::path canonical_root_;
};

} // namespace frans::infrastructure