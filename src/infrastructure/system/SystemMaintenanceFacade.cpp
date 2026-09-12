// ~/AgenAI/frans/src/infrastructure/system/SystemMaintenanceFacade.cpp
#include "infrastructure/system/SystemMaintenanceFacade.h"

#include "domain/command/ICommand.h"
#include "domain/command/MacroCommand.h"
#include "infrastructure/system/ProcessExecutor.h"

#include <cstdio>
#include <memory>
#include <string>

namespace frans::infrastructure {

namespace {

using frans::domain::CommandResult;
using frans::domain::ICommand;

// ---------------------------------------------------------------------------
// Command wrappers (blueprint §4 — Command pattern).
// ---------------------------------------------------------------------------

class SyncCommand final : public ICommand {
public:
    CommandResult execute() override {
        const auto r = runCommand({"sync"}, 15);
        if (r.exit_code == 0) return CommandResult::ok("flushed");
        if (r.timed_out)     return CommandResult::fail(-1, "sync timeout");
        return CommandResult::fail(r.exit_code, "sync gagal: " + r.stderr_text);
    }
    std::string name() const override { return "sync"; }
};

class DropCachesCommand final : public ICommand {
public:
    CommandResult execute() override {
        // `sudo -n tee /proc/sys/vm/drop_caches` dengan stdin "3\n".
        // sudoers.d mengizinkan tepat perintah ini (blueprint §5.1).
        const auto r = runCommand(
            {"sudo", "-n", "tee", "/proc/sys/vm/drop_caches"},
            /*timeout_sec=*/10,
            /*stdin_data=*/"3\n");

        if (r.exit_code == 0) return CommandResult::ok("drop_caches=3");
        if (r.timed_out)     return CommandResult::fail(-1, "drop_caches TIMEOUT (kemungkinan sudo blocking di stdin)");

        std::string err = r.stderr_text;
        if (err.empty()) err = "(stderr kosong — cek sudoers.d NOPASSWD)";
        return CommandResult::fail(r.exit_code, "drop_caches gagal: " + err);
    }
    std::string name() const override { return "drop_caches"; }
    bool isDangerous() const override { return true; }
};

class SwapOffCommand final : public ICommand {
public:
    CommandResult execute() override {
        const auto r = runCommand({"sudo", "-n", "swapoff", "-a"}, /*timeout_sec=*/120);
        if (r.exit_code == 0) return CommandResult::ok("swapoff -a");
        if (r.timed_out)     return CommandResult::fail(-1, "swapoff TIMEOUT (swap sedang sibuk / banyak halaman)");

        std::string err = r.stderr_text;
        if (err.empty()) err = "(stderr kosong — cek sudoers.d NOPASSWD)";
        return CommandResult::fail(r.exit_code, "swapoff gagal: " + err);
    }
    std::string name() const override { return "swapoff"; }
    bool isDangerous() const override { return true; }
};

class SwapOnCommand final : public ICommand {
public:
    CommandResult execute() override {
        const auto r = runCommand({"sudo", "-n", "swapon", "-a"}, /*timeout_sec=*/30);
        if (r.exit_code == 0) return CommandResult::ok("swapon -a");
        if (r.timed_out)     return CommandResult::fail(-1, "swapon TIMEOUT");

        std::string err = r.stderr_text;
        if (err.empty()) err = "(stderr kosong — cek sudoers.d NOPASSWD)";
        return CommandResult::fail(r.exit_code, "swapon gagal: " + err);
    }
    std::string name() const override { return "swapon"; }
};

} // namespace

// ---------------------------------------------------------------------------
// Facade API
// ---------------------------------------------------------------------------

MemoryInfo SystemMaintenanceFacade::getMemoryStatus() const {
    MemoryInfo info;
    (void)readMemoryInfo(info);
    return info;
}

bool SystemMaintenanceFacade::isSudoAvailable() const {
    const auto r = runCommand({"sudo", "-n", "true"}, 5);
    return r.exit_code == 0;
}

frans::domain::CommandResult SystemMaintenanceFacade::syncDisks() {
    SyncCommand c;
    return c.execute();
}

frans::domain::CommandResult SystemMaintenanceFacade::dropCaches() {
    DropCachesCommand c;
    return c.execute();
}

frans::domain::CommandResult SystemMaintenanceFacade::swapOff() {
    SwapOffCommand c;
    return c.execute();
}

frans::domain::CommandResult SystemMaintenanceFacade::swapOn() {
    SwapOnCommand c;
    return c.execute();
}

frans::domain::CommandResult SystemMaintenanceFacade::swapCycle() {
    // stop_on_failure = false: kalau swapoff gagal, swapon tetap dicoba.
    auto macro = std::make_unique<frans::domain::MacroCommand>("swap_cycle", false);
    macro->add(std::make_unique<SwapOffCommand>());
    macro->add(std::make_unique<SwapOnCommand>());
    return macro->execute();
}

frans::domain::CommandResult SystemMaintenanceFacade::cleanMemory() {
    // stop_on_failure = false: satu step gagal (mis. drop_caches ditolak sudo)
    // tidak boleh memblokir step lain.
    auto macro = std::make_unique<frans::domain::MacroCommand>("clean_memory", false);
    macro->add(std::make_unique<SyncCommand>());
    macro->add(std::make_unique<DropCachesCommand>());
    macro->add(std::make_unique<SwapOffCommand>());
    macro->add(std::make_unique<SwapOnCommand>());
    return macro->execute();
}

} // namespace frans::infrastructure
