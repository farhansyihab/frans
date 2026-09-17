#pragma once

#include <string>

namespace frans::config {

// Return direktori executable yang sedang berjalan.
// Return string kosong kalau tidak bisa ditentukan (fallback ke argv0
// ditangani oleh findConfigPath).
std::string executableDir();

// Cari config/app.json di beberapa lokasi standar supaya bisa
// di-double-click dari file manager (CWD tidak reliable):
//
//   1. <CWD>/config/app.json                 (perilaku terminal lama)
//   2. <exe_dir>/config/app.json             (install bareng binary)
//   3. <exe_dir>/../share/frans/app.json     (install layout Unix)
//   4. Walk-up dari <exe_dir> naik max 6 level:
//        <exe_dir>/../config/app.json
//        <exe_dir>/../../config/app.json
//        ...
//
// Return string kosong kalau tidak ada satu pun yang ketemu.
std::string findConfigPath(const std::string& argv0);

} // namespace frans::config