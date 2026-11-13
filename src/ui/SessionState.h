#pragma once

#include <string>

namespace frans::ui {

// State bersama antar-dialog. Dimiliki MainWindow, pointer
// diteruskan ke dialog yang butuh.
//
// TIDAK menyimpan API key (lihat BluePrint v9 Addendum, aturan keamanan).
struct SessionState {
    std::string last_project_root;
    std::string last_build_error;      // ringkasan error dari BuildReport terakhir
    std::string last_context_path;     // SourceCode.md terakhir
};

} // namespace frans::ui