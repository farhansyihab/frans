// ~/AgenAI/frans/src/infrastructure/system/SystemMaintenanceFacade.h
#pragma once

#include "domain/command/CommandResult.h"
#include "infrastructure/system/MemoryInfo.h"

namespace frans::infrastructure {

// Blueprint §5.1 — System Maintenance Module.
//
// Facade untuk operasi sistem yang di-scope sempit:
//   - cleanMemory(): sync + drop_caches + swap cycle (via sudo NOPASSWD)
//   - getMemoryStatus(): baca /proc/meminfo
//
// Semua panggilan eksternal lewat `sudo -n` (NON-INTERACTIVE). Kalau sudoers.d
// belum diset, command akan gagal cepat dengan exit_code != 0 dan pesan jelas,
// BUKAN menggantung menunggu password.
class SystemMaintenanceFacade {
public:
    SystemMaintenanceFacade() = default;

    // Snapshot /proc/meminfo saat ini.
    MemoryInfo getMemoryStatus() const;

    // Rangkaian lengkap: sync -> drop_caches -> swapoff -> swapon.
    // stop_on_failure = false: satu step gagal tidak membatalkan step lain.
    frans::domain::CommandResult cleanMemory();

    // Ekspos tiap step untuk granularitas UI / testing.
    frans::domain::CommandResult syncDisks();
    frans::domain::CommandResult dropCaches();
    frans::domain::CommandResult swapOff();
    frans::domain::CommandResult swapOn();
    frans::domain::CommandResult swapCycle();   // swapoff lalu swapon

    // Cek apakah `sudo -n true` berhasil (NOPASSWD sudah dikonfigurasi).
    bool isSudoAvailable() const;
};

} // namespace frans::infrastructure
