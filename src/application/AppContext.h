// ~/AgenAI/frans/src/application/AppContext.h
#pragma once

#include "application/budget/DailyBudgetGuard.h"
#include "application/project/ProjectGraphCache.h"
#include "application/resource_guard/IdleUnloadTimer.h"
#include "application/resource_guard/ResourceGuard.h"
#include "config/AppConfig.h"
#include "infrastructure/llama/LlamaServerProcessController.h"
#include "infrastructure/llama/ModelRegistry.h"
#include "infrastructure/system/SystemMaintenanceFacade.h"

#include <memory>

namespace frans::application {

// State aplikasi non-UI yang dibagi ke seluruh panel.
//
// Konstruksi dilakukan sekali di main() (lihat buildAppContext di main.cpp),
// lalu di-share ke panel lewat pointer. Panel TIDAK boleh menambah state
// bisnis di sini — ini murni "shared services".
//
// Stage 7f: tambah graph_cache (single-slot, hidup selama aplikasi).
// Ditaruh di sini (bukan di AgentPanel) supaya bisa dipakai fitur lain
// yang mungkin butuh nanti (mis. syntax checker).
//
// Stage 9c: tambah budget_guard — hidup selama aplikasi supaya spent_usd
// konsisten lintas task run dalam satu sesi. Kalau budget_guard dibuat
// per-task di AgentPanel, spent_usd akan reset tiap task dan rollover
// harian tidak bisa di-enforce.
struct AppContext {
    frans::config::AppConfig                       config;
    frans::infrastructure::SystemMaintenanceFacade facade;
    frans::infrastructure::ModelRegistry           registry;

    std::unique_ptr<frans::infrastructure::LlamaServerProcessController> llama;
    std::unique_ptr<ResourceGuard>                 resource_guard;
    std::unique_ptr<IdleUnloadTimer>               idle_timer;
    std::unique_ptr<ProjectGraphCache>             graph_cache;
    std::unique_ptr<DailyBudgetGuard>              budget_guard;   // Stage 9c
};

} // namespace frans::application
