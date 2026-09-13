// ~/AgenAI/frans/src/main.cpp
//
// Entry point frans_ai.
//
// Tanggung jawab main.cpp SEMPIT:
//   1. Load config dari config/*.json.
//   2. Bangun AppContext (services + controller).
//   3. Buat MainWindow, jalankan FLTK event loop.
//
// Logika UI dipindah ke src/ui/ (MainWindow + panel).
// State non-UI dipindah ke application/AppContext.h.

#include "application/AppContext.h"
#include "application/project/ProjectGraphCache.h"
#include "application/resource_guard/IdleUnloadTimer.h"
#include "application/resource_guard/ResourceGuard.h"
#include "config/AppConfig.h"
#include "config/LlamaServerConfig.h"
#include "infrastructure/llama/LlamaServerProcessController.h"
#include "infrastructure/llama/ModelRegistry.h"
#include "ui/MainWindow.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include <chrono>
#include <cstdio>
#include <memory>
#include <utility>

namespace {

void printStartupBanner(const frans::application::AppContext& ctx,
                        const frans::config::LlamaServerConfig& ls_cfg)
{
    std::printf("[frans_ai] startup OK - FLTK %d.%d.%d\n",
                FL_MAJOR_VERSION, FL_MINOR_VERSION, FL_PATCH_VERSION);
    std::printf("[frans_ai] models loaded: %zu\n",
                ctx.registry.models().size());
    for (const auto& m : ctx.registry.models()) {
        const auto req = ctx.resource_guard->estimateModelRequirement(
            m, m.context_size);
        std::printf(
            "[frans_ai]   - %-24s  file=%.2f GB  kv=%.3f GB  total=%.2f GB%s\n",
            m.id.c_str(),
            req.model_file_gb, req.kv_cache_gb, req.total_required_gb,
            req.kv_estimated ? "" : "  [KV estimated]");
    }
    std::printf("[frans_ai] llama binary: %s\n", ls_cfg.binary.c_str());
    std::printf("[frans_ai] llama endpoint target: %s:%d\n",
                ls_cfg.host.c_str(), ls_cfg.port);
    std::printf("[frans_ai] idle unload: %d menit\n",
                ctx.config.idle_unload_minutes);
    std::printf("[frans_ai] safety margin: %d MB\n",
                ctx.config.ram_available_threshold_mb);
    std::printf("[frans_ai] chat provider: /v1/chat/completions (SSE streaming)\n");
    std::printf("[frans_ai] graph cache: single-slot (Stage 7f)\n");
    std::fflush(stdout);
}

frans::application::AppContext buildAppContext(
    const frans::config::AppConfig& cfg,
    const frans::config::LlamaServerConfig& ls_cfg)
{
    using frans::application::AppContext;
    using frans::application::IdleUnloadTimer;
    using frans::application::ProjectGraphCache;
    using frans::application::ResourceGuard;
    using frans::infrastructure::LlamaServerProcessController;
    using frans::infrastructure::ModelRegistry;

    AppContext ctx;
    ctx.config   = cfg;
    ctx.registry = ModelRegistry::loadFromFile("config/models.json");

    // Llama server process controller.
    LlamaServerProcessController::Config cc;
    cc.binary_path         = ls_cfg.binary;
    cc.host                = ls_cfg.host;
    cc.port                = ls_cfg.port;
    cc.startup_timeout_sec = ls_cfg.startup_timeout_sec;
    cc.log_dir             = cfg.log_dir;
    ctx.llama = std::make_unique<LlamaServerProcessController>(std::move(cc));

    // Resource guard (KV-cache estimator + /proc/meminfo reader).
    ctx.resource_guard = std::make_unique<ResourceGuard>(cfg);

    // Idle unload timer (mulai dari stopped; di-start saat server ready).
    ctx.idle_timer = std::make_unique<IdleUnloadTimer>(
        std::chrono::seconds(cfg.idle_unload_minutes * 60));

    // Stage 7f: project graph cache (single-slot).
    ctx.graph_cache = std::make_unique<ProjectGraphCache>();

    return ctx;
}

} // namespace

int main(int argc, char** argv) {
    using frans::config::AppConfig;
    using frans::config::LlamaServerConfig;

    const AppConfig cfg = AppConfig::loadFromFile("config/app.json");
    const LlamaServerConfig ls =
        LlamaServerConfig::loadFromFile("config/providers.json");

    frans::application::AppContext ctx = buildAppContext(cfg, ls);

    frans::ui::MainWindow window(&ctx);
    Fl_Window* win = window.create();
    win->show(argc, argv);

    printStartupBanner(ctx, ls);
    window.startTimers();

    return Fl::run();
}
