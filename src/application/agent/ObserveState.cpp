// ~/AgenAI/frans/src/application/agent/ObserveState.cpp
#include "application/agent/ObserveState.h"

#include "application/agent/AgentContext.h"

#include <cctype>
#include <string>
#include <vector>

namespace frans::application {

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string tailOf(const std::string& s, std::size_t n) {
    return s.size() <= n ? s : s.substr(s.size() - n);
}

// Cari "line N" terakhir di string `s` SEBELUM posisi `end_pos`.
// Return -1 kalau tidak ketemu. Dipakai baik untuk traceback maupun
// SyntaxError.
int lastLineNumberBefore(const std::string& s, std::size_t end_pos) {
    int last = -1;
    std::size_t cursor = 0;
    while (cursor < end_pos) {
        const std::size_t found = s.find("line ", cursor);
        if (found == std::string::npos || found >= end_pos) break;

        const std::size_t num_start = found + 5;
        std::size_t num_end = num_start;
        while (num_end < s.size()
               && std::isdigit(static_cast<unsigned char>(s[num_end]))) {
            ++num_end;
        }
        if (num_end > num_start) {
            try {
                last = std::stoi(s.substr(num_start, num_end - num_start));
            } catch (...) {
                // ignore
            }
        }
        cursor = num_end;
    }
    return last;
}

// ---------------------------------------------------------------------------
// SyntaxError parser
//
// Format khas:
//   File "path", line 3
//     print(2+2
//           ^
//   SyntaxError: '(' was never closed
//
// Tidak ada "Traceback (most recent call last):" header di kasus ini.
// ---------------------------------------------------------------------------

struct SyntaxErrorInfo {
    std::string message;
    int         line = -1;
    bool        found = false;
};

SyntaxErrorInfo parseSyntaxError(const std::string& stderr) {
    SyntaxErrorInfo s;
    const std::size_t pos = stderr.find("SyntaxError:");
    if (pos == std::string::npos) return s;

    s.found = true;

    const std::size_t eol = stderr.find('\n', pos);
    s.message = stderr.substr(
        pos + 12,
        eol == std::string::npos ? std::string::npos : eol - pos - 12);
    while (!s.message.empty() && s.message.front() == ' ') {
        s.message.erase(0, 1);
    }

    s.line = lastLineNumberBefore(stderr, pos);
    return s;
}

// ---------------------------------------------------------------------------
// Python traceback parser
//
// Format khas:
//   Traceback (most recent call last):
//     File "path", line 3, in <module>
//       fail()
//     File "path", line 2, in fail
//       raise ValueError('oops')
//   ValueError: oops
//
// Kita hanya peduli exception TERAKHIR (kita tidak menangani chained
// exception via __cause__/__context__ — out of scope Stage 7d).
// ---------------------------------------------------------------------------

struct PythonError {
    std::string exception_type;
    std::string exception_message;
    int         line = -1;
    std::string function;
    bool        found = false;
};

PythonError parsePythonTraceback(const std::string& stderr) {
    PythonError e;

    const std::string marker = "Traceback (most recent call last):";
    const std::size_t start = stderr.find(marker);
    if (start == std::string::npos) return e;

    const std::string body = stderr.substr(start);

    // Line number + function name: cari occurrence terakhir dari
    // "line N, in <func>" di dalam body.
    {
        std::size_t cursor = 0;
        while (true) {
            const std::size_t found = body.find("line ", cursor);
            if (found == std::string::npos) break;

            const std::size_t num_start = found + 5;
            std::size_t num_end = num_start;
            while (num_end < body.size()
                   && std::isdigit(static_cast<unsigned char>(body[num_end]))) {
                ++num_end;
            }
            if (num_end > num_start) {
                try {
                    e.line = std::stoi(
                        body.substr(num_start, num_end - num_start));
                } catch (...) {
                    // ignore
                }
            }

            const std::size_t in_pos = body.find(", in ", num_end);
            if (in_pos != std::string::npos && in_pos < num_end + 40) {
                const std::size_t fname_start = in_pos + 5;
                std::size_t fname_end = fname_start;
                while (fname_end < body.size() &&
                       (std::isalnum(static_cast<unsigned char>(body[fname_end])) ||
                        body[fname_end] == '_' ||
                        body[fname_end] == '<' ||
                        body[fname_end] == '>')) {
                    ++fname_end;
                }
                e.function = body.substr(fname_start, fname_end - fname_start);
            }
            cursor = num_end;
        }
    }

    // Baris exception terakhir: split body jadi lines, scan dari belakang,
    // cari baris yang tidak mulai dengan whitespace dan berbentuk
    // "TypeName: message".
    std::vector<std::string> lines;
    {
        std::size_t p = 0;
        while (p < body.size()) {
            std::size_t nl = body.find('\n', p);
            if (nl == std::string::npos) nl = body.size();
            lines.emplace_back(body.substr(p, nl - p));
            p = nl + 1;
        }
    }

    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        const std::string& L = *it;
        if (L.empty()) continue;
        if (std::isspace(static_cast<unsigned char>(L[0]))) continue;
        if (L.find("Traceback") != std::string::npos) continue;

        const std::size_t colon = L.find(':');
        if (colon == std::string::npos) continue;

        const std::string maybe_type = L.substr(0, colon);
        if (maybe_type.empty()) continue;
        if (!std::isupper(static_cast<unsigned char>(maybe_type[0]))) continue;

        e.exception_type    = maybe_type;
        e.exception_message = L.substr(colon + 1);
        while (!e.exception_message.empty()
               && e.exception_message.front() == ' ') {
            e.exception_message.erase(0, 1);
        }
        e.found = true;
        return e;
    }

