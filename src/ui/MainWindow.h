// ~/AgenAI/frans/src/ui/MainWindow.h
#pragma once

#include "application/Worker.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include <memory>
#include <string>

class Fl_Box;

namespace frans::application {
struct AppContext;
}

namespace frans::ui {

class ChatView;
class MaintenancePanel;
class LlamaPanel;

// Main window FLTK.
//
// Tanggung jawab HANYA:
//   - menyusun layout (header + panel + chat + hint)
//   - mendaftarkan timer ke FLTK
//   - meneruskan busy state ke panel-panel
//
// Detail bisnis ada di masing-masing panel; state non-UI di AppContext.
class MainWindow {
public:
    explicit MainWindow(frans::application::AppContext* ctx);
    ~MainWindow();

    MainWindow(const MainWindow&)            = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    // Bangun window + semua panel. Return window (owned by FLTK).
    Fl_Window* create();

    // Daftarkan timer. Panggil setelah create() dan show().
    void startTimers();

private:
    // Timer callbacks (semuanya dipanggil dari main thread oleh FLTK).
    static void onMemoryRefresh(void*);
    static void onLlamaRefresh(void*);
    static void onWorkerPoll(void*);
    static void onWatchdog(void*);
    static void onChatTick(void*);

    // Worker listener.
    void onWorkerBusy(bool busy, const std::string& label);
    void onWorkerDone(const frans::application::WorkerDone& done);

    frans::application::AppContext* ctx_;

    std::unique_ptr<frans::application::Worker> worker_;
    std::unique_ptr<MaintenancePanel>            maintenance_;
    std::unique_ptr<LlamaPanel>                  llama_panel_;
    std::unique_ptr<ChatView>                    chat_;

    Fl_Window* win_         = nullptr;
    Fl_Box*    status_line_ = nullptr;

    std::string status_buf_;
};

} // namespace frans::ui
