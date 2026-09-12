// ~/AgenAI/frans/src/main.cpp
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include "application/resource_guard/IdleUnloadTimer.h"
#include "application/resource_guard/ResourceGuard.h"
#include "config/AppConfig.h"
#include "config/LlamaServerConfig.h"
#include "infrastructure/llama/LlamaServerProcessController.h"
#include "infrastructure/llama/ModelRegistry.h"
#include "infrastructure/system/SystemMaintenanceFacade.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace {

using frans::application::IdleUnloadTimer;
using frans::application::ResourceCheck;
using frans::application::ResourceDecision;
using frans::application::ResourceGuard;
using frans::config::AppConfig;
using frans::config::LlamaServerConfig;
using frans::domain::CommandResult;
using frans::infrastructure::LlamaServerProcessController;
using frans::infrastructure::ModelEntry;
using frans::infrastructure::ModelRegistry;
using frans::infrastructure::SystemMaintenanceFacade;

struct DonePayload {
    bool         success = false;
    std::string  message;
    std::string  label;
};

struct UiState {
    AppConfig                config;
    SystemMaintenanceFacade  facade;
    ModelRegistry            registry;
    std::unique_ptr<LlamaServerProcessController> llama;
    std::unique_ptr<ResourceGuard>                resource_guard;
    std::unique_ptr<IdleUnloadTimer>              idle_timer;

    Fl_Box*                  mem_label    = nullptr;
    Fl_Box*                  swap_label   = nullptr;
    Fl_Box*                  status_line  = nullptr;
    Fl_Button*               btn_clean    = nullptr;
    Fl_Button*               btn_swap     = nullptr;
    Fl_Button*               btn_sync     = nullptr;
    Fl_Button*               btn_drop     = nullptr;
    Fl_Button*               btn_reset    = nullptr;

    Fl_Choice*               model_choice = nullptr;
    Fl_Button*               btn_llama_start = nullptr;
    Fl_Button*               btn_llama_stop  = nullptr;
    Fl_Box*                  llama_status_label = nullptr;
    Fl_Box*                  llama_log_label    = nullptr;
    Fl_Box*                  mem_required_label = nullptr;

    std::atomic<bool>        busy{false};
    std::chrono::steady_clock::time_point busy_started{};

    std::mutex                   result_mutex;
    std::unique_ptr<DonePayload> pending_result;

    std::string              mem_buf;
    std::string              swap_buf;
    std::string              status_buf;
    std::string              llama_status_buf;
    std::string              llama_log_buf;
    std::string              mem_required_buf;
};

UiState* g_state = nullptr;

constexpr int kWorkerTimeoutSec = 180;   // start llama bisa sampai 120s + cleanMemory

// ---------------------------------------------------------------------------
// Forward declarations.
// Dideklarasikan di atas supaya fungsi yang lebih awal (mis. timer callback)
// bisa memanggilnya tanpa harus menyusun ulang urutan definisi.
// ---------------------------------------------------------------------------
void spawn_worker(const std::string& label,
                  std::function<CommandResult()> fn);

void set_ui_busy(bool busy, const std::string& label);

// ---------------------------------------------------------------------------
// set_ui_busy — HANYA dari main thread.
// ---------------------------------------------------------------------------
void set_ui_busy(bool busy, const std::string& label) {
    auto* st = g_state;
    if (!st) return;

    if (busy) {
        st->busy.store(true);
        st->busy_started = std::chrono::steady_clock::now();
        if (st->btn_clean) st->btn_clean->deactivate();
        if (st->btn_swap)  st->btn_swap->deactivate();
        if (st->btn_sync)  st->btn_sync->deactivate();
        if (st->btn_drop)  st->btn_drop->deactivate();
        if (st->btn_llama_start) st->btn_llama_start->deactivate();
        if (st->btn_llama_stop)  st->btn_llama_stop->deactivate();
        if (st->model_choice)    st->model_choice->deactivate();

        st->status_buf = "RUNNING: " + label + " ...";
        if (st->status_line) st->status_line->copy_label(st->status_buf.c_str());
    } else {
        st->busy.store(false);
        if (st->btn_clean) st->btn_clean->activate();
        if (st->btn_swap)  st->btn_swap->activate();
        if (st->btn_sync)  st->btn_sync->activate();
        if (st->btn_drop)  st->btn_drop->activate();
        if (st->btn_llama_start) st->btn_llama_start->activate();
        if (st->btn_llama_stop)  st->btn_llama_stop->activate();
        if (st->model_choice)    st->model_choice->activate();

        st->status_buf = "idle.";
        if (st->status_line) st->status_line->copy_label(st->status_buf.c_str());
    }
}

