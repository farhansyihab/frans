#include "application/git/GitFacade.h"
#include "infrastructure/system/ProcessExecutor.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

using namespace frans::application;
namespace fs = std::filesystem;

namespace {

// ---------------------------------------------------------------------------
// Helpers untuk integration test
// ---------------------------------------------------------------------------

std::string makeTempDir(const std::string& prefix) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    auto path = fs::temp_directory_path() / (prefix + "-" + std::to_string(dist(gen)));
    std::error_code ec;
    fs::create_directories(path, ec);
    if (ec) {
        throw std::runtime_error("Failed to create temp dir: " + ec.message());
    }
    return path.string();
}

// RAII wrapper untuk repo git temporer.
// - Membuat repo baru dengan `git init`
// - Konfigurasi user.name/email (wajib untuk commit)
// - Membuat initial commit supaya HEAD ada
// - Otomatis menghapus seluruh direktori saat destructor
class TempGitRepo {
public:
    TempGitRepo() {
        path_ = makeTempDir("frans-git-test");
        runGitDirect({"init"});
        runGitDirect({"config", "user.email", "test@frans.local"});
        runGitDirect({"config", "user.name", "Frans Test"});
        // Initial commit supaya HEAD valid
        writeFile("README.md", "# Test\n");
        runGitDirect({"add", "README.md"});
        runGitDirect({"commit", "-m", "Initial commit"});
    }

    ~TempGitRepo() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    TempGitRepo(const TempGitRepo&) = delete;
    TempGitRepo& operator=(const TempGitRepo&) = delete;

    const std::string& path() const { return path_; }

    void writeFile(const std::string& rel, const std::string& content) {
        auto full = fs::path(path_) / rel;
        std::error_code ec;
        fs::create_directories(full.parent_path(), ec);
        std::ofstream(full) << content;
    }

    frans::infrastructure::ProcessResult runGitDirect(
        const std::vector<std::string>& args)
    {
        std::vector<std::string> argv = {"git"};
        argv.insert(argv.end(), args.begin(), args.end());
        return frans::infrastructure::runCommand(
            argv, 30, {}, path_, nullptr, {});
    }

    static bool isGitAvailable() {
        auto result = frans::infrastructure::runCommand(
            {"git", "--version"}, 5, {}, {}, nullptr, {});
        return result.exit_code == 0;
    }

private:
    std::string path_;
};

// Buat bare repo lokal untuk dijadikan remote (push test).
// Return path kosong jika gagal.
std::string createBareRemote() {
    auto path = makeTempDir("frans-git-remote");
    auto result = frans::infrastructure::runCommand(
        {"git", "init", "--bare"}, 30, {}, path, nullptr, {});
    if (result.exit_code != 0) {
        std::error_code ec;
        fs::remove_all(path, ec);
        return {};
    }
    return path;
}

#define SKIP_IF_NO_GIT()                                              \
    do {                                                              \
        if (!TempGitRepo::isGitAvailable()) {                         \
            std::cout << "SKIP: " << __func__                         \
                      << " (git not available)\n";                    \
            return;                                                   \
        }                                                             \
    } while (0)

} // namespace

// ===========================================================================
// PURE TESTS (Fase D #67) — tidak butuh git binary
// ===========================================================================

void test_parseStatusPorcelain_clean() {
    std::string input = "## main...origin/main\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(status.is_repo);
    assert(status.branch == "main");
    assert(status.upstream == "origin/main");
    assert(status.ahead == 0);
    assert(status.behind == 0);
    assert(status.changes.empty());
    assert(status.clean());
    std::cout << "PASS: test_parseStatusPorcelain_clean\n";
}

void test_parseStatusPorcelain_ahead_behind() {
    std::string input = "## feature...origin/feature [ahead 3, behind 2]\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(status.is_repo);
    assert(status.branch == "feature");
    assert(status.upstream == "origin/feature");
    assert(status.ahead == 3);
    assert(status.behind == 2);
    std::cout << "PASS: test_parseStatusPorcelain_ahead_behind\n";
}

void test_parseStatusPorcelain_no_upstream() {
    std::string input = "## new-branch\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(status.is_repo);
    assert(status.branch == "new-branch");
    assert(status.upstream.empty());
    std::cout << "PASS: test_parseStatusPorcelain_no_upstream\n";
}

