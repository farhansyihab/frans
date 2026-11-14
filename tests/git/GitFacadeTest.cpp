#include "application/git/GitFacade.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace frans::application;

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

int main() {
    std::cout << "Running GitFacade tests...\n\n";
    
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
    
    std::cout << "\nAll tests passed!\n";
    return 0;
}