// ---------------------------------------------------------------------------
// poll_worker_result — dispatch hasil worker ke main thread.
// ---------------------------------------------------------------------------
void poll_worker_result(void* data) {
    auto* st = static_cast<UiState*>(data);

    DonePayload* ready = nullptr;
    {
        std::lock_guard<std::mutex> lk(st->result_mutex);
        if (st->pending_result) {
            ready = st->pending_result.release();
        }
    }

    if (ready) {
        std::fprintf(stderr, "[frans] poll: deliver result '%s' (success=%d)\n",
                     ready->label.c_str(), ready->success ? 1 : 0);
        std::fflush(stderr);

        set_ui_busy(false, "");
        fl_message("%s: %s\n\n%s",
                   ready->label.c_str(),
                   ready->success ? "SUCCESS" : "SELESAI (ada step gagal)",
                   ready->message.c_str());
        delete ready;
    }

    Fl::repeat_timeout(0.2, poll_worker_result, data);
}

// ---------------------------------------------------------------------------
// watchdog — jaring pengaman kalau worker stuck.
// ---------------------------------------------------------------------------
void watchdog_tick(void* data) {
    auto* st = static_cast<UiState*>(data);
    if (st->busy.load()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - st->busy_started).count();
        if (elapsed >= kWorkerTimeoutSec) {
            std::fprintf(stderr,
                "[frans] WATCHDOG: worker busy %llds — force re-enable UI.\n",
                static_cast<long long>(elapsed));
            std::fflush(stderr);
            set_ui_busy(false, "");
            fl_message("Peringatan: worker belum selesai setelah %d detik.\n"
                       "UI di-reset. Cek terminal untuk log [frans].",
                       kWorkerTimeoutSec);
        }
    }
    Fl::repeat_timeout(1.0, watchdog_tick, data);
}

// ---------------------------------------------------------------------------
// refresh_memory_display — RAM/Swap tiap 2 detik.
// ---------------------------------------------------------------------------
void refresh_memory_display(void* data) {
    auto* st = static_cast<UiState*>(data);
    const auto info = st->facade.getMemoryStatus();

    char buf[256];

    std::snprintf(buf, sizeof(buf),
        "RAM  :  %.2f GB total   /   %.2f GB available",
        info.mem_total_gb(), info.mem_available_gb());
    st->mem_buf = buf;
    if (st->mem_label) st->mem_label->copy_label(st->mem_buf.c_str());

    std::snprintf(buf, sizeof(buf),
        "Swap :  %.2f GB total   /   %.2f GB free",
        static_cast<double>(info.swap_total_kb) / 1024.0 / 1024.0,
        static_cast<double>(info.swap_free_kb)  / 1024.0 / 1024.0);
    st->swap_buf = buf;
    if (st->swap_label) st->swap_label->copy_label(st->swap_buf.c_str());

    Fl::repeat_timeout(2.0, refresh_memory_display, data);
}

