#pragma once

#include <atomic>
#include <string>
#include <vector>

namespace frans::infrastructure {

struct ProcessResult {
    int  exit_code = -1;
    bool timed_out = false;
    bool cancelled_by_user = false;
    std::string stdout_text;
    std::string stderr_text;
};

// Jalankan argv (argv[0] di-resolve via PATH) dan tunggu selesai.
//
// - timeout_sec : >0 = batas detik; kalau lewat, proses di-kill.
// - stdin_data  : kalau tidak kosong, ditulis ke stdin child.
// - working_dir : kalau tidak kosong, child chdir ke sini SEBELUM exec.
// - cancel_flag : kalau non-null dan bernilai true, kill child.
// - env_extra   : daftar "KEY=VALUE" yang di-setenv di child sebelum exec
//                 (POSIX). Entri tanpa '=' atau dengan key kosong akan
//                 di-skip dengan warning. Di Win32 saat ini belum
//                 diimplementasikan (akan dikerjakan di #72); entri
//                 non-kosong akan memicu warning di stderr.
//
// Fase D #66: backward-compatible; default env_extra = {}.
ProcessResult runCommand(const std::vector<std::string>& argv,
                         int timeout_sec = 30,
                         const std::string& stdin_data = {},
                         const std::string& working_dir = {},
                         std::atomic<bool>* cancel_flag = nullptr,
                         const std::vector<std::string>& env_extra = {});

} // namespace frans::infrastructure