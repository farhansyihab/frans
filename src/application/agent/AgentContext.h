// ~/AgenAI/frans/src/application/agent/AgentContext.h
#pragma once

#include "code_intelligence/parsers/CodeSnippet.h"

#include <string>
#include <vector>

namespace frans::application {

// State yang dibawa lintas state agent (Generate → Execute → Observe → Fix).
//
// Stage 7b hanya memakai subset field: taskDescription, targetRelativePath,
// contextSnippets, lastGeneratedCode, lastError, success. Field tambahan
// (nanti) ditambahkan saat ExecuteState/ObserveState dibangun di Stage 7c-7d.
//
// Konvensi: setiap state WAJIB mengisi ctx.success dengan benar sebelum
// return, dan mengosongkan ctx.lastError saat sukses.
struct AgentContext {
    // Instruksi user, mentah (natural language).
    std::string taskDescription;

    // Path relatif terhadap sandbox root, mis. "generated/solve.py".
    // Bukan path absolut — SandboxedFileSystem yang resolve.
    std::string targetRelativePath;

    // Snippet yang dipilih ContextSelector. Diisi GenerateState, disimpan
    // untuk audit dan untuk dipakai state berikutnya (mis. FixState
    // menambahkan error message tapi tetap butuh snippet konteks).
    std::vector<frans::code_intelligence::CodeSnippet> contextSnippets;

    // Kode yang baru saja digenerate (isi fenced block, tanpa fence-nya).
    std::string lastGeneratedCode;

    // Pesan error terakhir kalau success == false. Human-readable, siap
    // ditampilkan ke user.
    std::string lastError;

    // Hasil langkah terakhir. Setiap state WAJIB set ini.
    bool success = false;
};

} // namespace frans::application