// ---------------------------------------------------------------------------
// update_mem_required_label — hitung ulang estimasi RAM untuk model terpilih.
// Dipanggil saat dropdown berubah dan saat startup.
// ---------------------------------------------------------------------------
void update_mem_required_label() {
    auto* st = g_state;
    if (!st || !st->mem_required_label) return;

    const int idx = st->model_choice ? st->model_choice->value() : -1;
    if (idx < 0 || idx >= static_cast<int>(st->registry.models().size())) {
        st->mem_required_buf = "Estimated RAM: (tidak ada model dipilih)";
        st->mem_required_label->copy_label(st->mem_required_buf.c_str());
        return;
    }

    const auto& entry = st->registry.models()[static_cast<std::size_t>(idx)];
    auto req = st->resource_guard->estimateModelRequirement(entry, entry.context_size);

    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "Estimated RAM: %.2f GB  (model %.2f + KV %.2f + overhead %.2f)%s",
        req.total_required_gb,
        req.model_file_gb,
        req.kv_cache_gb,
        req.runtime_overhead_gb,
        req.kv_estimated ? "" : "   [KV diestimasi kasar]");
    st->mem_required_buf = buf;
    st->mem_required_label->copy_label(st->mem_required_buf.c_str());
}

void on_model_choice_changed(Fl_Widget*, void*) {
    update_mem_required_label();
}

// ---------------------------------------------------------------------------
// refresh_llama_status — update label status llama-server tiap 1 detik.
// Termasuk cek idle unload timer dan trigger stop otomatis.
// ---------------------------------------------------------------------------
void refresh_llama_status(void* data) {
    auto* st = static_cast<UiState*>(data);
    if (!st->llama) {
        Fl::repeat_timeout(1.0, refresh_llama_status, data);
        return;
    }

    // --- Idle unload check (blueprint §5.4) ---
    // Hanya trigger kalau: server running, timer expired, UI tidak busy.
    // Karena semua dijalankan di main thread, tidak ada race antara dua tick.
    if (st->llama->isRunning()
        && st->idle_timer
        && st->idle_timer->isRunning()
        && st->idle_timer->expired()
        && !st->busy.load())
    {
        std::fprintf(stderr, "[frans] idle unload: timeout tercapai, stop server.\n");
        std::fflush(stderr);
        spawn_worker("Idle Unload", []() -> CommandResult {
            auto* llama = g_state->llama.get();
            if (g_state->idle_timer) g_state->idle_timer->stop();
            if (!llama->isRunning()) {
                return CommandResult::ok("server memang sudah tidak berjalan.");
            }
            llama->stop();
            return CommandResult::ok("idle timeout tercapai \u2014 server dihentikan.");
        });
    }

    std::string s;
    if (st->llama->isRunning()) {
        s = "llama-server: RUNNING  @  " + st->llama->baseUrl()
          + "  (pid=" + std::to_string(static_cast<int>(st->llama->pid())) + ")";

        if (st->idle_timer && st->idle_timer->isRunning()) {
            const auto rem = st->idle_timer->remaining().count();
            char tail[64];
            std::snprintf(tail, sizeof(tail),
                          "   |   auto-unload in %02ld:%02ld",
                          static_cast<long>(rem / 60),
                          static_cast<long>(rem % 60));
            s += tail;
        }
    } else {
        s = "llama-server: stopped";
        const auto err = st->llama->lastError();
        if (!err.empty()) s += "   |   last error: " + err;
    }
    st->llama_status_buf = s;
    if (st->llama_status_label) st->llama_status_label->copy_label(s.c_str());

    std::string logp = st->llama->logPath();
    if (!logp.empty()) {
        st->llama_log_buf = "log: " + logp;
    } else {
        st->llama_log_buf = "log: (belum ada \u2014 start server dulu)";
    }
    if (st->llama_log_label) st->llama_log_label->copy_label(st->llama_log_buf.c_str());

    Fl::repeat_timeout(1.0, refresh_llama_status, data);
}

// ---------------------------------------------------------------------------
// spawn_worker — definisi.
// ---------------------------------------------------------------------------
void spawn_worker(const std::string& label,
                  std::function<CommandResult()> fn) {
    auto* st = g_state;
    if (!st) return;

    if (st->busy.exchange(true)) {
        std::fprintf(stderr, "[frans] spawn_worker DITOLAK: masih ada worker lain.\n");
        std::fflush(stderr);
        return;
    }

    set_ui_busy(true, label);

    std::thread([label, fn = std::move(fn)]() {
        std::fprintf(stderr, "[frans] worker START: %s\n", label.c_str());
        std::fflush(stderr);

        CommandResult r = fn();

        std::fprintf(stderr, "[frans] worker DONE : %s (exit=%d success=%d)\n",
                     label.c_str(), r.exit_code, r.success ? 1 : 0);
        std::fflush(stderr);

        auto* payload = new DonePayload{r.success, std::move(r.message), label};

        auto* state = g_state;
        if (state) {
            std::lock_guard<std::mutex> lk(state->result_mutex);
            state->pending_result.reset(payload);
        } else {
            delete payload;
        }
    }).detach();
}

