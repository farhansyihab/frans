#pragma once

#include <string>
#include <vector>

namespace frans::application {

struct RelatedFile {
    std::string path;
    int         line = 0;
    std::string role;
    std::string note;
};

enum class BuildPhase {
    Build,
    Test,
    Run,
};

const char* buildPhaseName(BuildPhase p);

struct CommandResult {
    std::string label;
    std::string command;
    bool        ran         = true;
    bool        success     = false;
    int         exit_code   = -1;
    bool        timed_out   = false;
    bool        cancelled   = false;
    double      duration_sec = 0.0;
    std::string stdout_tail;
    std::string stderr_tail;
};

struct PhaseResult {
    BuildPhase  phase       = BuildPhase::Build;
    bool        skipped     = true;
    bool        success     = false;
    double      duration_sec = 0.0;
    std::string error_message;
    std::vector<RelatedFile> related_files;
    std::vector<CommandResult> commands;

    bool ran() const { return !skipped; }
};

struct BuildReport {
    std::string project_root;
    std::string sandbox_root;
    std::string build_system;
    std::string test_runner;
    std::string primary_language;

    PhaseResult build;
    PhaseResult test;
    PhaseResult run;

    bool        overall_success   = false;
    double      total_duration_sec = 0.0;
    BuildPhase  failed_phase      = BuildPhase::Build;

    bool has_any_phase_ran() const {
        return build.ran() || test.ran() || run.ran();
    }
    bool is_success() const { return overall_success; }
};

} // namespace frans::application