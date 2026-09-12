// ~/AgenAI/frans/src/infrastructure/system/ProcessExecutor.cpp
#include "infrastructure/system/ProcessExecutor.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

namespace frans::infrastructure {

namespace {

void close_if_valid(int fd) {
    if (fd >= 0) ::close(fd);
}

std::string argvToStr(const std::vector<std::string>& argv) {
    std::string s;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) s += ' ';
        s += argv[i];
    }
    return s;
}

} // namespace

// ===========================================================================
// runCommand (foreground, existing)
// ===========================================================================

ProcessResult runCommand(const std::vector<std::string>& argv,
                         int timeout_sec,
                         const std::string& stdin_data) {
    ProcessResult result;
    if (argv.empty()) {
        result.stderr_text = "runCommand: argv kosong";
        return result;
    }

    const std::string cmd_str = argvToStr(argv);
    std::fprintf(stderr, "[frans] runCommand START: %s  (timeout=%ds, stdin=%zu bytes)\n",
                 cmd_str.c_str(), timeout_sec, stdin_data.size());
    std::fflush(stderr);

    int in_pipe[2]  = {-1, -1};
    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};

    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        result.stderr_text = std::string("pipe: ") + std::strerror(errno);
        std::fprintf(stderr, "[frans] runCommand PIPE FAIL: %s\n", result.stderr_text.c_str());
        return result;
    }
    if (!stdin_data.empty() && pipe(in_pipe) != 0) {
        result.stderr_text = std::string("pipe(stdin): ") + std::strerror(errno);
        close_if_valid(out_pipe[0]); close_if_valid(out_pipe[1]);
        close_if_valid(err_pipe[0]); close_if_valid(err_pipe[1]);
        return result;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        result.stderr_text = std::string("fork: ") + std::strerror(errno);
        close_if_valid(in_pipe[0]);  close_if_valid(in_pipe[1]);
        close_if_valid(out_pipe[0]); close_if_valid(out_pipe[1]);
        close_if_valid(err_pipe[0]); close_if_valid(err_pipe[1]);
        return result;
    }

    if (pid == 0) {
        // ---------- child ----------
        if (!stdin_data.empty()) {
            ::dup2(in_pipe[0], STDIN_FILENO);
            close_if_valid(in_pipe[0]);
            close_if_valid(in_pipe[1]);
        } else {
            const int devnull = ::open("/dev/null", O_RDONLY);
            if (devnull >= 0) {
                ::dup2(devnull, STDIN_FILENO);
                ::close(devnull);
            }
        }

        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::dup2(err_pipe[1], STDERR_FILENO);
        close_if_valid(out_pipe[0]); close_if_valid(out_pipe[1]);
        close_if_valid(err_pipe[0]); close_if_valid(err_pipe[1]);

        std::vector<char*> c_argv;
        c_argv.reserve(argv.size() + 1);
        for (const auto& s : argv) {
            c_argv.push_back(const_cast<char*>(s.c_str()));
        }
        c_argv.push_back(nullptr);

        ::execvp(c_argv[0], c_argv.data());

        std::fprintf(stderr, "execvp(%s): %s\n", c_argv[0], std::strerror(errno));
        ::_exit(127);
    }

    // ---------- parent ----------
    close_if_valid(out_pipe[1]); out_pipe[1] = -1;
    close_if_valid(err_pipe[1]); err_pipe[1] = -1;

    if (!stdin_data.empty()) {
        close_if_valid(in_pipe[0]); in_pipe[0] = -1;
        const char* p = stdin_data.data();
        std::size_t remaining = stdin_data.size();
        while (remaining > 0) {
            const ssize_t w = ::write(in_pipe[1], p, remaining);
            if (w < 0) {
                if (errno == EINTR) continue;
                break;
            }
            p += w;
            remaining -= static_cast<std::size_t>(w);
        }
        close_if_valid(in_pipe[1]); in_pipe[1] = -1;
    }

    (void)::fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    (void)::fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

    const int effective_timeout = timeout_sec > 0 ? timeout_sec : 30;
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::seconds(effective_timeout);

    bool out_open = true;
    bool err_open = true;
    bool killed = false;
    char buf[4096];

    while (out_open || err_open) {
        if (!killed && std::chrono::steady_clock::now() > deadline) {
            std::fprintf(stderr, "[frans] runCommand TIMEOUT (%ds) -> SIGKILL pid=%d: %s\n",
                         effective_timeout, static_cast<int>(pid), cmd_str.c_str());
            std::fflush(stderr);
            result.timed_out = true;
            ::kill(pid, SIGKILL);
            killed = true;
        }

        struct pollfd fds[2];
        int nfds = 0;
        int idx_out = -1, idx_err = -1;
        if (out_open) { fds[nfds].fd = out_pipe[0]; fds[nfds].events = POLLIN; idx_out = nfds++; }
        if (err_open) { fds[nfds].fd = err_pipe[0]; fds[nfds].events = POLLIN; idx_err = nfds++; }

        const int p = ::poll(fds, static_cast<nfds_t>(nfds), 200);
        if (p < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (p == 0) continue;

        for (int i = 0; i < nfds; ++i) {
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            const ssize_t r = ::read(fds[i].fd, buf, sizeof(buf));
            if (r > 0) {
                if (i == idx_out) result.stdout_text.append(buf, static_cast<std::size_t>(r));
                else if (i == idx_err) result.stderr_text.append(buf, static_cast<std::size_t>(r));
            } else if (r == 0) {
                if (i == idx_out) out_open = false;
                else if (i == idx_err) err_open = false;
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    if (i == idx_out) out_open = false;
                    else if (i == idx_err) err_open = false;
                }
            }
        }
    }

    close_if_valid(out_pipe[0]);
    close_if_valid(err_pipe[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        // retry
    }

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    } else {
        result.exit_code = -1;
    }

    std::fprintf(stderr, "[frans] runCommand DONE : %s  -> exit=%d%s\n",
                 cmd_str.c_str(),
                 result.exit_code,
                 result.timed_out ? " (TIMED OUT)" : "");
    std::fflush(stderr);

    return result;
}

