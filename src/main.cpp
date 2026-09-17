#include "config/AppConfig.h"
#include "ui/MainWindow.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include <cstdio>

int main(int argc, char** argv) {
    const frans::config::AppConfig cfg =
        frans::config::AppConfig::loadFromFile("config/app.json");

    std::printf("[frans] startup OK — FLTK %d.%d.%d\n",
                FL_MAJOR_VERSION, FL_MINOR_VERSION, FL_PATCH_VERSION);
    std::fflush(stdout);

    frans::ui::MainWindow window(cfg);
    Fl_Window* win = window.create();
    win->show(argc, argv);

    return Fl::run();
}