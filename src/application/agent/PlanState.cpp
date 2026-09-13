// ~/AgenAI/frans/src/application/agent/PlanState.cpp
#include "application/agent/PlanState.h"

#include "application/agent/AgentContext.h"

#include <cctype>
#include <string>
#include <vector>

namespace frans::application {

namespace {

std::string toLower(std::string s) {
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool containsAny(const std::string& haystack,
                 const std::vector<std::string>& needles)
{
    for (const auto& n : needles) {
        if (haystack.find(n) != std::string::npos) return true;
    }
    return false;
}

TaskComplexity classify(const std::string& task) {
    const std::string low = toLower(task);

    static const std::vector<std::string> kComplex = {
        "refactor", "architecture", "redesign", "restructure",
        "migrate", "rewrite", "optimi", "concurrent", "parallel",
        "across files", "multi-file", "whole project",
    };
    if (containsAny(low, kComplex)) {
        return TaskComplexity::Complex;
    }

    static const std::vector<std::string> kTrivial = {
        "typo", "rename", "one line", "single line",
        "add print", "add comment",
    };
    if (containsAny(low, kTrivial)) {
        return TaskComplexity::Trivial;
    }

    return TaskComplexity::Moderate;
}

int maxAttemptsFor(TaskComplexity c) {
    switch (c) {
        case TaskComplexity::Trivial:               return 2;
        case TaskComplexity::Moderate:              return 3;
        case TaskComplexity::Complex:               return 3;
        case TaskComplexity::RequiresDeepReasoning: return 4;
    }
    return 3;
}

} // namespace

void PlanState::run(AgentContext& ctx) {
    ctx.success = false;
    ctx.lastError.clear();

    // ---- 1. Validasi input ----
    if (ctx.taskDescription.empty()) {
        ctx.lastError = "PlanState: taskDescription kosong.";
        return;
    }
    if (ctx.targetRelativePath.empty()) {
        ctx.lastError = "PlanState: targetRelativePath kosong.";
        return;
    }

    // ---- 2. Klasifikasi ----
    ctx.plannedComplexity = classify(ctx.taskDescription);

    // ---- 3. Reset loop state ----
    ctx.attempt     = 0;
    ctx.maxAttempts = maxAttemptsFor(ctx.plannedComplexity);
    ctx.attemptHistory.clear();
    ctx.lastObservation.clear();
    ctx.lastExecution.reset();
    ctx.lastTest.reset();          // Stage 7f.5 — reset hasil test run sebelumnya
    // ctx.lastGeneratedCode & ctx.contextSnippets SENGAJA tidak di-clear:
    // GenerateState yang akan mengisinya di attempt 0.

    ctx.success = true;
}

} // namespace frans::application
