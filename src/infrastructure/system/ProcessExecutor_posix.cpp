#include "infrastructure/system/ProcessExecutor.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
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

    const std::string cmd_str = argvToStr(argv);
    std::fprintf(stderr,
        "[frans] runCommand START: %s (cwd=%s, timeout=%ds)\n",
        cmd_str.c_str(), working_dir.c_str(), timeout_sec);
    std::fflush(stderr);

    int in_pipe[2]  = {-1, -1};
    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};

    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        result.stderr_text = std::string("pipe: ") + std::strerror(errno);
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
        if (!stdin_data.empty()) {
            ::dup2(in_pipe[0], STDIN_FILENO);
            close_if_valid(in_pipe[0]);
            close_if_valid(in_pipe[1]);
        } else {
            const int devnull = ::open("/dev/null", O_RDONLY);
            if (devnull >= 0) { ::dup2(devnull, STDIN_FILENO); ::close(devnull); }
        }

        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::dup2(err_pipe[1], STDERR_FILENO);
        close_if_valid(out_pipe[0]); close_if_valid(out_pipe[1]);
        close_if_valid(err_pipe[0]); close_if_valid(err_pipe[1]);

        if (!working_dir.empty()) {
            if (::chdir(working_dir.c_str()) != 0) {
                std::fprintf(stderr, "chdir(%s): %s\n",
                             working_dir.c_str(), std::strerror(errno));
                ::_exit(126);
            }
        }

        std::vector<char*> c_argv;
        c_argv.reserve(argv.size() + 1);
        for (const auto& s : argv) c_argv.push_back(const_cast<char*>(s.c_str()));
        c_argv.push_back(nullptr);

        ::execvp(c_argv[0], c_argv.data());

        std::fprintf(stderr, "execvp(%s): %s\n", c_argv[0], std::strerror(errno));
        ::_exit(127);
    }

    close_if_valid(out_pipe[1]); out_pipe[1] = -1;
    close_if_valid(err_pipe[1]); err_pipe[1] = -1;

    if (!stdin_data.empty()) {
        close_if_valid(in_pipe[0]); in_pipe[0] = -1;
        const char* p = stdin_data.data();
        std::size_t remaining = stdin_data.size();
        while (remaining > 0) {
            const ssize_t w = ::write(in_pipe[1], p, remaining);
            if (w < 0) { if (errno == EINTR) continue; break; }
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
        if (!killed && cancel_flag && cancel_flag->load()) {
            result.cancelled_by_user = true;
            ::kill(pid, SIGKILL);
            killed = true;
        }

        if (!killed && std::chrono::steady_clock::now() > deadline) {
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
        if (p < 0) { if (errno == EINTR) continue; break; }
        if (p == 0) continue;

        for (int i = 0; i < nfds; ++i) {
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            const ssize_t r = ::read(fds[i].fd, buf, sizeof(buf));
            if (r > 0) {
                if (i == idx_out)      result.stdout_text.append(buf, static_cast<std::size_t>(r));
                else if (i == idx_err) result.stderr_text.append(buf, static_cast<std::size_t>(r));
            } else if (r == 0) {
                if (i == idx_out)      out_open = false;
                else if (i == idx_err) err_open = false;
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    if (i == idx_out)      out_open = false;
                    else if (i == idx_err) err_open = false;
                }
            }
        }
    }

    close_if_valid(out_pipe[0]);
    close_if_valid(err_pipe[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}

    if (WIFEXITED(status))         result.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))  result.exit_code = 128 + WTERMSIG(status);
    else                           result.exit_code = -1;

    std::fprintf(stderr, "[frans] runCommand DONE: %s -> exit=%d%s%s\n",
                 cmd_str.c_str(), result.exit_code,
                 result.timed_out ? " (TIMED OUT)" : "",
                 result.cancelled_by_user ? " (CANCELLED)" : "");
    std::fflush(stderr);

    return result;
}

} // namespace frans::infrastructure