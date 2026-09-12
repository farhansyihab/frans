// ~/AgenAI/frans/src/domain/command/MacroCommand.cpp
#include "domain/command/MacroCommand.h"

#include <sstream>

namespace frans::domain {

MacroCommand::MacroCommand(std::string name, bool stop_on_failure)
    : name_(std::move(name)), stop_on_failure_(stop_on_failure) {}

void MacroCommand::add(std::unique_ptr<ICommand> cmd) {
    if (cmd) children_.push_back(std::move(cmd));
}

bool MacroCommand::isDangerous() const {
    for (const auto& c : children_) {
        if (c && c->isDangerous()) return true;
    }
    return false;
}

CommandResult MacroCommand::execute() {
    std::ostringstream log;
    int first_fail_code = 0;

    for (const auto& c : children_) {
        if (!c) continue;

        log << "[ " << c->name() << " ] ";
        const CommandResult r = c->execute();
        log << (r.success ? "OK" : "FAIL");
        if (!r.message.empty()) {
            log << " - " << r.message;
        }
        log << '\n';

        if (!r.success) {
            if (first_fail_code == 0) first_fail_code = r.exit_code;
            if (stop_on_failure_) {
                return CommandResult::fail(first_fail_code, log.str());
            }
        }
    }

    if (first_fail_code != 0) {
        return CommandResult::fail(first_fail_code, log.str());
    }
    return CommandResult::ok(log.str());
}

} // namespace frans::domain
