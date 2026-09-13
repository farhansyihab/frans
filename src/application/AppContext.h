// ~/AgenAI/frans/src/application/AppContext.h
#pragma once

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
struct AppContext {
    frans::config::AppConfig                       config;
    frans::infrastructure::SystemMaintenanceFacade facade;
    frans::infrastructure::ModelRegistry           registry;

    std::unique_ptr<frans::infrastructure::LlamaServerProcessController> llama;
    std::unique_ptr<ResourceGuard>                 resource_guard;
    std::unique_ptr<IdleUnloadTimer>               idle_timer;
    std::unique_ptr<ProjectGraphCache>             graph_cache;
};

} // namespace frans::application