// ---------------------------------------------------------------------------
// Callbacks maintenance
// ---------------------------------------------------------------------------
void on_clean_memory(Fl_Widget*, void*) {
    spawn_worker("Clean Memory (all 4)", [] {
        return g_state->facade.cleanMemory();
    });
}

void on_swap_cycle(Fl_Widget*, void*) {
    spawn_worker("Swap Cycle", [] {
        return g_state->facade.swapCycle();
    });
}

void on_sync_only(Fl_Widget*, void*) {
    spawn_worker("Sync", [] {
        return g_state->facade.syncDisks();
    });
}

void on_drop_only(Fl_Widget*, void*) {
    spawn_worker("Drop Caches", [] {
        return g_state->facade.dropCaches();
    });
}

void on_check_sudo(Fl_Widget*, void*) {
    auto* st = g_state;
    const bool ok = st->facade.isSudoAvailable();
    fl_message("%s", ok ? "sudo -n true : OK" : "sudo -n true : GAGAL");
}

void on_check_memory(Fl_Widget*, void*) {
    auto* st = g_state;
    const int idx = st->model_choice ? st->model_choice->value() : -1;
    if (idx < 0 || idx >= static_cast<int>(st->registry.models().size())) {
        fl_message("Pilih model dulu dari dropdown.");
        return;
    }
    const auto& entry = st->registry.models()[static_cast<std::size_t>(idx)];
    const auto r = st->resource_guard->check(entry, entry.context_size);

    const char* verdict = nullptr;
    switch (r.decision) {
        case ResourceDecision::Proceed:                verdict = "PROCEED"; break;
        case ResourceDecision::CleanMemoryThenProceed: verdict = "CLEAN FIRST"; break;
        case ResourceDecision::Reject:                 verdict = "REJECT"; break;
    }

    std::string msg = std::string("Model: ") + entry.id
                    + "\nContext: " + std::to_string(entry.context_size)
                    + "\n\nDecision: " + verdict
                    + "\n\n" + r.explanation;
    if (!r.suggestion.empty()) {
        msg += "\n\nSaran: " + r.suggestion;
    }
    fl_message("%s", msg.c_str());
}

void on_reset_ui(Fl_Widget*, void*) {
    std::fprintf(stderr, "[frans] RESET UI dipanggil user.\n");
    std::fflush(stderr);
    set_ui_busy(false, "");
}

