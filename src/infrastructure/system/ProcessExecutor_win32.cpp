// Implementasi Windows pakai CreateProcess + pipe + Job Object.
//
// Job Object dipakai untuk memastikan Cancel & Timeout benar-benar
// membunuh SELURUH process tree (cmd.exe + child-nya seperti ping.exe,
// node.exe, npm.cmd, dst). Tanpa ini, TerminateProcess hanya bunuh
// proses langsung, sementara child tetap jalan sampai selesai.
//
// Referensi: 
//   https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects
//   JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE

#include "infrastructure/system/ProcessExecutor.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace frans::infrastructure {

namespace {

std::string joinArgv(const std::vector<std::string>& argv) {
    std::string s;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) s += ' ';
        // Quote sederhana; tidak menangani embedded quote.
        if (argv[i].find(' ') != std::string::npos) {
            s += '"' + argv[i] + '"';
        } else {
            s += argv[i];
        }
    }
    return s;
}

std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
        static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
        static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
        static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
        static_cast<int>(s.size()), out.data(), n);
    return out;
}

// Kill seluruh job — semua proses yang di-assign ke job ini (termasuk
// child dari cmd.exe) akan dibunuh.
void killJob(HANDLE job) {
    if (job && job != INVALID_HANDLE_VALUE) {
        ::TerminateJobObject(job, 1);
    }
}

} // namespace

