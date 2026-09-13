// ~/AgenAI/frans/src/ui/MainWindow.cpp
#include "ui/MainWindow.h"

#include "application/AppContext.h"
#include "ui/ChatView.h"
#include "ui/LlamaPanel.h"
#include "ui/MaintenancePanel.h"

#include <FL/Fl_Box.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <cstdio>

namespace frans::ui {

using frans::application::AppContext;
using frans::application::Worker;
using frans::application::WorkerDone;
using frans::infrastructure::ModelEntry;

namespace {
constexpr int kWorkerTimeoutSec = 180;
}

MainWindow::MainWindow(AppContext* ctx) : ctx_(ctx) {
    worker_ = std::make_unique<Worker>(
        [this](bool busy, const std::string& label) {
            onWorkerBusy(busy, label);
        },
        [this](const WorkerDone& done) {
            onWorkerDone(done);
        });
}

MainWindow::~MainWindow() = default;

Fl_Window* MainWindow::create() {
    const auto& cfg = ctx_->config;
    const int W = cfg.window_width;

    win_ = new Fl_Window(cfg.window_width, cfg.window_height,
                         cfg.window_title.c_str());
    win_->begin();

    int y = 12;

    // ---- Header ----
    Fl_Box* title = new Fl_Box(20, y, W - 40, 30,
        "frans_ai  \u2014  Stage 4: LlamaCppProvider + Chat Streaming");
    title->labelfont(FL_BOLD);
    title->labelsize(17);
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    y += 30;

    Fl_Box* sub = new Fl_Box(20, y, W - 40, 18,
        "SSE streaming ke /v1/chat/completions  |  "
        "markActivity() reset idle timer per token");
    sub->labelsize(11);
    sub->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    y += 26;

    // ---- Maintenance section (RAM/Swap + tombol maintenance) ----
    maintenance_ = std::make_unique<MaintenancePanel>(
        ctx_, worker_.get(),
        // ModelProvider — di-evaluasi saat user klik tombol "Check Memory".
        [this]() -> const ModelEntry* {
            return llama_panel_ ? llama_panel_->selectedModel() : nullptr;
        });
    y = maintenance_->createWidgets(20, y, W - 40);
    y += 12;

    // ---- Status line (shared) ----
    status_line_ = new Fl_Box(20, y, W - 40, 20, "idle.");
    status_line_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    status_line_->labelsize(11);
    status_line_->labelcolor(FL_DARK_BLUE);
    y += 26;

    // ---- Llama section ----
    llama_panel_ = std::make_unique<LlamaPanel>(
        ctx_, worker_.get(),
        // ChatStreamingFn — di-evaluasi saat refreshStatus() dari timer.
        [this]() -> bool {
            return chat_ && chat_->isStreaming();
        });
    y = llama_panel_->createWidgets(20, y, W - 40);
    y += 12;

    // ---- Chat section ----
    Fl_Box* ctitle = new Fl_Box(20, y, W - 40, 20, "Chat  (streaming test)");
    ctitle->labelfont(FL_BOLD);
    ctitle->labelsize(12);
    ctitle->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    y += 22;

    const int chat_h = 240;

    ChatDeps chat_deps;
    chat_deps.is_server_running = [this]() -> bool {
        return ctx_->llama && ctx_->llama->isRunning();
    };
    chat_deps.get_host = [this]() -> std::string {
        if (!ctx_->llama) return "127.0.0.1";
        const std::string url = ctx_->llama->baseUrl();
        const auto scheme_end = url.find("://");
        const auto colon      = url.rfind(':');
        if (scheme_end == std::string::npos || colon == std::string::npos) {
            return "127.0.0.1";
        }
        return url.substr(scheme_end + 3, colon - (scheme_end + 3));
    };
    chat_deps.get_port = [this]() -> int {
        if (!ctx_->llama) return 8080;
        const std::string url = ctx_->llama->baseUrl();
        const auto colon = url.rfind(':');
        if (colon == std::string::npos) return 8080;
        try { return std::stoi(url.substr(colon + 1)); }
        catch (...) { return 8080; }
    };
    chat_deps.get_model_tag = [this]() -> std::string {
        const ModelEntry* e = llama_panel_ ? llama_panel_->selectedModel() : nullptr;
        return e ? e->id : std::string{};
    };
    chat_deps.on_activity = [this]() {
        if (ctx_->idle_timer) ctx_->idle_timer->markActivity();
    };

    chat_ = std::make_unique<ChatView>(20, y, W - 40, chat_h,
                                       std::move(chat_deps));
    chat_->createWidgets();
    y += chat_h + 10;

    // ---- Hints ----
    Fl_Box* hint = new Fl_Box(20, y, W - 40, cfg.window_height - y - 15,
        "Catatan Stage 4:\n"
        "  \u2022 LlamaCppProvider memakai endpoint OpenAI-compatible "
        "/v1/chat/completions\n"
        "    dengan stream=true \u2192 respons SSE (data: {...}\\n\\n).\n"
        "  \u2022 Setiap token yang diterima memanggil idle_timer->markActivity() "
        "supaya\n"
        "    server tidak auto-unload di tengah streaming panjang.\n"
        "  \u2022 Format status llama: \"auto-unload in MM:SS | llama-server: "
        "RUNNING @ URL\"\n"
        "    Countdown ditempatkan di DEPAN supaya selalu terlihat.\n"
        "  \u2022 Cancel memutus SSE di tengah jalan (content_receiver return "
        "false).\n"
        "  \u2022 Worker thread hanya menulis ke buffer di bawah mutex; widget "
        "FLTK\n"
        "    di-update dari timer main thread (ChatView::tick setiap 150 ms).");
    hint->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    hint->labelsize(11);

    win_->end();
    return win_;
}

void MainWindow::startTimers() {
    Fl::add_timeout(0.4,  onMemoryRefresh, this);
    Fl::add_timeout(0.5,  onLlamaRefresh,  this);
    Fl::add_timeout(1.0,  onWatchdog,      this);
    Fl::add_timeout(0.2,  onWorkerPoll,    this);
    Fl::add_timeout(0.15, onChatTick,      this);
}

// ---------------------------------------------------------------------------
// Timer callbacks
// ---------------------------------------------------------------------------
void MainWindow::onMemoryRefresh(void* data) {
    auto* self = static_cast<MainWindow*>(data);
    if (self->maintenance_) self->maintenance_->refreshMemoryDisplay();
    Fl::repeat_timeout(2.0, onMemoryRefresh, data);
}

void MainWindow::onLlamaRefresh(void* data) {
    auto* self = static_cast<MainWindow*>(data);
    if (self->llama_panel_) self->llama_panel_->refreshStatus();
    Fl::repeat_timeout(1.0, onLlamaRefresh, data);
}

void MainWindow::onWorkerPoll(void* data) {
    auto* self = static_cast<MainWindow*>(data);
    self->worker_->poll();
    Fl::repeat_timeout(0.2, onWorkerPoll, data);
}

void MainWindow::onWatchdog(void* data) {
    auto* self = static_cast<MainWindow*>(data);
    self->worker_->watchdog(kWorkerTimeoutSec);
    Fl::repeat_timeout(1.0, onWatchdog, data);
}

void MainWindow::onChatTick(void* data) {
    auto* self = static_cast<MainWindow*>(data);
    if (self->chat_) self->chat_->tick();
    Fl::repeat_timeout(0.15, onChatTick, data);
}

// ---------------------------------------------------------------------------
// Worker listener
// ---------------------------------------------------------------------------
void MainWindow::onWorkerBusy(bool busy, const std::string& label) {
    if (maintenance_) maintenance_->setEnabled(!busy);
    if (llama_panel_) llama_panel_->setEnabled(!busy);

    status_buf_ = busy ? ("RUNNING: " + label + " ...")
                       : std::string("idle.");
    if (status_line_) status_line_->copy_label(status_buf_.c_str());
}

void MainWindow::onWorkerDone(const WorkerDone& done) {
    fl_message("%s: %s\n\n%s",
               done.label.c_str(),
               done.success ? "SUCCESS" : "SELESAI (ada step gagal)",
               done.message.c_str());
}

} // namespace frans::ui
