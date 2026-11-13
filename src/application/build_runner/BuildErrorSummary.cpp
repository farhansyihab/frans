// src/application/build_runner/BuildErrorSummary.cpp
#include "application/build_runner/BuildErrorSummary.h"

#include <string>

namespace frans::application {
namespace {

// Nama phase versi lokal.
//
// Sengaja TIDAK memanggil frans::application::buildPhaseName() yang
// didefinisikan di BuildRunner.cpp: supaya modul ini bisa di-link ke
// target test yang ringan (tanpa BuildRunner + semua BuildSystems +
// ProcessExecutor + Threads). Duplikasinya hanya satu switch 3 baris.
const char* phaseName(BuildPhase p) {
    switch (p) {
        case BuildPhase::Build: return "BUILD";
        case BuildPhase::Test:  return "TEST";
        case BuildPhase::Run:   return "RUN";
    }
    return "?";
}

const PhaseResult* failedPhaseResult(const BuildReport& r) {
    if (r.build.phase == r.failed_phase && !r.build.success) return &r.build;
    if (r.test.phase  == r.failed_phase && !r.test.success)  return &r.test;
    if (r.run.phase   == r.failed_phase && !r.run.success)   return &r.run;
    return nullptr;
}

bool phaseCancelled(const PhaseResult& pr) {
    for (const auto& cmd : pr.commands) {
        if (cmd.cancelled) return true;
    }
    return pr.error_message.find("Dibatalkan") != std::string::npos;
}

} // namespace

std::string summarizeBuildError(const BuildReport& r,
                                std::size_t max_stderr_bytes)
{
    if (r.overall_success) return {};

    std::string s;
    s += "Build FAILED pada phase ";
    s += phaseName(r.failed_phase);
    s += " (";
    s += r.build_system;
    s += ").\n";

    const PhaseResult* failed = failedPhaseResult(r);
    if (!failed) return s;

    if (!failed->error_message.empty()) {
        s += "Error: ";
        s += failed->error_message;
        s += "\n";
    }

    if (!failed->related_files.empty()) {
        s += "Related files:\n";
        for (const auto& rf : failed->related_files) {
            s += "  * " + rf.path;
            if (rf.line > 0) s += ":" + std::to_string(rf.line);
            if (!rf.note.empty()) s += " -- " + rf.note;
            s += "\n";
        }
    }

    // Tail stderr dari command PERTAMA yang gagal.
    for (const auto& cmd : failed->commands) {
        if (cmd.success || cmd.stderr_tail.empty()) continue;

        s += "stderr tail:\n";
        const std::string& t = cmd.stderr_tail;
        if (max_stderr_bytes > 0 && t.size() > max_stderr_bytes) {
            s += "...(truncated)...\n";
            s += t.substr(t.size() - max_stderr_bytes);
        } else {
            s += t;
        }
        if (!t.empty() && t.back() != '\n') s += "\n";
        break;
    }

    return s;
}

bool wasCancelledByUser(const BuildReport& r) {
    return phaseCancelled(r.build)
        || phaseCancelled(r.test)
        || phaseCancelled(r.run);
}

} // namespace frans::application