// ~/AgenAI/frans/src/domain/command/ICommand.h
#pragma once

#include "domain/command/CommandResult.h"

#include <string>

namespace frans::domain {

// Blueprint §4 — Command pattern.
// Semua aksi (clean memory, start/stop model, tulis file, panggil API)
// direpresentasikan sebagai objek ICommand agar bisa di-log, di-decorate
// (Retry/Logging/CostTracking), dan dikomposisi (MacroCommand).
class ICommand {
public:
    virtual ~ICommand() = default;

    // Jalankan aksi. Implementasi TIDAK boleh throw; return CommandResult.
    virtual CommandResult execute() = 0;

    // Nama pendek untuk logging & UI.
    virtual std::string name() const = 0;

    // Blueprint §5.8 — Safety Layer. Command yang mengembalikan true wajib
    // melewati DangerCheckHandler (minta konfirmasi user) sebelum eksekusi.
    virtual bool isDangerous() const { return false; }
};

} // namespace frans::domain
