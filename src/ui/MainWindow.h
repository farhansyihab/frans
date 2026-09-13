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
class AgentPanel;

// Main window FLTK.
//
// Stage 7e: tambah AgentPanel (dijadwalkan lewat timer 100ms) di antara
// LlamaPanel dan ChatView.
class MainWindow {
public:
    explicit MainWindow(frans::application::AppContext* ctx);
    ~MainWindow();

    MainWindow(const MainWindow&)            = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    Fl_Window* create();
    void       startTimers();

private:
    // Timer callbacks.
    static void onMemoryRefresh(void*);
    static void onLlamaRefresh(void*);
    static void onWorkerPoll(void*);
    static void onWatchdog(void*);
    static void onChatTick(void*);
    static void onAgentTick(void*);

    // Worker listener.
    void onWorkerBusy(bool busy, const std::string& label);
    void onWorkerDone(const frans::application::WorkerDone& done);

    frans::application::AppContext* ctx_;

    std::unique_ptr<frans::application::Worker> worker_;
    std::unique_ptr<MaintenancePanel>            maintenance_;
    std::unique_ptr<LlamaPanel>                  llama_panel_;
    std::unique_ptr<AgentPanel>                  agent_panel_;
    std::unique_ptr<ChatView>                    chat_;

    Fl_Window* win_         = nullptr;
    Fl_Box*    status_line_ = nullptr;

    std::string status_buf_;
};

} // namespace frans::ui
