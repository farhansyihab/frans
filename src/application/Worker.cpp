// ~/AgenAI/frans/src/application/Worker.cpp
#include "application/Worker.h"

#include <cstdio>
#include <thread>
#include <utility>

namespace frans::application {

Worker::Worker(BusyListener on_busy, DoneListener on_done)
    : on_busy_(std::move(on_busy)), on_done_(std::move(on_done)) {}

Worker::~Worker() = default;

bool Worker::spawn(const std::string& label, Task task) {
    if (busy_.exchange(true)) {
        std::fprintf(stderr,
            "[frans] Worker::spawn DITOLAK: masih ada worker lain.\n");
        std::fflush(stderr);
        return false;
    }

    busy_started_ = std::chrono::steady_clock::now();
    if (on_busy_) on_busy_(true, label);

    std::thread([this, label, task = std::move(task)]() mutable {
        std::fprintf(stderr, "[frans] worker START: %s\n", label.c_str());
        std::fflush(stderr);

        frans::domain::CommandResult r = task();

        std::fprintf(stderr,
            "[frans] worker DONE : %s (exit=%d success=%d)\n",
            label.c_str(), r.exit_code, r.success ? 1 : 0);
        std::fflush(stderr);

        auto payload = std::make_unique<WorkerDone>(
            WorkerDone{r.success, std::move(r.message), label});

        {
            std::lock_guard<std::mutex> lk(mtx_);
            pending_ = std::move(payload);
        }
    }).detach();

    return true;
}

void Worker::poll() {
    std::unique_ptr<WorkerDone> ready;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        ready = std::move(pending_);
    }
    if (!ready) return;

    std::fprintf(stderr, "[frans] worker poll: deliver '%s' (success=%d)\n",
                 ready->label.c_str(), ready->success ? 1 : 0);
    std::fflush(stderr);

    busy_.store(false);
    if (on_busy_) on_busy_(false, "");
    if (on_done_) on_done_(*ready);
}

void Worker::watchdog(int timeout_sec) {
    if (!busy_.load()) return;

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - busy_started_).count();
    if (elapsed < timeout_sec) return;

    std::fprintf(stderr,
        "[frans] WATCHDOG: worker busy %llds — force re-enable UI.\n",
        static_cast<long long>(elapsed));
    std::fflush(stderr);

    forceReset();
}

void Worker::forceReset() {
    busy_.store(false);
    if (on_busy_) on_busy_(false, "");
}

} // namespace frans::application