void test_parseStatusPorcelain_modified() {
    std::string input =
        "## main\n"
        " M src/main.cpp\n"
        "M  src/util.cpp\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(status.is_repo);
    assert(status.changes.size() == 2);
    assert(status.changes[0].index_status == ' ');
    assert(status.changes[0].worktree_status == 'M');
    assert(status.changes[0].path == "src/main.cpp");
    assert(status.changes[1].index_status == 'M');
    assert(status.changes[1].worktree_status == ' ');
    assert(status.changes[1].path == "src/util.cpp");
    std::cout << "PASS: test_parseStatusPorcelain_modified\n";
}

void test_parseStatusPorcelain_added() {
    std::string input =
        "## main\n"
        "A  new_file.cpp\n"
        "?? untracked.txt\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(status.changes.size() == 2);
    assert(status.changes[0].index_status == 'A');
    assert(status.changes[0].path == "new_file.cpp");
    assert(status.changes[1].index_status == '?');
    assert(status.changes[1].worktree_status == '?');
    assert(status.changes[1].path == "untracked.txt");
    std::cout << "PASS: test_parseStatusPorcelain_added\n";
}

void test_parseStatusPorcelain_deleted() {
    std::string input =
        "## main\n"
        "D  old_file.cpp\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(status.changes.size() == 1);
    assert(status.changes[0].index_status == 'D');
    assert(status.changes[0].path == "old_file.cpp");
    std::cout << "PASS: test_parseStatusPorcelain_deleted\n";
}

void test_parseStatusPorcelain_renamed() {
    std::string input =
        "## main\n"
        "R  old_name.cpp -> new_name.cpp\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(status.changes.size() == 1);
    assert(status.changes[0].index_status == 'R');
    assert(status.changes[0].path == "new_name.cpp");
    std::cout << "PASS: test_parseStatusPorcelain_renamed\n";
}

void test_parseStatusPorcelain_quoted_path() {
    std::string input =
        "## main\n"
        " M \"file with spaces.cpp\"\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(status.changes.size() == 1);
    assert(status.changes[0].path == "file with spaces.cpp");
    std::cout << "PASS: test_parseStatusPorcelain_quoted_path\n";
}

void test_parseStatusPorcelain_suspicious() {
    std::string input =
        "## main\n"
        "A  .env\n"
        "A  credentials.json\n"
        "A  normal_file.cpp\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(status.changes.size() == 3);
    assert(status.changes[0].suspicious == true);
    assert(status.changes[1].suspicious == true);
    assert(status.changes[2].suspicious == false);
    std::cout << "PASS: test_parseStatusPorcelain_suspicious\n";
}

void test_parseStatusPorcelain_empty() {
    std::string input = "";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(!status.is_repo);
    assert(!status.error.empty());
    std::cout << "PASS: test_parseStatusPorcelain_empty\n";
}

void test_parseStatusPorcelain_invalid() {
    std::string input = "Not a git repo\n";
    auto status = GitFacade::parseStatusPorcelain(input);
    assert(!status.is_repo);
    assert(!status.error.empty());
    std::cout << "PASS: test_parseStatusPorcelain_invalid\n";
}

void test_suggestCommitMessage_clean() {
    GitStatus status;
    status.is_repo = true;
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg == "No changes to commit");
    std::cout << "PASS: test_suggestCommitMessage_clean\n";
}

void test_suggestCommitMessage_single_added() {
    GitStatus status;
    status.is_repo = true;
    status.changes.push_back({'A', ' ', "new_file.cpp", false});
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg == "Add new_file.cpp");
    std::cout << "PASS: test_suggestCommitMessage_single_added\n";
}

void test_suggestCommitMessage_single_modified() {
    GitStatus status;
    status.is_repo = true;
    status.changes.push_back({'M', ' ', "src/main.cpp", false});
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg == "Update main.cpp");
    std::cout << "PASS: test_suggestCommitMessage_single_modified\n";
}

void test_suggestCommitMessage_single_deleted() {
    GitStatus status;
    status.is_repo = true;
    status.changes.push_back({'D', ' ', "old_file.cpp", false});
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg == "Remove old_file.cpp");
    std::cout << "PASS: test_suggestCommitMessage_single_deleted\n";
}