// ---------------------------------------------------------------------------
// Callbacks Llama Server
// ---------------------------------------------------------------------------
void on_llama_start(Fl_Widget*, void*) {
    auto* st = g_state;
    if (!st->llama) {
        fl_message("Llama controller belum diinisialisasi.");
        return;
    }
    if (st->registry.empty()) {
        fl_message("Tidak ada model di config/models.json.");
        return;
    }
    if (st->llama->isRunning()) {
        fl_message("Server sudah berjalan. Stop dulu sebelum start ulang.");
        return;
    }

    const int idx = st->model_choice ? st->model_choice->value() : -1;
    if (idx < 0 || idx >= static_cast<int>(st->registry.models().size())) {
        fl_message("Pilih model dulu dari dropdown.");
        return;
    }

    const ModelEntry entry = st->registry.models()[static_cast<std::size_t>(idx)];

    if (!entry.pathExists()) {
        fl_message("File model tidak ditemukan:\n%s", entry.path.c_str());
        return;
    }

    // --- ResourceGuard pre-flight (blueprint §5.4) ---
    const ResourceCheck check = st->resource_guard->check(entry, entry.context_size);

    std::fprintf(stderr, "[frans] preflight: %s -> decision=%d\n",
                 check.explanation.c_str(), static_cast<int>(check.decision));
    std::fflush(stderr);

    if (check.decision == ResourceDecision::Reject) {
        std::string msg = std::string("RAM tidak cukup untuk model ini.\n\n")
                        + check.explanation;
        if (!check.suggestion.empty()) {
            msg += "\n\nSaran: " + check.suggestion;
        }
        fl_message("%s", msg.c_str());
        return;
    }

    bool need_clean = false;
    if (check.decision == ResourceDecision::CleanMemoryThenProceed) {
        const int ans = fl_choice(
            "%s\n\nJalankan cleanMemory dulu, lalu start server?",
            "Batal",
            "Ya, bersihkan & start",
            nullptr,
            check.explanation.c_str());
        if (ans != 1) {
            std::fprintf(stderr, "[frans] user Batal start (RAM mepet).\n");
            std::fflush(stderr);
            return;
        }
        need_clean = true;
    }

    spawn_worker("Start Llama: " + entry.id, [entry, need_clean]() -> CommandResult {
        std::string prefix;

        if (need_clean) {
            std::fprintf(stderr, "[frans] preflight cleanMemory (RAM mepet)\n");
            std::fflush(stderr);
            const auto clean = g_state->facade.cleanMemory();
            prefix = "cleanMemory: ";
            prefix += (clean.success ? "OK" : "PARTIAL");
            prefix += "\n";
            prefix += clean.message;
            prefix += "----\n";
        }

        LlamaServerProcessController::LaunchSpec spec;
        spec.model_path   = entry.path;
        spec.context_size = entry.context_size;
        spec.threads      = entry.threads;
        spec.n_gpu_layers = 0;

        auto* llama = g_state->llama.get();
        if (!llama->start(spec)) {
            return CommandResult::fail(-1, prefix + "start gagal: " + llama->lastError());
        }
        if (!llama->waitUntilReady()) {
            const std::string err = llama->lastError();
            llama->stop();
            return CommandResult::fail(-1, prefix + "tidak ready: " + err);
        }

        // Server siap → aktifkan idle unload timer.
        if (g_state->idle_timer) g_state->idle_timer->start();

        return CommandResult::ok(prefix + "running di " + llama->baseUrl()
                                 + "\nLog: " + llama->logPath());
    });
}

void on_llama_stop(Fl_Widget*, void*) {
    auto* st = g_state;
    if (!st->llama) {
        fl_message("Llama controller belum diinisialisasi.");
        return;
    }

    spawn_worker("Stop Llama Server", []() -> CommandResult {
        // Hentikan idle timer dulu — supaya tidak balapan dengan refresh_llama_status.
        if (g_state->idle_timer) g_state->idle_timer->stop();

        auto* llama = g_state->llama.get();
        if (!llama->isRunning()) {
            return CommandResult::ok("server memang tidak berjalan.");
        }
        llama->stop();
        return CommandResult::ok("server dihentikan.");
    });
}

} // namespace

