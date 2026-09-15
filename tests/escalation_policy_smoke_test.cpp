// ~/AgenAI/frans/tests/escalation_policy_smoke_test.cpp
//
// Smoke test Stage 9b: EscalationPolicy.
//
// Murni unit test — tidak butuh llama-server, tidak butuh API key,
// tidak butuh network. Semua keputusan adalah fungsi murni dari
// (EscalationContext, TaskComplexity).
//
// EscalationPolicy tidak menyimpan state internal — semua informasi
// keputusan datang dari parameter. Jadi test ini adalah tabel
// input→expected yang straightforward.

#include "application/provider_router/EscalationPolicy.h"

#include <cstdio>
#include <string>
#include <vector>

using frans::application::EscalationContext;
using frans::application::EscalationPolicy;
using frans::application::TaskComplexity;

namespace {

int g_failures = 0;

struct TestCase {
    std::string       label;
    EscalationContext esc;
    TaskComplexity    complexity;
    bool              expected;
};

void runCase(const TestCase& t, const EscalationPolicy& p) {
    const bool got  = p.shouldEscalate(t.esc, t.complexity);
    const bool pass = (got == t.expected);
    std::printf("  %s  %-62s  (expected=%d, got=%d)\n",
                pass ? "PASS" : "FAIL",
                t.label.c_str(),
                t.expected ? 1 : 0,
                got ? 1 : 0);
    if (!pass) ++g_failures;
}

} // namespace

int main() {
    std::printf("=== frans_ai escalation policy smoke test (Stage 9b) ===\n\n");

    const EscalationPolicy policy;

    const std::vector<TestCase> cases = {
        // ================================================================
        // Grup 1: Trivial / Moderate dengan streak rendah → tetap local
        // ================================================================
        {
            "Trivial, streak=0, escalated=false -> false",
            EscalationContext{0, false},
            TaskComplexity::Trivial,
            false,
        },
        {
            "Moderate, streak=0, escalated=false -> false",
            EscalationContext{0, false},
            TaskComplexity::Moderate,
            false,
        },
        {
            "Moderate, streak=1, escalated=false -> false (belum 2x)",
            EscalationContext{1, false},
            TaskComplexity::Moderate,
            false,
        },

        // ================================================================
        // Grup 2: Local gagal 2x / lebih → eskalasi
        // ================================================================
        {
            "Moderate, streak=2, escalated=false -> true (2x gagal)",
            EscalationContext{2, false},
            TaskComplexity::Moderate,
            true,
        },
        {
            "Moderate, streak=3, escalated=false -> true (>2x gagal)",
            EscalationContext{3, false},
            TaskComplexity::Moderate,
            true,
        },
        {
            "Trivial, streak=5, escalated=false -> true (trivial pun bisa naik)",
            EscalationContext{5, false},
            TaskComplexity::Trivial,
            true,
        },

        // ================================================================
        // Grup 3: Sticky (sudah pernah eskalasi di run ini) → tetap true
        // ================================================================
        {
            "Trivial, streak=0, escalated=true -> true (sticky)",
            EscalationContext{0, true},
            TaskComplexity::Trivial,
            true,
        },
        {
            "Moderate, streak=0, escalated=true -> true (sticky)",
            EscalationContext{0, true},
            TaskComplexity::Moderate,
            true,
        },

        // ================================================================
        // Grup 4: Complex / RequiresDeepReasoning → selalu true (skip local)
        // ================================================================
        {
            "Complex, streak=0, escalated=false -> true (skip local)",
            EscalationContext{0, false},
            TaskComplexity::Complex,
            true,
        },
        {
            "Complex, streak=0, escalated=true -> true (redundant, tetap true)",
            EscalationContext{0, true},
            TaskComplexity::Complex,
            true,
        },
        {
            "RequiresDeepReasoning, streak=0, escalated=false -> true",
            EscalationContext{0, false},
            TaskComplexity::RequiresDeepReasoning,
            true,
        },
        {
            "RequiresDeepReasoning, streak=0, escalated=true -> true",
            EscalationContext{0, true},
            TaskComplexity::RequiresDeepReasoning,
            true,
        },
    };

    for (const auto& c : cases) {
        runCase(c, policy);
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