ProcessResult runCommand(const std::vector<std::string>& argv,
                         int timeout_sec,
                         const std::string& stdin_data,
                         const std::string& working_dir,
                         std::atomic<bool>* cancel_flag) {
    ProcessResult result;
    if (argv.empty()) {
        result.stderr_text = "runCommand: argv kosong";
        return result;
    }

    const std::string cmd_str = joinArgv(argv);
    std::fprintf(stderr, "[frans] runCommand START: %s (cwd=%s)\n",
        cmd_str.c_str(), working_dir.c_str());
    std::fflush(stderr);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE out_r = nullptr, out_w = nullptr;
    HANDLE err_r = nullptr, err_w = nullptr;
    HANDLE in_r  = nullptr, in_w  = nullptr;

    if (!::CreatePipe(&out_r, &out_w, &sa, 0)) {
        result.stderr_text = "CreatePipe(stdout) gagal";
        return result;
    }
    if (!::CreatePipe(&err_r, &err_w, &sa, 0)) {
        ::CloseHandle(out_r); ::CloseHandle(out_w);
        result.stderr_text = "CreatePipe(stderr) gagal";
        return result;
    }
    if (!stdin_data.empty()) {
        if (!::CreatePipe(&in_r, &in_w, &sa, 0)) {
            ::CloseHandle(out_r); ::CloseHandle(out_w);
            ::CloseHandle(err_r); ::CloseHandle(err_w);
            result.stderr_text = "CreatePipe(stdin) gagal";
            return result;
        }
    }

    ::SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
    if (in_w) ::SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);

    // ---- Job Object ----
    // KILL_ON_JOB_CLOSE: saat handle job ditutup, semua proses di
    // dalamnya dibunuh. Kita juga panggil TerminateJobObject() eksplisit
    // untuk timeout/cancel — lebih cepat dari menunggu CloseHandle.
    HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jinfo = {};
        jinfo.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!::SetInformationJobObject(job,
                JobObjectExtendedLimitInformation,
                &jinfo, sizeof(jinfo))) {
            std::fprintf(stderr,
                "[frans] SetInformationJobObject gagal (err=%lu)\n",
                ::GetLastError());
            std::fflush(stderr);
            ::CloseHandle(job);
            job = nullptr;
        }
    } else {
        std::fprintf(stderr,
            "[frans] CreateJobObject gagal (err=%lu) — timeout/cancel "
            "hanya akan bunuh proses langsung\n", ::GetLastError());
        std::fflush(stderr);
    }

    STARTUPINFOW si{};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = out_w;
    si.hStdError  = err_w;
    si.hStdInput  = in_r ? in_r : ::GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

    std::wstring cmdw = utf8ToWide(cmd_str);
    std::vector<wchar_t> cmd_buf(cmdw.begin(), cmdw.end());
    cmd_buf.push_back(L'\0');

    std::wstring cwdw = working_dir.empty() ? std::wstring{}
                                             : utf8ToWide(working_dir);

    // CREATE_SUSPENDED: bikin proses dulu tanpa jalan, assign ke job,
    // baru ResumeThread. Ini mencegah race di mana child sempat spawn
    // subprocess sebelum di-assign ke job.
    //
    // CREATE_NO_WINDOW: jangan tampilkan console window untuk cmd.exe
    // saat frans.exe dijalankan sebagai GUI subsystem.
    const DWORD creation_flags = CREATE_SUSPENDED | CREATE_NO_WINDOW;

    const BOOL ok = ::CreateProcessW(
        nullptr, cmd_buf.data(),
        nullptr, nullptr, TRUE, creation_flags,
        nullptr,
        cwdw.empty() ? nullptr : cwdw.c_str(),
        &si, &pi);

    ::CloseHandle(out_w);
    ::CloseHandle(err_w);
    if (in_r) ::CloseHandle(in_r);

    if (!ok) {
        ::CloseHandle(out_r); ::CloseHandle(err_r);
        if (in_w) ::CloseHandle(in_w);
        if (job)  ::CloseHandle(job);
        result.stderr_text = "CreateProcess gagal (err="
                           + std::to_string(::GetLastError()) + ")";
        return result;
    }

    // Assign ke job SEBELUM proses jalan.
    if (job) {
        if (!::AssignProcessToJobObject(job, pi.hProcess)) {
            std::fprintf(stderr,
                "[frans] AssignProcessToJobObject gagal (err=%lu) — "
                "timeout/cancel hanya akan bunuh proses langsung\n",
                ::GetLastError());
            std::fflush(stderr);
        }
    }

    // Resume proses yang tadinya di-suspend.
    if (::ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) {
        std::fprintf(stderr,
            "[frans] ResumeThread gagal (err=%lu)\n", ::GetLastError());
        std::fflush(stderr);
    }

    // Kirim stdin kalau ada.
    if (!stdin_data.empty() && in_w) {
        DWORD written = 0;
        ::WriteFile(in_w, stdin_data.data(),
                    static_cast<DWORD>(stdin_data.size()), &written, nullptr);
        ::CloseHandle(in_w);
        in_w = nullptr;
    }

    // Baca stdout + stderr paralel.
    auto readPipe = [](HANDLE h, std::string& out) {
        char buf[4096];
        DWORD r = 0;
        while (::ReadFile(h, buf, sizeof(buf), &r, nullptr) && r > 0) {
            out.append(buf, r);
        }
    };

    std::thread t_out([&]{ readPipe(out_r, result.stdout_text); });
    std::thread t_err([&]{ readPipe(err_r, result.stderr_text); });

    const DWORD timeout_ms = timeout_sec > 0
        ? static_cast<DWORD>(timeout_sec) * 1000u
        : 30000u;

    const auto start = std::chrono::steady_clock::now();
    bool killed = false;

    for (;;) {
        const DWORD w = ::WaitForSingleObject(pi.hProcess, 200);
        if (w == WAIT_OBJECT_0) break;

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (static_cast<DWORD>(elapsed) >= timeout_ms) {
            result.timed_out = true;
            // Kill seluruh job (cmd + ping child).
            if (job) killJob(job);
            else     ::TerminateProcess(pi.hProcess, 1);
            killed = true;
            break;
        }
        if (cancel_flag && cancel_flag->load()) {
            result.cancelled_by_user = true;
            if (job) killJob(job);
            else     ::TerminateProcess(pi.hProcess, 1);
            killed = true;
            break;
        }
    }

    if (!killed) ::WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD ec = 0;
    if (::GetExitCodeProcess(pi.hProcess, &ec)) {
        result.exit_code = static_cast<int>(ec);
    }

    // Tutup pipe supaya thread pembaca selesai.
    ::CloseHandle(out_r);
    ::CloseHandle(err_r);
    t_out.join();
    t_err.join();

    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
    if (job) ::CloseHandle(job);

    std::fprintf(stderr, "[frans] runCommand DONE: %s -> exit=%d\n",
        cmd_str.c_str(), result.exit_code);
    std::fflush(stderr);
    return result;
}

} // namespace frans::infrastructure