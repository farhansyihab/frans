// ~/AgenAI/frans/src/application/agent/IAgentState.h
#pragma once

#include <string>

namespace frans::application {

// Blueprint §5.7 — satu langkah kerja agent.
//
// Catatan Stage 7b: kontrak ini sengaja MINIMAL. run() return void, bukan
// unique_ptr<IAgentState> next-state, karena orkestrasi transisi antar-state
// belum dibangun (itu Stage 7d). Untuk sekarang caller (test/UI) yang
// memanggil run() secara eksplisit, satu state pada satu waktu.
class IAgentState {
public:
    virtual ~IAgentState() = default;

    // Nama pendek untuk logging / UI ("Generate", "Execute", ...).
    virtual std::string name() const = 0;

    // Jalankan satu langkah kerja. State membaca/menulis AgentContext.
    // Implementasi TIDAK boleh throw — semua kegagalan dilaporkan lewat
    // ctx.success + ctx.lastError.
    virtual void run(struct AgentContext& ctx) = 0;
};

} // namespace frans::application
