// ~/AgenAI/frans/src/domain/command/MacroCommand.h
#pragma once

#include "domain/command/ICommand.h"

#include <memory>
#include <string>
#include <vector>

namespace frans::domain {

// Blueprint §4 — Composite.
// Menggabungkan beberapa ICommand menjadi satu aksi tunggal.
//
// stop_on_failure = true  -> eksekusi berhenti di command pertama yang gagal
//                            (dipakai untuk rangkaian yang saling bergantung).
// stop_on_failure = false -> semua command dieksekusi, hasil diagregasi
//                            (dipakai untuk "bersihkan memori total" agar
//                            satu step gagal tidak memblokir step lain).
class MacroCommand : public ICommand {
public:
    explicit MacroCommand(std::string name, bool stop_on_failure = true);

    void add(std::unique_ptr<ICommand> cmd);

    CommandResult execute() override;
    std::string name() const override { return name_; }
    bool isDangerous() const override;

private:
    std::string name_;
    bool stop_on_failure_;
    std::vector<std::unique_ptr<ICommand>> children_;
};

} // namespace frans::domain
