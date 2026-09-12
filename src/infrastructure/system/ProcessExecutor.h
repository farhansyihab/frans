// ~/AgenAI/frans/src/infrastructure/system/ProcessExecutor.h
#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

namespace frans::infrastructure {

// ---------------------------------------------------------------------------
// runCommand: jalankan proses foreground, tunggu selesai.
// ---------------------------------------------------------------------------

struct ProcessResult {
    int exit_code = -1;          // -1 jika gagal fork/exec, atau 128+signal
    bool timed_out = false;
    std::string stdout_text;
    std::string stderr_text;
};

// Jalankan argv (argv[0] di-resolve via PATH oleh execvp) dan tunggu selesai.
//
// - timeout_sec: >0 = batas detik; jika lewat, proses di-SIGKILL.
// - stdin_data : jika tidak kosong, ditulis ke stdin child sebelum close.
//                Data diasumsikan kecil (< 64 KiB) — cukup untuk "3\n" dsb.
ProcessResult runCommand(const std::vector<std::string>& argv,
                         int timeout_sec = 30,
                         const std::string& stdin_data = {});

// ---------------------------------------------------------------------------
// spawnBackground: launch long-running process TANPA menunggu.
// Dipakai oleh LlamaServerProcessController (blueprint §5.2).
// ---------------------------------------------------------------------------

struct SpawnResult {
    pid_t pid = -1;                 // > 0 kalau sukses
    std::string error_message;      // terisi kalau gagal fork

    bool ok() const { return pid > 0; }
};

// Fork + exec argv[0] di background.
// stdout & stderr di-redirect ke file log (dibuat/truncate).
// stdin di-redirect ke /dev/null.
//
// Return: SpawnResult dengan pid child. Caller WAJIB reap child via
// waitpid(pid, WNOHANG) saat process berhenti (lihat reapIfExited).
//
// Catatan: child TIDAK di-setsid — kita butuh pid untuk SIGTERM/SIGKILL
// dan reaping. Kalau ingin full detach, caller harus handle sendiri.
SpawnResult spawnBackground(const std::vector<std::string>& argv,
                            const std::string& stdout_log_path,
                            const std::string& stderr_log_path);

// Cek apakah child dengan pid tsb masih hidup. Kalau sudah exit, reap
// (waitpid WNOHANG) dan return false. Kalau masih jalan, return true.
bool reapIfExited(pid_t pid);

// Utility: cek apakah TCP port di host:port bisa di-connect.
// timeout_ms = batas connect attempt (non-blocking connect + poll).
bool isTcpPortOpen(const std::string& host, int port, int timeout_ms = 500);

} // namespace frans::infrastructure
