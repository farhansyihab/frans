#include "src/infrastructure/system/ProcessExecutor.h"
#include <iostream>

int main() {
    std::vector<std::string> argv = {"sh", "-c", "echo GIT_TERMINAL_PROMPT=$GIT_TERMINAL_PROMPT"};
    std::vector<std::string> env_extra = {"GIT_TERMINAL_PROMPT=0"};
    
    auto result = frans::infrastructure::runCommand(
        argv,
        30,           // timeout_sec
        "",           // stdin_data
        "",           // working_dir
        nullptr,      // cancel_flag
        env_extra     // env_extra
    );
    
    std::cout << "STDOUT: " << result.stdout_text << std::endl;
    std::cout << "STDERR: " << result.stderr_text << std::endl;
    std::cout << "EXIT: " << result.exit_code << std::endl;
    
    return 0;
}