void test_suggestCommitMessage_multiple() {
    GitStatus status;
    status.is_repo = true;
    status.changes.push_back({'A', ' ', "file1.cpp", false});
    status.changes.push_back({'M', ' ', "file2.cpp", false});
    status.changes.push_back({'D', ' ', "file3.cpp", false});
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg.find("Update 3 files") != std::string::npos);
    assert(msg.find("1 added") != std::string::npos);
    assert(msg.find("1 modified") != std::string::npos);
    assert(msg.find("1 deleted") != std::string::npos);
    std::cout << "PASS: test_suggestCommitMessage_multiple\n";
}

void test_suggestCommitMessage_many_files() {
    GitStatus status;
    status.is_repo = true;
    for (int i = 0; i < 20; ++i) {
        status.changes.push_back({'M', ' ', "file" + std::to_string(i) + ".cpp", false});
    }
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg == "Update 20 files");
    std::cout << "PASS: test_suggestCommitMessage_many_files\n";
}

// ===========================================================================
// INTEGRATION TESTS (Fase D #68) — butuh git binary
// Semua test di-skip otomatis jika git tidak tersedia.
// ===========================================================================

void test_integration_status_clean() {
    SKIP_IF_NO_GIT();
    TempGitRepo repo;
    GitFacade facade(repo.path());

    auto status = facade.status();
    assert(status.is_repo);
    assert(status.git_available);
    assert(status.clean());
    // Default branch bisa "master" atau "main" tergantung git config global
    assert(!status.branch.empty());
    std::cout << "PASS: test_integration_status_clean (branch=" << status.branch << ")\n";
}

void test_integration_status_untracked() {
    SKIP_IF_NO_GIT();
    TempGitRepo repo;
    repo.writeFile("new.cpp", "int main() {}\n");

    GitFacade facade(repo.path());
    auto status = facade.status();

    assert(status.is_repo);
    assert(!status.clean());
    assert(status.changes.size() == 1);
    assert(status.changes[0].index_status == '?');
    assert(status.changes[0].worktree_status == '?');
    assert(status.changes[0].path == "new.cpp");
    assert(!status.changes[0].suspicious);
    std::cout << "PASS: test_integration_status_untracked\n";
}

void test_integration_status_modified() {
    SKIP_IF_NO_GIT();
    TempGitRepo repo;
    // Ubah file yang sudah di-commit
    repo.writeFile("README.md", "# Modified\n");

    GitFacade facade(repo.path());
    auto status = facade.status();

    assert(status.is_repo);
    assert(!status.clean());
    assert(status.changes.size() == 1);
    assert(status.changes[0].worktree_status == 'M');
    assert(status.changes[0].path == "README.md");
    std::cout << "PASS: test_integration_status_modified\n";
}

void test_integration_status_suspicious_file() {
    SKIP_IF_NO_GIT();
    TempGitRepo repo;
    repo.writeFile(".env", "SECRET=value\n");

    GitFacade facade(repo.path());
    auto status = facade.status();

    assert(status.changes.size() == 1);
    assert(status.changes[0].path == ".env");
    assert(status.changes[0].suspicious);
    std::cout << "PASS: test_integration_status_suspicious_file\n";
}

void test_integration_addAll() {
    SKIP_IF_NO_GIT();
    TempGitRepo repo;
    repo.writeFile("file1.cpp", "int a = 1;\n");
    repo.writeFile("file2.cpp", "int b = 2;\n");

    GitFacade facade(repo.path());

    // Sebelum addAll: 2 untracked
    auto status = facade.status();
    assert(status.changes.size() == 2);
    assert(status.changes[0].index_status == '?');

    // addAll
    auto add_result = facade.addAll();
    assert(add_result.ok);
    assert(add_result.exit_code == 0);
    assert(!add_result.cancelled);

    // Setelah addAll: 2 staged (index_status = 'A')
    status = facade.status();
    assert(status.changes.size() == 2);
    for (size_t i = 0; i < status.changes.size(); ++i) {
        assert(status.changes[i].index_status == 'A');
        assert(status.changes[i].worktree_status == ' ');
    }
    std::cout << "PASS: test_integration_addAll\n";
}

