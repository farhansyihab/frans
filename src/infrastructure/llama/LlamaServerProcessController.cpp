// ~/AgenAI/frans/src/infrastructure/llama/LlamaServerProcessController.cpp
#include "infrastructure/llama/LlamaServerProcessController.h"

#include "infrastructure/http/HttpProbe.h"
#include "infrastructure/system/ProcessExecutor.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>

namespace frans::infrastructure {

LlamaServerProcessController::LlamaServerProcessController(Config cfg)
    : cfg_(std::move(cfg)) {}

LlamaServerProcessController::~LlamaServerProcessController() {
    stop();
}

std::vector<std::string>
LlamaServerProcessController::buildArgv(const LaunchSpec& spec) const {
    return {
        cfg_.binary_path,
        "-m",    spec.model_path,
        "-c",    std::to_string(spec.context_size),
        "-t",    std::to_string(spec.threads),
        "-ngl",  std::to_string(spec.n_gpu_layers),
        "--host", cfg_.host,
        "--port", std::to_string(cfg_.port),
    };
}

bool LlamaServerProcessController::checkAliveLocked() {
    if (pid_ <= 0) return false;

    int status = 0;
    const pid_t r = ::waitpid(pid_, &status, WNOHANG);
    if (r == pid_) {
        pid_ = -1;
        return false;
    }
    if (r == 0) return true;
    pid_ = -1;
    return false;
}

bool LlamaServerProcessController::start(const LaunchSpec& spec) {
    std::string err_log_snapshot;

    {
        std::lock_guard<std::mutex> lk(mtx_);

        if (pid_ > 0) {
            last_error_ = "server sudah berjalan (pid=" + std::to_string(pid_) + ")";
            return false;
        }
        if (cfg_.binary_path.empty()) {
            last_error_ = "binary path kosong (cek providers.json .llama_server.binary)";
            return false;
        }
        if (spec.model_path.empty()) {
            last_error_ = "model path kosong";
            return false;
        }

        std::error_code ec;
        if (!std::filesystem::exists(cfg_.binary_path, ec)) {
            last_error_ = "binary tidak ditemukan: " + cfg_.binary_path;
            return false;
        }
        if (!std::filesystem::exists(spec.model_path, ec)) {
            last_error_ = "model file tidak ditemukan: " + spec.model_path;
            return false;
        }

        std::string log_dir = cfg_.log_dir.empty() ? std::string("/tmp") : cfg_.log_dir;
        std::filesystem::create_directories(log_dir, ec);

        stdout_log_path_ = log_dir + "/llama-server.stdout.log";
        stderr_log_path_ = log_dir + "/llama-server.stderr.log";
        err_log_snapshot = stderr_log_path_;

        const auto argv = buildArgv(spec);
        const SpawnResult sr = spawnBackground(argv, stdout_log_path_, stderr_log_path_);
        if (!sr.ok()) {
            last_error_ = sr.error_message;
            return false;
        }

        pid_ = sr.pid;
        last_error_.clear();
    }

    // Deteksi dini exec failure: beri 300 ms, lalu cek proses masih hidup.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!checkAliveLocked()) {
            last_error_ = "llama-server exit dini setelah start. Cek log: " + err_log_snapshot;
            return false;
        }
    }
    return true;
}

void LlamaServerProcessController::stop() {
    pid_t pid = -1;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        pid = pid_;
        pid_ = -1;
    }

    if (pid <= 0) return;

    std::fprintf(stderr, "[frans] llama-server: SIGTERM pid=%d\n", static_cast<int>(pid));
    std::fflush(stderr);
    ::kill(pid, SIGTERM);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        int status = 0;
        const pid_t r = ::waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            std::fprintf(stderr, "[frans] llama-server: exit cleanly (status=%d)\n", status);
            std::fflush(stderr);
            return;
        }
        if (r < 0 && errno == ECHILD) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::fprintf(stderr, "[frans] llama-server: SIGKILL pid=%d (SIGTERM timeout)\n",
                 static_cast<int>(pid));
    std::fflush(stderr);
    ::kill(pid, SIGKILL);
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        // retry
    }
}

bool LlamaServerProcessController::isRunning() {
    std::lock_guard<std::mutex> lk(mtx_);
    return checkAliveLocked();
}

// ---------------------------------------------------------------------------
// waitUntilReady — 2 fase:
//   1. TCP probe cepat: kalau port belum open, tunggu (murah, cepat).
//   2. HTTP /health probe: hanya lolos kalau status_code == 200.
//      llama-server mengembalikan 503 "Loading model" selama load — itu
//      akan terus di-poll sampai model benar-benar siap.
// ---------------------------------------------------------------------------
bool LlamaServerProcessController::waitUntilReady(int timeout_sec) {
    pid_t pid_snapshot;
    std::string host;
    int port;
    int cfg_timeout;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        pid_snapshot = pid_;
        host         = cfg_.host;
        port         = cfg_.port;
        cfg_timeout  = cfg_.startup_timeout_sec;
    }

    if (pid_snapshot <= 0) {
        std::lock_guard<std::mutex> lk(mtx_);
        last_error_ = "waitUntilReady: server tidak berjalan";
        return false;
    }

    const int total = timeout_sec > 0 ? timeout_sec : cfg_timeout;
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::seconds(total);

    std::fprintf(stderr,
        "[frans] waitUntilReady: polling http://%s:%d/health (timeout=%ds)\n",
        host.c_str(), port, total);
    std::fflush(stderr);

    auto last_log = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() < deadline) {
        // Proses masih hidup?
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (!checkAliveLocked()) {
                last_error_ = "llama-server mati sebelum ready. Cek log: " + stderr_log_path_;
                return false;
            }
        }

        // Cek HTTP /health
        const auto probe = httpGet(host, port, "/health", /*timeout_sec=*/3);

        if (probe.reachable && probe.status_code == 200) {
            std::fprintf(stderr,
                "[frans] llama-server READY di %s:%d (health=200)\n",
                host.c_str(), port);
            std::fflush(stderr);
            return true;
        }

        // Log progress tiap 5 detik supaya user tahu masih loading
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() >= 5) {
            if (probe.reachable) {
                std::fprintf(stderr,
                    "[frans] waitUntilReady: health=%d (masih loading model...)\n",
                    probe.status_code);
            } else {
                std::fprintf(stderr,
                    "[frans] waitUntilReady: belum bisa connect (%s)\n",
                    probe.error.c_str());
            }
            std::fflush(stderr);
            last_log = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    {
        std::lock_guard<std::mutex> lk(mtx_);
        last_error_ = "timeout " + std::to_string(total) + "s menunggu "
                    + host + ":" + std::to_string(port) + " ready (/health != 200)";
    }
    return false;
}

std::string LlamaServerProcessController::baseUrl() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return "http://" + cfg_.host + ":" + std::to_string(cfg_.port);
}

std::string LlamaServerProcessController::lastError() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return last_error_;
}

std::string LlamaServerProcessController::logPath() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return stderr_log_path_;
}

pid_t LlamaServerProcessController::pid() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return pid_;
}

} // namespace frans::infrastructure