// ===========================================================================
// spawnBackground
// ===========================================================================

SpawnResult spawnBackground(const std::vector<std::string>& argv,
                            const std::string& stdout_log_path,
                            const std::string& stderr_log_path) {
    SpawnResult result;
    if (argv.empty()) {
        result.error_message = "spawnBackground: argv kosong";
        return result;
    }

    const std::string cmd_str = argvToStr(argv);
    std::fprintf(stderr, "[frans] spawnBackground START: %s\n", cmd_str.c_str());
    std::fprintf(stderr, "[frans]   stdout -> %s\n", stdout_log_path.c_str());
    std::fprintf(stderr, "[frans]   stderr -> %s\n", stderr_log_path.c_str());
    std::fflush(stderr);

    const pid_t pid = ::fork();
    if (pid < 0) {
        result.error_message = std::string("fork: ") + std::strerror(errno);
        std::fprintf(stderr, "[frans] spawnBackground FORK FAIL: %s\n", result.error_message.c_str());
        return result;
    }

    if (pid == 0) {
        // ---------- child ----------
        // stdin -> /dev/null
        const int devnull = ::open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            ::close(devnull);
        }

        // stdout -> file (truncate)
        const int out_fd = ::open(stdout_log_path.c_str(),
                                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) {
            std::fprintf(stderr, "spawnBackground: open stdout log '%s': %s\n",
                         stdout_log_path.c_str(), std::strerror(errno));
            ::_exit(126);
        }
        ::dup2(out_fd, STDOUT_FILENO);
        ::close(out_fd);

        // stderr -> file (truncate)
        const int err_fd = ::open(stderr_log_path.c_str(),
                                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (err_fd < 0) {
            std::fprintf(stderr, "spawnBackground: open stderr log '%s': %s\n",
                         stderr_log_path.c_str(), std::strerror(errno));
            ::_exit(126);
        }
        ::dup2(err_fd, STDERR_FILENO);
        ::close(err_fd);

        std::vector<char*> c_argv;
        c_argv.reserve(argv.size() + 1);
        for (const auto& s : argv) {
            c_argv.push_back(const_cast<char*>(s.c_str()));
        }
        c_argv.push_back(nullptr);

        ::execvp(c_argv[0], c_argv.data());

        std::fprintf(stderr, "execvp(%s): %s\n", c_argv[0], std::strerror(errno));
        ::_exit(127);
    }

    // ---------- parent ----------
    result.pid = pid;
    std::fprintf(stderr, "[frans] spawnBackground OK: pid=%d\n", static_cast<int>(pid));
    std::fflush(stderr);
    return result;
}

// ===========================================================================
// reapIfExited
// ===========================================================================

bool reapIfExited(pid_t pid) {
    if (pid <= 0) return false;

    int status = 0;
    const pid_t r = ::waitpid(pid, &status, WNOHANG);
    if (r == pid) return false;   // exited & reaped
    if (r == 0)   return true;    // still running
    return false;                 // ECHILD atau error lain -> anggap mati
}

// ===========================================================================
// isTcpPortOpen
// ===========================================================================

bool isTcpPortOpen(const std::string& host, int port, int timeout_ms) {
    struct addrinfo hints {};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    std::snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo* res = nullptr;
    const int gai = ::getaddrinfo(host.c_str(), port_str, &hints, &res);
    if (gai != 0 || res == nullptr) {
        return false;
    }

    bool connected = false;
    for (struct addrinfo* rp = res; rp != nullptr && !connected; rp = rp->ai_next) {
        const int fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;

        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        const int rc = ::connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (rc == 0) {
            connected = true;
        } else if (errno == EINPROGRESS) {
            struct pollfd pfd;
            pfd.fd     = fd;
            pfd.events = POLLOUT;
            const int pr = ::poll(&pfd, 1, timeout_ms);
            if (pr > 0 && (pfd.revents & POLLOUT)) {
                int soerr = 0;
                socklen_t len = sizeof(soerr);
                if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len) == 0 && soerr == 0) {
                    connected = true;
                }
            }
        }

        ::close(fd);
    }

    ::freeaddrinfo(res);
    return connected;
}

} // namespace frans::infrastructure