    return e;
}

} // namespace

// ---------------------------------------------------------------------------
// ObserveState::run
// ---------------------------------------------------------------------------
void ObserveState::run(AgentContext& ctx) {
    // Simpan prior error — bisa jadi ExecuteState memotong sebelum
    // runCommand (blocked, file tidak ada, dsb.) dan sudah mengisi
    // lastError dengan alasan yang informatif.
    const std::string prior_error = ctx.lastError;

    ctx.success = false;
    ctx.lastError.clear();
    ctx.lastObservation.clear();

    // ---- Kasus 1: eksekusi sukses ----
    if (ctx.lastExecution.is_success()) {
        ctx.lastObservation = "Execution succeeded (exit=0, no timeout).";
        ctx.success = true;
        return;
    }

    // ---- Kasus 2: eksekusi tidak pernah jalan ----
    // (`ran == false`: dipotong di ExecuteState sebelum runCommand.)
    if (!ctx.lastExecution.ran) {
        if (!prior_error.empty()) {
            ctx.lastObservation = "Execution was skipped: " + prior_error;
            ctx.lastError = prior_error;
        } else {
            ctx.lastObservation = "Execution did not produce any result.";
            ctx.lastError = "ObserveState: no execution attempt recorded.";
        }
        return;
    }

    // ---- Kasus 3: timeout ----
    if (ctx.lastExecution.timed_out) {
        std::string obs = "TIMEOUT: execution did not finish within the "
                        + std::to_string(ctx.lastExecution.duration_sec)
                        + "s limit and was killed (SIGKILL).";
        const std::string tail = tailOf(ctx.lastExecution.stdout_text, 200);
        if (!tail.empty()) {
            obs += "\nstdout before kill (tail): " + tail;
        }
        ctx.lastObservation = obs;
        ctx.lastError = "Execution TIMEOUT.";
        return;
    }

    // ---- Kasus 3.5: cancelled by user (Stage 7g) ----
    // CATATAN: kasus ini dicek SETELAH timeout karena mutually exclusive,
    // tapi DIPISAH dari fallback umum supaya observation-nya jelas dan
    // FixState tidak sia-sia mencoba memperbaiki.
    if (ctx.lastExecution.cancelled_by_user) {
        ctx.lastObservation =
            "Execution cancelled by user (SIGKILL sent before completion).";
        ctx.lastError = "Execution cancelled by user.";
        return;
    }

    // ---- Kasus 4: SyntaxError (muncul sebelum traceback) ----
    const std::string& err = ctx.lastExecution.stderr_text;

    const SyntaxErrorInfo syn = parseSyntaxError(err);
    if (syn.found) {
        std::string obs = "Execution failed with SyntaxError (exit="
                        + std::to_string(ctx.lastExecution.exit_code) + ").\n";
        if (syn.line > 0) {
            obs += "Line: " + std::to_string(syn.line) + "\n";
        }
        obs += "Message: " + syn.message + "\n";
        ctx.lastObservation = obs;
        ctx.lastError = "SyntaxError: " + syn.message;
        return;
    }

    // ---- Kasus 5: Python runtime traceback ----
    const PythonError py = parsePythonTraceback(err);
    if (py.found) {
        std::string obs = "Execution failed with exit code "
                        + std::to_string(ctx.lastExecution.exit_code) + ".\n";
        obs += "Exception: " + py.exception_type + "\n";
        obs += "Message: " + py.exception_message + "\n";
        if (py.line > 0) {
            obs += "Line: " + std::to_string(py.line);
            if (!py.function.empty()) obs += " (in " + py.function + ")";
            obs += "\n";
        }
        obs += "stderr (tail, last 500 chars):\n";
        obs += tailOf(err, 500);
        ctx.lastObservation = obs;
        ctx.lastError = py.exception_type + ": " + py.exception_message;
        return;
    }

    // ---- Kasus 6: exit != 0 tapi format tidak dikenali ----
    std::string obs = "Execution failed with exit code "
                    + std::to_string(ctx.lastExecution.exit_code)
                    + " (unrecognized error format).\n";
    const std::string& source = !err.empty()
                              ? err
                              : ctx.lastExecution.stdout_text;
    obs += "output (tail, last 500 chars):\n";
    obs += tailOf(source, 500);
    ctx.lastObservation = obs;
    ctx.lastError = "Execution failed (exit="
                  + std::to_string(ctx.lastExecution.exit_code) + ").";
}

} // namespace frans::application
