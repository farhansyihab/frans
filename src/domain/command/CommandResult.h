// ~/AgenAI/frans/src/domain/command/CommandResult.h
#pragma once

#include <string>
#include <utility>

namespace frans::domain {

// Hasil eksekusi satu ICommand. Dipakai untuk logging, retry, dan
// reporting ke UI. Selalu berisi pesan yang bisa ditampilkan ke user.
struct CommandResult {
    bool success = false;
    int exit_code = 0;
    std::string message;

    static CommandResult ok(std::string msg = {}) {
        CommandResult r;
        r.success = true;
        r.exit_code = 0;
        r.message = std::move(msg);
        return r;
    }

    static CommandResult fail(int code, std::string msg) {
        CommandResult r;
        r.success = false;
        r.exit_code = code;
        r.message = std::move(msg);
        return r;
    }
};

} // namespace frans::domain
