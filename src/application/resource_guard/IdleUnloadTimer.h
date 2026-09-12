// ~/AgenAI/frans/src/application/resource_guard/IdleUnloadTimer.h
#pragma once

#include <chrono>
#include <mutex>

namespace frans::application {

// Blueprint §5.4 — idleUnloadTimer.
//
// Menghitung mundur sejak aktivitas terakhir. Caller (UI timer) memanggil
// markActivity() setiap kali server menerima request / user berinteraksi,
// dan expired() untuk cek apakah sudah lewat timeout.
//
// Thread-safe: markActivity() dipanggil dari worker thread (HTTP handler di
// masa depan / Stage 4), sementara expired()/remaining() dari main thread.
class IdleUnloadTimer {
public:
    explicit IdleUnloadTimer(std::chrono::seconds timeout);

    void setTimeout(std::chrono::seconds timeout);
    std::chrono::seconds timeout() const;

    // Aktifkan timer. Waktu mulai dianggap "sekarang".
    void start();
    // Nonaktifkan. expired() berikutnya = false.
    void stop();

    // Reset countdown. Kalau belum running, otomatis start().
    void markActivity();

    bool isRunning() const;
    bool expired() const;
    std::chrono::seconds remaining() const;

private:
    mutable std::mutex mtx_;
    std::chrono::seconds timeout_;
    std::chrono::steady_clock::time_point last_activity_{};
    bool running_ = false;
};

} // namespace frans::application