int main(int argc, char** argv) {
    AppConfig cfg = AppConfig::loadFromFile("config/app.json");
    LlamaServerConfig ls_cfg = LlamaServerConfig::loadFromFile("config/providers.json");

    UiState state;
    state.config   = cfg;
    state.registry = ModelRegistry::loadFromFile("config/models.json");
    g_state        = &state;

    // Bangun LlamaServerProcessController dari config.
    {
        LlamaServerProcessController::Config cc;
        cc.binary_path         = ls_cfg.binary;
        cc.host                = ls_cfg.host;
        cc.port                = ls_cfg.port;
        cc.startup_timeout_sec = ls_cfg.startup_timeout_sec;
        cc.log_dir             = cfg.log_dir;
        state.llama = std::make_unique<LlamaServerProcessController>(std::move(cc));
    }

    // ResourceGuard (blueprint §5.4).
    state.resource_guard = std::make_unique<ResourceGuard>(cfg);

    // Idle unload timer — timeout dari config/app.json (idle_unload_minutes).
    state.idle_timer = std::make_unique<IdleUnloadTimer>(
        std::chrono::seconds(cfg.idle_unload_minutes * 60));

    Fl_Window* win = new Fl_Window(cfg.window_width, cfg.window_height,
                                   cfg.window_title.c_str());
    win->begin();

    const int W = cfg.window_width;

    // -------- Header --------
    Fl_Box* title = new Fl_Box(20, 12, W - 40, 30,
                               "frans_ai  \u2014  Stage 3: ResourceGuard + IdleUnloadTimer");
    title->labelfont(FL_BOLD);
    title->labelsize(17);
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    Fl_Box* sub = new Fl_Box(20, 42, W - 40, 18,
                             "KV cache estimation + pre-flight check + auto-unload setelah idle");
    sub->labelsize(11);
    sub->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    // -------- Memory box --------
    Fl_Box* mem_title = new Fl_Box(20, 68, 400, 18,
        "Memory status  (auto-refresh 2 detik)");
    mem_title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    mem_title->labelsize(11);

    state.mem_label = new Fl_Box(20, 88, W - 40, 26, "RAM  :  (loading...)");
    state.mem_label->box(FL_DOWN_BOX);
    state.mem_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    state.mem_label->labelsize(12);

    state.swap_label = new Fl_Box(20, 118, W - 40, 26, "Swap :  (loading...)");
    state.swap_label->box(FL_DOWN_BOX);
    state.swap_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    state.swap_label->labelsize(12);

    state.status_line = new Fl_Box(20, 152, W - 40, 20, "idle.");
    state.status_line->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    state.status_line->labelsize(11);
    state.status_line->labelcolor(FL_DARK_BLUE);

    // -------- Buttons maintenance (baris 1) --------
    const int bh = 32;
    const int gap = 8;
    int by = 178;
    int bx = 20;

    state.btn_clean = new Fl_Button(bx, by, 200, bh, "Clean Memory (all 4)");
    state.btn_clean->callback(on_clean_memory);
    bx += 200 + gap;

    state.btn_swap = new Fl_Button(bx, by, 170, bh, "Swap Cycle");
    state.btn_swap->callback(on_swap_cycle);
    bx += 170 + gap;

    state.btn_sync = new Fl_Button(bx, by, 80, bh, "Sync");
    state.btn_sync->callback(on_sync_only);
    bx += 80 + gap;

    state.btn_drop = new Fl_Button(bx, by, 120, bh, "Drop Caches");
    state.btn_drop->callback(on_drop_only);

    // -------- Buttons maintenance (baris 2) --------
    by += bh + 6;
    Fl_Button* sudo_btn = new Fl_Button(20, by, 120, bh, "Check sudo");
    sudo_btn->callback(on_check_sudo);

    state.btn_reset = new Fl_Button(150, by, 120, bh, "Reset UI");
    state.btn_reset->callback(on_reset_ui);
    state.btn_reset->tooltip("Paksa re-enable semua tombol kalau terkunci.");

    Fl_Button* check_mem_btn = new Fl_Button(280, by, 140, bh, "Check Memory");
    check_mem_btn->callback(on_check_memory);
    check_mem_btn->tooltip("Dry-run ResourceGuard untuk model terpilih.");

    // ========================================================================
    // Section: Llama Server
    // ========================================================================
    by += bh + 14;

    Fl_Box* ltitle = new Fl_Box(20, by, W - 40, 20, "Llama Server (local, llama.cpp)");
    ltitle->labelfont(FL_BOLD);
    ltitle->labelsize(12);
    ltitle->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    by += 22;

    // Model dropdown
    state.model_choice = new Fl_Choice(20, by, 480, bh, nullptr);
    for (const auto& m : state.registry.models()) {
        char lbl[256];
        std::snprintf(lbl, sizeof(lbl), "%s  [%s, %.1f GB]",
                      m.id.c_str(), m.role.c_str(), m.size_gb);
        state.model_choice->add(lbl);
    }
    if (!state.registry.empty()) {
        state.model_choice->value(0);
    } else {
        state.model_choice->add("(tidak ada model di config/models.json)");
        state.model_choice->value(0);
        state.model_choice->deactivate();
    }
    state.model_choice->tooltip("Pilih model dari config/models.json");
    state.model_choice->callback(on_model_choice_changed);

    state.btn_llama_start = new Fl_Button(510, by, 130, bh, "Start Server");
    state.btn_llama_start->callback(on_llama_start);

    state.btn_llama_stop = new Fl_Button(650, by, 130, bh, "Stop Server");
    state.btn_llama_stop->callback(on_llama_stop);

    by += bh + 4;

    // Estimated RAM label
    state.mem_required_label = new Fl_Box(20, by, W - 40, 20,
        "Estimated RAM: (menghitung...)");
    state.mem_required_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    state.mem_required_label->labelsize(11);
    state.mem_required_label->labelcolor(FL_DARK_GREEN);

    by += 24;

    // Status label
    state.llama_status_label = new Fl_Box(20, by, W - 40, 22,
        "llama-server: stopped");
    state.llama_status_label->box(FL_DOWN_BOX);
    state.llama_status_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    state.llama_status_label->labelsize(11);

    by += 26;

    state.llama_log_label = new Fl_Box(20, by, W - 40, 18,
        "log: (belum ada \u2014 start server dulu)");
    state.llama_log_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    state.llama_log_label->labelsize(10);

    by += 24;

    // -------- Hints --------
    Fl_Box* hint = new Fl_Box(20, by, W - 40, cfg.window_height - by - 15,
        "Catatan Stage 3:\n"
        "  \u2022 ResourceGuard: sebelum start, UI hitung kebutuhan RAM = ukuran file .gguf\n"
        "    + KV cache (rumus: 2 x layers x kv_heads x head_dim x 2 byte x context)\n"
        "    + overhead 0.5 GB. Dibandingkan dengan MemAvailable di /proc/meminfo.\n"
        "  \u2022 Decision: Proceed (headroom >= threshold), CleanMemoryThenProceed (mepet),\n"
        "    Reject (tidak cukup \u2014 UI beri saran: context lebih kecil / model 3B).\n"
        "  \u2022 Idle unload: setelah 5 menit tanpa aktivitas, server otomatis di-stop.\n"
        "    (Stage 4: HTTP request ke /v1/chat/completions akan reset timer ini.)\n"
        "  \u2022 Tombol 'Check Memory' = dry-run, tidak spawn apa pun.\n"
        "  \u2022 KV cache dihitung dari blok \"architecture\" di config/models.json \u2014 kalau\n"
        "    tidak ada, fallback estimasi kasar (ditandai di label).");
    hint->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    hint->labelsize(11);

    win->end();
    win->show(argc, argv);

    // Isi label estimasi RAM untuk model pertama.
    update_mem_required_label();

    // Timer
    Fl::add_timeout(0.4, refresh_memory_display, &state);
    Fl::add_timeout(0.5, refresh_llama_status,   &state);
    Fl::add_timeout(1.0, watchdog_tick,          &state);
    Fl::add_timeout(0.2, poll_worker_result,     &state);

    std::printf("[frans_ai] startup OK - FLTK %d.%d.%d\n",
                FL_MAJOR_VERSION, FL_MINOR_VERSION, FL_PATCH_VERSION);
    std::printf("[frans_ai] models loaded: %zu\n", state.registry.models().size());
    for (const auto& m : state.registry.models()) {
        const auto req = state.resource_guard->estimateModelRequirement(m, m.context_size);
        std::printf("[frans_ai]   - %-24s  file=%.2f GB  kv=%.3f GB  total=%.2f GB%s\n",
                    m.id.c_str(),
                    req.model_file_gb, req.kv_cache_gb, req.total_required_gb,
                    req.kv_estimated ? "" : "  [KV estimated]");
    }
    std::printf("[frans_ai] llama binary: %s\n", ls_cfg.binary.c_str());
    std::printf("[frans_ai] llama endpoint target: %s:%d\n",
                ls_cfg.host.c_str(), ls_cfg.port);
    std::printf("[frans_ai] idle unload: %d menit\n", cfg.idle_unload_minutes);
    std::printf("[frans_ai] safety margin: %d MB\n", cfg.ram_available_threshold_mb);
    std::fflush(stdout);

    return Fl::run();
}
