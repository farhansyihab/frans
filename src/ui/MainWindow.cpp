// ~/AgenAI/frans/src/ui/MainWindow.cpp
#include "ui/MainWindow.h"

#include "application/AppContext.h"
#include "ui/AgentPanel.h"
#include "ui/ChatView.h"
#include "ui/LlamaPanel.h"
#include "ui/MaintenancePanel.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <algorithm>
#include <cstdio>

namespace frans::ui {

using frans::application::AppContext;
using frans::application::Worker;
using frans::application::WorkerDone;
using frans::infrastructure::ModelEntry;

namespace {

constexpr int kWorkerTimeoutSec = 180;

// Lebar padding dalam tab (kiri+kanan). Nilai kecil supaya panel dapat
// lebar maksimal.
constexpr int kTabPaddingX = 10;

// Tinggi tab header bar (Fl_Tabs reserve ~25px untuk label tab).
constexpr int kTabHeaderH = 25;

// Padding vertikal dalam tab.
constexpr int kTabPaddingY = 8;

// Tinggi minimum window. Di bawah ini layout tab mulai tidak nyaman.
constexpr int kMinWindowW = 800;
constexpr int kMinWindowH = 560;

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// create — bangun window + layout tab
// ---------------------------------------------------------------------------
Fl_Window* MainWindow::create() {
    const auto& cfg = ctx_->config;

    // ---- Hitung ukuran & posisi window ----
    // Ambil work area (excludes taskbar/panel). Fallback ke Fl::w()/Fl::h()
    // kalau X server tidak melaporkan work area dengan benar.
    int sx = 0, sy = 0, sw = 0, sh = 0;
    Fl::screen_work_area(sx, sy, sw, sh);
    if (sw < kMinWindowW || sh < kMinWindowH) {
        sx = 0;
        sy = 0;
        sw = Fl::w() > 0 ? Fl::w() : 1366;
        sh = Fl::h() > 0 ? Fl::h() : 768;
    }

    int margin = cfg.window_margin_px;
    if (margin < 0) margin = 24;
    // Jangan sampai margin melebihi setengah layar (config salah).
    margin = std::min(margin, std::min(sw, sh) / 4);

    const int avail_w = std::max(sw - 2 * margin, kMinWindowW);
    const int avail_h = std::max(sh - 2 * margin, kMinWindowH);

    // width/height = 0 → AUTO (pakai avail). > 0 → pakai config, tapi clamp.
    int ww = cfg.window_width  > 0 ? std::min(cfg.window_width,  avail_w) : avail_w;
    int wh = cfg.window_height > 0 ? std::min(cfg.window_height, avail_h) : avail_h;

    const int wx = sx + margin;
    const int wy = sy + margin;

    std::fprintf(stderr,
        "[frans][ui] screen work area: (%d,%d) %dx%d; margin=%d; "
        "window: (%d,%d) %dx%d\n",
        sx, sy, sw, sh, margin, wx, wy, ww, wh);
    std::fflush(stderr);

    // ---- Buat window ----
    win_ = new Fl_Window(wx, wy, ww, wh, cfg.window_title.c_str());

    // resizable(win_) → user bisa drag border window; WM tahu window
    // resizable sehingga tombol maximize/restore aktif.
    // size_range → batasi min & max. max = ukuran layar supaya tidak
    // bisa drag melebihi layar.
    win_->resizable(win_);
    win_->size_range(kMinWindowW, kMinWindowH,
                     Fl::w() > 0 ? Fl::w() : 16384,
                     Fl::h() > 0 ? Fl::h() : 16384);

    win_->begin();

    // =====================================================================
    // Layout: header → LlamaPanel (sticky) → Tabs → status line
    // =====================================================================
    const int W  = ww - 40;   // content width (margin 20 kiri/kanan)
    const int LX = 20;        // left x

    int y = 12;

    // ---- Header (compact) ----
    {
        Fl_Box* title = new Fl_Box(LX, y, W, 24,
            "frans_ai  \u2014  Stage 7f: Project Awareness");
        title->labelfont(FL_BOLD);
        title->labelsize(15);
        title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        y += 28;
    }

    // ---- LlamaPanel (STICKY — di luar tabs) ----
    llama_panel_ = std::make_unique<LlamaPanel>(
        ctx_, worker_.get(),
        [this]() -> bool {
            return chat_ && chat_->isStreaming();
        });
    y = llama_panel_->createWidgets(LX, y, W);
    y += 6;

    // ---- Tab area ----
    // Status line di bawah tabs → reserve dari bawah.
    constexpr int kStatusH       = 24;
    constexpr int kBottomMargin  = 10;
    constexpr int kStatusGap     = 6;

    const int tabs_h = wh - y - kStatusH - kBottomMargin - kStatusGap;
    if (tabs_h < 200) {
        std::fprintf(stderr,
            "[frans][ui] WARNING: tabs_h=%d terlalu kecil. "
            "Beberapa panel mungkin terpotong.\n", tabs_h);
        std::fflush(stderr);
    }

    // Inner bounds untuk tab content (setelah dikurangi tab header + padding).
    const int tab_inner_x = LX + kTabPaddingX;
    const int tab_inner_y = y + kTabHeaderH + kTabPaddingY;
    const int tab_inner_w = W - 2 * kTabPaddingX;
    const int tab_inner_h = tabs_h - kTabHeaderH - 2 * kTabPaddingY;

    tabs_ = new Fl_Tabs(LX, y, W, tabs_h);

    // ---- Tab 1: Agent (default visible) ----
    {
        Fl_Group* g = new Fl_Group(LX + 2, y + kTabHeaderH,
                                    W - 4, tabs_h - kTabHeaderH,
                                    "Agent");
        g->begin();

        AgentPanel::Deps adeps;
        adeps.is_server_running = [this]() -> bool {
            return ctx_->llama && ctx_->llama->isRunning();
        };
        adeps.get_host = [this]() -> std::string {
            if (!ctx_->llama) return "127.0.0.1";
            const std::string url = ctx_->llama->baseUrl();
            const auto scheme_end = url.find("://");
            const auto colon      = url.rfind(':');
            if (scheme_end == std::string::npos || colon == std::string::npos) {
                return "127.0.0.1";
            }
            return url.substr(scheme_end + 3, colon - (scheme_end + 3));
        };
        adeps.get_port = [this]() -> int {
            if (!ctx_->llama) return 8080;
            const std::string url = ctx_->llama->baseUrl();
            const auto colon = url.rfind(':');
            if (colon == std::string::npos) return 8080;
            try { return std::stoi(url.substr(colon + 1)); }
            catch (...) { return 8080; }
        };
        adeps.on_activity = [this]() {
            if (ctx_->idle_timer) ctx_->idle_timer->markActivity();
        };

        agent_panel_ = std::make_unique<AgentPanel>(ctx_, std::move(adeps));
        // Panel dimulai dari tab_inner_y, bukan dari y + kTabHeaderH —
        // kita sudah hitung padding di atas.
        agent_panel_->createWidgets(tab_inner_x, tab_inner_y, tab_inner_w);

        g->end();
    }

    // ---- Tab 2: Chat ----
    {
        Fl_Group* g = new Fl_Group(LX + 2, y + kTabHeaderH,
                                    W - 4, tabs_h - kTabHeaderH,
                                    "Chat");
        g->begin();

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

        // ChatView punya (x, y, w, h) di constructor (bukan createWidgets
        // param), jadi kita pass tab_inner bounds langsung.
        chat_ = std::make_unique<ChatView>(tab_inner_x, tab_inner_y,
                                            tab_inner_w, tab_inner_h,
                                            std::move(chat_deps));
        chat_->createWidgets();

        g->end();
    }

    // ---- Tab 3: System (MaintenancePanel + hint box) ----
    {
        Fl_Group* g = new Fl_Group(LX + 2, y + kTabHeaderH,
                                    W - 4, tabs_h - kTabHeaderH,
                                    "System");
        g->begin();

        maintenance_ = std::make_unique<MaintenancePanel>(
            ctx_, worker_.get(),
            [this]() -> const ModelEntry* {
                return llama_panel_ ? llama_panel_->selectedModel() : nullptr;
            });

        const int maint_end_y =
            maintenance_->createWidgets(tab_inner_x, tab_inner_y, tab_inner_w);

        // Hint box di bawah maintenance.
        const int hint_y = maint_end_y + 16;
        const int hint_h = (tab_inner_y + tab_inner_h) - hint_y;
        if (hint_h > 60) {
            Fl_Box* hint = new Fl_Box(tab_inner_x, hint_y,
                                       tab_inner_w, hint_h,
                "Cara pakai:\n"
                "  \u2022  Llama Server (panel atas, sticky): pilih model, klik Start Server.\n"
                "  \u2022  Tab Agent: isi project root + task, kosongkan Target untuk auto-detect,\n"
                "     lalu klik Run Task. Hasil (attempt history + file + stdout/stderr)\n"
                "     muncul di kotak output di bawah tombol.\n"
                "  \u2022  Tab Chat: chat bebas dengan model lokal (streaming).\n"
                "  \u2022  Tab System: bersihkan RAM/Swap, drop caches, dll.\n"
                "\n"
                "Log terminal ber-prefix [frans] adalah alat debug utama \u2014 "
                "jalankan frans_ai dari terminal, bukan double-click.");
            hint->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
            hint->labelsize(11);
            hint->labelcolor(FL_DARK_BLUE);
        }

        g->end();
    }

    tabs_->end();

    y += tabs_h + kStatusGap;

    // ---- Status line (paling bawah, mirip status bar aplikasi) ----
    status_line_ = new Fl_Box(LX, y, W, kStatusH, "idle.");
    status_line_->box(FL_DOWN_BOX);
    status_line_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    status_line_->labelsize(11);
    status_line_->labelcolor(FL_DARK_BLUE);

    win_->end();
    return win_;
}

// ---------------------------------------------------------------------------
// startTimers
// ---------------------------------------------------------------------------
void MainWindow::startTimers() {
    Fl::add_timeout(0.4,  onMemoryRefresh, this);
    Fl::add_timeout(0.5,  onLlamaRefresh,  this);
    Fl::add_timeout(1.0,  onWatchdog,      this);
    Fl::add_timeout(0.2,  onWorkerPoll,    this);
    Fl::add_timeout(0.15, onChatTick,      this);
    Fl::add_timeout(0.10, onAgentTick,     this);
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

void MainWindow::onAgentTick(void* data) {
    auto* self = static_cast<MainWindow*>(data);
    if (self->agent_panel_) self->agent_panel_->tick();
    Fl::repeat_timeout(0.10, onAgentTick, data);
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
