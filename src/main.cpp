#include "config/AppConfig.h"
#include "config/AppPaths.h"
#include "ui/MainWindow.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    const std::string argv0 = (argc > 0 && argv[0]) ? argv[0] : "";

    // Resolve config relatif ke binary supaya bisa di-double-click
    // dari file manager (CWD tidak reliable).
    const std::string cfg_path = frans::config::findConfigPath(argv0);

    frans::config::AppConfig cfg;
    if (!cfg_path.empty()) {
        cfg = frans::config::AppConfig::loadFromFile(cfg_path);
    }

    std::printf("[frans] startup OK — FLTK %d.%d.%d\n",
                FL_MAJOR_VERSION, FL_MINOR_VERSION, FL_PATCH_VERSION);
    std::fflush(stdout);

    frans::ui::MainWindow window(cfg);
    Fl_Window* win = window.create();
    win->show(argc, argv);

    return Fl::run();
}