#pragma once
#include <atomic>
#include <string>
#include <vector>

namespace frans::application {

struct GitFileChange {
    char index_status = ' ';
    char worktree_status = ' ';
    std::string path;
    bool suspicious = false;
};

struct GitStatus {
    bool is_repo = false;
    bool git_available = true;
    std::string branch;
    std::string upstream;
    std::string error;
    int ahead = 0;
    int behind = 0;
    std::vector<GitFileChange> changes;
    
    bool clean() const { return is_repo && changes.empty(); }
};

struct GitOpResult {
    bool ok = false;
    int exit_code = -1;
    bool cancelled = false;
    std::string output;
    std::string error;
};

class GitFacade {
public:
    explicit GitFacade(std::string project_root);
    
    GitStatus   status(std::atomic<bool>* cancel = nullptr);
    GitOpResult addAll(std::atomic<bool>* cancel = nullptr);
    GitOpResult commit(const std::string& message, std::atomic<bool>* cancel = nullptr);
    GitOpResult push(std::atomic<bool>* cancel = nullptr);
    
    static GitStatus   parseStatusPorcelain(const std::string& text);
    static std::string suggestCommitMessage(const GitStatus& st);

private:
    std::string project_root_;
};

} // namespace frans::application