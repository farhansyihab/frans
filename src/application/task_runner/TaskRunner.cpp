// ~/AgenAI/frans/src/application/task_runner/TaskRunner.cpp
#include "application/task_runner/TaskRunner.h"

#include <chrono>
#include <cstdio>
#include <utility>

namespace frans::application {

using namespace std::chrono;

TaskRunner::TaskRunner() = default;

TaskRunner::~TaskRunner() {
    requestCancel();
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool TaskRunner::start(Agent::Deps deps, AgentContext ctx) {
    if (running_.exchange(true)) {
        std::fprintf(stderr,
            "[frans][task_runner] start DITOLAK: masih ada task berjalan.\n");
        std::fflush(stderr);
        return false;
    }

    cancel_requested_.store(false);

    {
        std::lock_guard<std::mutex> lk(progress_mtx_);
        progress_ = TaskProgress{};
        progress_.running = true;
        progress_.max_attempts = ctx.maxAttempts;
        start_time_ = steady_clock::now();
    }

    const Agent::StateCallback user_on_state = deps.on_state;

    deps.confirm     = [this](const std::string& msg) { return workerConfirm(msg); };
    deps.cancel_flag = &cancel_requested_;
    deps.on_state    = [this, user_on_state](const std::string& state, int attempt) {
        {
            std::lock_guard<std::mutex> lk(progress_mtx_);
            progress_.current_state = state;
            progress_.attempt       = attempt;
            progress_.elapsed_sec   =
                duration<double>(steady_clock::now() - start_time_).count();
        }
        if (user_on_state) user_on_state(state, attempt);
    };

    if (thread_.joinable()) thread_.join();

    thread_ = std::thread([this,
                            deps_moved = std::move(deps),
                            ctx_moved  = std::move(ctx)]() mutable {
        workerMain(std::move(deps_moved), std::move(ctx_moved));
    });

    return true;
}

void TaskRunner::poll() {
    if (!running_.load()) return;

    std::lock_guard<std::mutex> lk(progress_mtx_);
    if (progress_.running) {
        progress_.elapsed_sec =
            duration<double>(steady_clock::now() - start_time_).count();
    }
}

void TaskRunner::requestCancel() {
    if (!running_.load()) return;
    cancel_requested_.store(true);
    std::fprintf(stderr, "[frans][task_runner] cancel diminta.\n");
    std::fflush(stderr);
}

void TaskRunner::workerMain(Agent::Deps deps, AgentContext ctx) {
    std::fprintf(stderr,
        "[frans][task_runner] worker START untuk task: %.60s...\n",
        ctx.taskDescription.c_str());
    std::fflush(stderr);

    Agent agent(deps);
    const bool ok = agent.run(ctx);

    const bool was_cancelled = cancel_requested_.load();

    {
        std::lock_guard<std::mutex> lk(progress_mtx_);
        progress_.running      = false;
        progress_.done         = true;
        progress_.success      = ok;
        progress_.cancelled    = was_cancelled;
        progress_.attempt      = ctx.attempt;
        progress_.max_attempts = ctx.maxAttempts;
        progress_.last_message = ctx.lastObservation;
        progress_.history      = ctx.attemptHistory;
        progress_.files        = ctx.generatedFiles;
        progress_.stdout_text  = ctx.lastExecution.stdout_text;
        progress_.stderr_text  = ctx.lastExecution.stderr_text;
        progress_.final_error  = ctx.lastError;
        progress_.elapsed_sec  =
            duration<double>(steady_clock::now() - start_time_).count();

        // Stage 7g — bawa snapshot untuk DiffEngine.
        progress_.file_snapshots = ctx.fileSnapshots;

        // Stage 7f.5 — bawa hasil test ke UI.
        progress_.last_test_ran       = ctx.lastTest.ran;
        progress_.last_test_file      = ctx.lastTest.test_file;
        progress_.last_test_exit_code = ctx.lastTest.exit_code;
    }

    std::fprintf(stderr,
        "[frans][task_runner] worker DONE: success=%d cancelled=%d "
        "attempts=%zu test_ran=%d test_exit=%d snapshots=%zu\n",
        ok ? 1 : 0, was_cancelled ? 1 : 0, ctx.attemptHistory.size(),
        ctx.lastTest.ran ? 1 : 0, ctx.lastTest.exit_code,
        ctx.fileSnapshots.size());
    std::fflush(stderr);

    running_.store(false);
}

bool TaskRunner::workerConfirm(const std::string& message) {
    std::unique_lock<std::mutex> lk(confirm_mtx_);

    pending_confirm_.message  = message;
    pending_confirm_.has_new  = true;
    pending_confirm_.shown    = false;
    pending_confirm_.resolved = false;
    pending_confirm_.result   = false;

    std::fprintf(stderr,
        "[frans][task_runner] worker MENUNGGU konfirmasi user.\n");
    std::fflush(stderr);

    confirm_cv_.wait(lk, [&] {
        return pending_confirm_.resolved;
    });

    const bool r = pending_confirm_.result;
    pending_confirm_.has_new  = false;
    pending_confirm_.shown    = false;
    pending_confirm_.resolved = false;

    std::fprintf(stderr,
        "[frans][task_runner] konfirmasi resolved: %s\n", r ? "YES" : "NO");
    std::fflush(stderr);
    return r;
}

std::string TaskRunner::takePendingConfirmMessage() {
    std::lock_guard<std::mutex> lk(confirm_mtx_);
    if (!pending_confirm_.has_new) return {};
    std::string msg = pending_confirm_.message;
    pending_confirm_.has_new = false;
    pending_confirm_.shown   = true;
    return msg;
}

void TaskRunner::resolvePendingConfirm(bool result) {
    std::lock_guard<std::mutex> lk(confirm_mtx_);
    if (!pending_confirm_.shown) return;
    pending_confirm_.result   = result;
    pending_confirm_.resolved = true;
    confirm_cv_.notify_all();
}

TaskProgress TaskRunner::snapshot() const {
    std::lock_guard<std::mutex> lk(progress_mtx_);
    return progress_;
}

} // namespace frans::application
