// ~/AgenAI/frans/src/application/task_runner/TaskRunner.h
#pragma once

#include "application/agent/Agent.h"
#include "application/agent/AgentContext.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace frans::application {

// Snapshot status TaskRunner — dibaca dari main thread untuk update UI.
struct TaskProgress {
    bool                       running        = false;
    bool                       done           = false;
    bool                       success        = false;
    bool                       cancelled      = false;
    std::string                current_state;
    int                        attempt        = 0;
    int                        max_attempts   = 0;
    double                     elapsed_sec    = 0.0;
    std::string                last_message;
    std::vector<AttemptRecord> history;
    std::vector<GeneratedFile> files;
    std::string                stdout_text;
    std::string                stderr_text;
    std::string                final_error;

    // ---- Stage 7f.5 — test-aware execution ----
    // `last_test_ran == false` → tidak ada test file terdeteksi.
    // `last_test_ran == true` → test dijalankan; exit_code == 0 berarti
    //   pass (entry point kemudian dijalankan), exit_code != 0 berarti
    //   fail (entry point SKIPPED).
    bool                       last_test_ran        = false;
    std::string                last_test_file;
    int                        last_test_exit_code  = -1;
};

// Blueprint §3 — TaskRunner.
//
// Wrapper worker-thread di atas Agent. Tugasnya:
//   1. Spawn worker thread yang menjalankan Agent::run(ctx).
//   2. Terima progress callback dari Agent, simpan di state bersama.
//   3. Terima confirmation request dari ExecuteState (block worker),
//      expose ke main thread lewat polling, resolve dari main thread.
//   4. Honour cancellation request.
//
// Pola threading mengikuti blueprint §10:
//   - Main thread: TaskRunner::poll() + takePendingConfirmMessage() +
//     resolvePendingConfirm().
//   - Worker thread: Agent::run() → workerConfirm() (block condvar).
//
// TaskRunner TIDAK menyentuh widget FLTK.
class TaskRunner {
public:
    TaskRunner();
    ~TaskRunner();

    TaskRunner(const TaskRunner&)            = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    bool start(Agent::Deps deps, AgentContext ctx);
    void poll();
    void requestCancel();

    std::string takePendingConfirmMessage();
    void        resolvePendingConfirm(bool result);

    TaskProgress snapshot() const;
    bool         isRunning() const { return running_.load(); }

private:
    void workerMain(Agent::Deps deps, AgentContext ctx);
    bool workerConfirm(const std::string& message);

    std::atomic<bool>  running_{false};
    std::atomic<bool>  cancel_requested_{false};
    std::thread        thread_;

    mutable std::mutex progress_mtx_;
    TaskProgress       progress_;
    std::chrono::steady_clock::time_point start_time_{};

    struct PendingConfirm {
        std::string message;
        bool        has_new  = false;
        bool        shown    = false;
        bool        resolved = false;
        bool        result   = false;
    };

    std::mutex               confirm_mtx_;
    std::condition_variable  confirm_cv_;
    PendingConfirm           pending_confirm_;
};

} // namespace frans::application
