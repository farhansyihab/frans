// ~/AgenAI/frans/src/infrastructure/llama/LlamaServerProcessController.h
#pragma once

#include <mutex>
#include <string>
#include <sys/types.h>
#include <vector>

namespace frans::infrastructure {

// Blueprint §5.2 — ILlamaServerController (implementasi konkret).
//
// RAII: destructor memanggil stop() otomatis. Non-copyable.
//
// Alur pakai:
//   LlamaServerProcessController ctrl(cfg);
//   if (ctrl.start(spec)) {
//       if (ctrl.waitUntilReady()) { /* siap dipakai */ }
//   }
//   ctrl.stop();   // atau biarkan destructor
//
// Semua method boleh dipanggil dari worker thread. Method state-query
// (isRunning, lastError, logPath, pid) aman dari thread lain.
class LlamaServerProcessController {
public:
    struct Config {
        std::string binary_path;
        std::string host = "127.0.0.1";
        int port = 8080;
        int startup_timeout_sec = 120;
        std::string log_dir;
    };

    struct LaunchSpec {
        std::string model_path;
        int context_size  = 4096;
        int threads       = 4;
        int n_gpu_layers  = 0;   // -ngl 0, WAJIB untuk laptop tanpa GPU diskrit
    };

    explicit LlamaServerProcessController(Config cfg);
    ~LlamaServerProcessController();

    LlamaServerProcessController(const LlamaServerProcessController&) = delete;
    LlamaServerProcessController& operator=(const LlamaServerProcessController&) = delete;

    // Start llama-server dengan spec. Return true kalau proses berhasil
    // di-spawn DAN tidak exit dalam 300 ms pertama (exec failure deteksi dini).
    bool start(const LaunchSpec& spec);

    // SIGTERM -> tunggu 5s -> SIGKILL. Idempotent.
    void stop();

    // Cek apakah server masih hidup (reap kalau sudah exit).
    bool isRunning();

    // Poll TCP port host:port sampai terbuka atau timeout.
    // timeout_sec = -1 -> pakai cfg_.startup_timeout_sec.
    bool waitUntilReady(int timeout_sec = -1);

    std::string baseUrl() const;       // "http://host:port"
    std::string lastError() const;     // pesan error terakhir (thread-safe)
    std::string logPath() const;       // path stderr log (untuk dibuka user)
    pid_t       pid() const;           // -1 kalau tidak jalan

private:
    // Panggil dengan mtx_ held.
    bool checkAliveLocked();
    std::vector<std::string> buildArgv(const LaunchSpec& spec) const;

    Config      cfg_;
    pid_t       pid_ = -1;
    std::string last_error_;
    std::string stdout_log_path_;
    std::string stderr_log_path_;
    mutable std::mutex mtx_;
};

} // namespace frans::infrastructure
