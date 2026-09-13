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
// TaskRunner TIDAK menyentuh widget FLTK. Semua interaksi UI lewat
// metode publik yang dipanggil dari main thread.
class TaskRunner {
public:
    TaskRunner();
    ~TaskRunner();

    TaskRunner(const TaskRunner&)            = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    // Start task asinkron. Return false kalau sudah ada task berjalan.
    // `deps` di-copy; caller tetap memiliki object yang ditunjuk pointer
    // (provider, fs) — WAJIB hidup selama task berjalan.
    //
    // Field `confirm`, `cancel_flag`, `on_state` di `deps` AKAN
    // di-override oleh TaskRunner (chain untuk on_state, override penuh
    // untuk confirm & cancel_flag).
    bool start(Agent::Deps deps, AgentContext ctx);

    // Dipanggil dari timer main thread (mis. 100ms).
    // Update internal elapsed, consumer bisa baca via snapshot().
    void poll();

    // Minta pembatalan. Worker cek di batas state.
    void requestCancel();

    // ---- Confirm bridge (worker → main thread → worker) ----
    //
    // Kalau ada request baru, return pesan (non-empty) dan tandai sebagai
    // "sudah ditampilkan". Caller WAJIB segera memanggil
    // resolvePendingConfirm() setelah dialog selesai.
    //
    // Kalau tidak ada request baru, return string kosong.
    std::string takePendingConfirmMessage();

    // Jawab request yang sedang aktif. Idempotent-ish: kalau tidak ada
    // request aktif, no-op.
    void resolvePendingConfirm(bool result);

    // Snapshot status. Thread-safe.
    TaskProgress snapshot() const;
    bool         isRunning() const { return running_.load(); }

private:
    void workerMain(Agent::Deps deps, AgentContext ctx);
    bool workerConfirm(const std::string& message);

    // ---- task control ----
    std::atomic<bool>  running_{false};
    std::atomic<bool>  cancel_requested_{false};
    std::thread        thread_;

    // ---- progress state (mutex-protected) ----
    mutable std::mutex progress_mtx_;
    TaskProgress       progress_;
    std::chrono::steady_clock::time_point start_time_{};

    // ---- confirm bridge ----
    struct PendingConfirm {
        std::string message;
        bool        has_new  = false;   // worker set, belum di-take UI
        bool        shown    = false;   // sudah di-take, dialog in progress
        bool        resolved = false;
        bool        result   = false;
    };

    std::mutex               confirm_mtx_;
    std::condition_variable  confirm_cv_;
    PendingConfirm           pending_confirm_;
};

} // namespace frans::application