void test_integration_addAll_and_commit() {
    SKIP_IF_NO_GIT();
    TempGitRepo repo;
    repo.writeFile("file1.cpp", "int a = 1;\n");
    repo.writeFile("file2.cpp", "int b = 2;\n");

    GitFacade facade(repo.path());

    auto add_result = facade.addAll();
    assert(add_result.ok);

    auto commit_result = facade.commit("Add two files");
    assert(commit_result.ok);
    assert(commit_result.exit_code == 0);
    assert(!commit_result.cancelled);
    // Output commit biasanya berisi hash dan nama branch
    assert(!commit_result.output.empty());

    // Status harus clean setelah commit
    auto status = facade.status();
    assert(status.clean());
    std::cout << "PASS: test_integration_addAll_and_commit\n";
}

void test_integration_commit_empty_staging() {
    SKIP_IF_NO_GIT();
    TempGitRepo repo;
    GitFacade facade(repo.path());

    // Commit tanpa staged changes harus gagal
    auto result = facade.commit("Should fail");
    assert(!result.ok);
    // Error message dari implementasi kita
    assert(result.error.find("di-stage") != std::string::npos ||
           result.error.find("stage") != std::string::npos);
    std::cout << "PASS: test_integration_commit_empty_staging\n";
}

void test_integration_commit_message_with_special_chars() {
    SKIP_IF_NO_GIT();
    TempGitRepo repo;
    repo.writeFile("file.cpp", "int x = 1;\n");

    GitFacade facade(repo.path());
    facade.addAll();

    // Pesan commit dengan karakter khusus (kutip, newline tidak di argv)
    auto result = facade.commit("Fix: handle \"edge case\" & special <chars>");
    assert(result.ok);
    std::cout << "PASS: test_integration_commit_message_with_special_chars\n";
}

void test_integration_push_no_upstream() {
    SKIP_IF_NO_GIT();
    TempGitRepo repo;
    repo.writeFile("file.cpp", "int x = 1;\n");

    GitFacade facade(repo.path());
    facade.addAll();
    auto commit_result = facade.commit("Add file");
    assert(commit_result.ok);

    // Push tanpa remote/upstream harus gagal dengan pesan jelas
    auto push_result = facade.push();
    assert(!push_result.ok);
    assert(!push_result.error.empty());
    // Git akan error karena tidak ada remote
    std::cout << "PASS: test_integration_push_no_upstream (error: "
              << push_result.error.substr(0, 60) << "...)\n";
}

void test_integration_push_success() {
    SKIP_IF_NO_GIT();

    // Buat bare repo lokal sebagai remote
    std::string remote_path = createBareRemote();
    if (remote_path.empty()) {
        std::cout << "SKIP: test_integration_push_success (cannot create bare remote)\n";
        return;
    }

    {
        TempGitRepo repo;
        // Tambahkan remote origin
        repo.runGitDirect({"remote", "add", "origin", remote_path});

        // Buat commit pertama
        repo.writeFile("file.cpp", "int x = 1;\n");
        GitFacade facade(repo.path());
        facade.addAll();
        auto commit1 = facade.commit("First commit");
        assert(commit1.ok);

        // Push pertama: set upstream dengan git langsung
        // (karena GitFacade::push tidak auto -u, sesuai blueprint)
        auto push_setup = repo.runGitDirect({"push", "-u", "origin", "HEAD"});
        assert(push_setup.exit_code == 0);

        // Buat commit kedua
        repo.writeFile("file2.cpp", "int y = 2;\n");
        facade.addAll();
        auto commit2 = facade.commit("Second commit");
        assert(commit2.ok);

        // Push kedua via GitFacade harus sukses
        auto push_result = facade.push();
        assert(push_result.ok);
        assert(push_result.exit_code == 0);
    }

    std::error_code ec;
    fs::remove_all(remote_path, ec);
    std::cout << "PASS: test_integration_push_success\n";
}

