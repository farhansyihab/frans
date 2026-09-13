// ~/AgenAI/frans/src/application/Worker.h
#pragma once

#include "domain/command/CommandResult.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace frans::application {

// Hasil worker, diserahkan ke main thread lewat poll().
struct WorkerDone {
    bool        success = false;
    std::string message;
    std::string label;
};

// Menjalankan satu task di background thread. Hanya satu task pada
// satu waktu (busy flag).
//
// Alur:
//   Worker w(on_busy, on_done);
//   w.spawn("Clean Memory", []{ ... });  // dari main thread (button click)
//   // Setelah itu, dari timer main-thread:
//   w.poll();         // deliver hasil ke on_done, reset busy
//   w.watchdog(180);  // paksa reset kalau menggantung
//
// Semua widget HARUS di-update dari main thread. Karena itu spawn() hanya
// memicu on_busy(true) sinkron, sedangkan hasil task diserahkan lewat
// poll() yang dipanggil dari timer.
class Worker {
public:
    using Task         = std::function<frans::domain::CommandResult()>;
    using BusyListener = std::function<void(bool busy, const std::string& label)>;
    using DoneListener = std::function<void(const WorkerDone&)>;

    Worker(BusyListener on_busy, DoneListener on_done);
    ~Worker();

    Worker(const Worker&)            = delete;
    Worker& operator=(const Worker&) = delete;

    // Return false kalau masih ada task lain berjalan.
    bool spawn(const std::string& label, Task task);

    bool busy() const { return busy_.load(); }

    // Dipanggil dari timer main-thread.
    void poll();
    void watchdog(int timeout_sec);

    // Paksa reset state busy. Task yang masih berjalan akan tetap selesai di
    // background, tapi UI tidak lagi terkunci. Dipakai oleh watchdog() dan
    // tombol "Reset UI".
    void forceReset();

private:
    BusyListener on_busy_;
    DoneListener on_done_;

    std::atomic<bool> busy_{false};
    std::chrono::steady_clock::time_point busy_started_{};

    std::mutex                  mtx_;
    std::unique_ptr<WorkerDone> pending_;
};

} // namespace frans::application