void test_integration_status_ahead() {
    SKIP_IF_NO_GIT();

    std::string remote_path = createBareRemote();
    if (remote_path.empty()) {
        std::cout << "SKIP: test_integration_status_ahead (cannot create bare remote)\n";
        return;
    }

    {
        TempGitRepo repo;
        repo.runGitDirect({"remote", "add", "origin", remote_path});

        repo.writeFile("file.cpp", "int x = 1;\n");
        GitFacade facade(repo.path());
        facade.addAll();
        facade.commit("Initial");
        repo.runGitDirect({"push", "-u", "origin", "HEAD"});

        // Buat 2 commit baru tanpa push
        repo.writeFile("file2.cpp", "int y = 2;\n");
        facade.addAll();
        facade.commit("Second");

        repo.writeFile("file3.cpp", "int z = 3;\n");
        facade.addAll();
        facade.commit("Third");

        auto status = facade.status();
        assert(status.is_repo);
        assert(status.ahead == 2);
        assert(status.behind == 0);
    }

    std::error_code ec;
    fs::remove_all(remote_path, ec);
    std::cout << "PASS: test_integration_status_ahead\n";
}

void test_integration_status_not_a_repo() {
    // Buat temp dir TANPA git init
    auto path = makeTempDir("frans-not-repo");

    GitFacade facade(path);
    auto status = facade.status();

    assert(!status.is_repo);
    assert(status.git_available);  // git ada, tapi bukan repo
    assert(!status.error.empty());
    assert(status.error.find("Bukan git repository") != std::string::npos);

    std::error_code ec;
    fs::remove_all(path, ec);
    std::cout << "PASS: test_integration_status_not_a_repo\n";
}

void test_integration_full_workflow() {
    SKIP_IF_NO_GIT();

    // End-to-end: status -> addAll -> commit -> status
    TempGitRepo repo;

    // 1. Status awal: clean
    GitFacade facade(repo.path());
    auto status = facade.status();
    assert(status.clean());

    // 2. Tambah file baru
    repo.writeFile("src/main.cpp", "#include <iostream>\nint main() { return 0; }\n");
    repo.writeFile("src/util.h", "#pragma once\nvoid util();\n");

    // 3. Status: 2 untracked
    status = facade.status();
    assert(status.changes.size() == 2);

    // 4. Suggest commit message
    auto msg = GitFacade::suggestCommitMessage(status);
    assert(msg.find("Add") != std::string::npos || msg.find("Untracked") != std::string::npos);

    // 5. addAll
    auto add_result = facade.addAll();
    assert(add_result.ok);

    // 6. Commit dengan pesan saran
    auto commit_result = facade.commit(msg);
    assert(commit_result.ok);

    // 7. Status final: clean
    status = facade.status();
    assert(status.clean());

    std::cout << "PASS: test_integration_full_workflow\n";
}

// ===========================================================================
// Main
// ===========================================================================

int main() {
    std::cout << "=== GitFacade Tests ===\n\n";

    std::cout << "--- Pure tests (Fase D #67) ---\n";
    test_parseStatusPorcelain_clean();
    test_parseStatusPorcelain_ahead_behind();
    test_parseStatusPorcelain_no_upstream();
    test_parseStatusPorcelain_modified();
    test_parseStatusPorcelain_added();
    test_parseStatusPorcelain_deleted();
    test_parseStatusPorcelain_renamed();
    test_parseStatusPorcelain_quoted_path();
    test_parseStatusPorcelain_suspicious();
    test_parseStatusPorcelain_empty();
    test_parseStatusPorcelain_invalid();
    test_suggestCommitMessage_clean();
    test_suggestCommitMessage_single_added();
    test_suggestCommitMessage_single_modified();
    test_suggestCommitMessage_single_deleted();
    test_suggestCommitMessage_multiple();
    test_suggestCommitMessage_many_files();

    std::cout << "\n--- Integration tests (Fase D #68) ---\n";
    test_integration_status_clean();
    test_integration_status_untracked();
    test_integration_status_modified();
    test_integration_status_suspicious_file();
    test_integration_addAll();
    test_integration_addAll_and_commit();
    test_integration_commit_empty_staging();
    test_integration_commit_message_with_special_chars();
    test_integration_push_no_upstream();
    test_integration_push_success();
    test_integration_status_ahead();
    test_integration_status_not_a_repo();
    test_integration_full_workflow();

    std::cout << "\n=== All tests completed ===\n";
    return 0;
}