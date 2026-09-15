// ~/AgenAI/frans/tests/escalation_policy_smoke_test.cpp
//
// Stage 12 — EscalationPolicy smoke test (3-tier).
//
// API lama: bool shouldEscalate(const EscalationContext&, TaskComplexity)
// API baru: RoutingTier decide(const EscalationContext&, TaskComplexity)
//
// Test di bawah murni menguji fungsi decide() — stateless, tanpa
// filesystem, tanpa network.
#include "application/provider_router/EscalationPolicy.h"

#include <cstdio>
#include <string>

using namespace frans::application;

namespace {

struct TestCase {
    std::string        name;
    EscalationContext  esc;
    TaskComplexity     complexity;
    RoutingTier        expected;
};

int runCase(const TestCase& t, const EscalationPolicy& p) {
    const RoutingTier got = p.decide(t.esc, t.complexity);
    if (got != t.expected) {
        std::fprintf(stderr,
            "FAIL [%s]: expected tier=%d, got tier=%d\n",
            t.name.c_str(),
            static_cast<int>(t.expected),
            static_cast<int>(got));
        return 1;
    }
    std::printf("PASS [%s] -> tier=%d\n",
                t.name.c_str(), static_cast<int>(got));
    return 0;
}

} // namespace

int main() {
    std::printf("[escalation_policy_smoke] starting...\n");

    const EscalationPolicy policy;
    int fails = 0;

    // -----------------------------------------------------------------
    // Aturan 1: RequiresDeepReasoning -> PaidHeavy langsung
    // -----------------------------------------------------------------
    const TestCase cases[] = {
        // ---------- Default: tetap Local ----------
        {"trivial fresh",
         EscalationContext{},
         TaskComplexity::Trivial,
         RoutingTier::Local},

        {"moderate fresh",
         EscalationContext{},
         TaskComplexity::Moderate,
         RoutingTier::Local},

        {"moderate local_fail=1",
         EscalationContext{1, 0, RoutingTier::Local},
         TaskComplexity::Moderate,
         RoutingTier::Local},

        // ---------- Naik ke PaidLight setelah 2x fail lokal ----------
        {"moderate local_fail=2 -> PaidLight",
         EscalationContext{2, 0, RoutingTier::Local},
         TaskComplexity::Moderate,
         RoutingTier::PaidLight},

        {"moderate local_fail=5 -> PaidLight",
         EscalationContext{5, 0, RoutingTier::Local},
         TaskComplexity::Moderate,
         RoutingTier::PaidLight},

        // ---------- Complex: minimal PaidLight ----------
        {"complex fresh -> PaidLight",
         EscalationContext{},
         TaskComplexity::Complex,
         RoutingTier::PaidLight},

        {"complex with max_tier=PaidHeavy -> tetap PaidHeavy",
         EscalationContext{0, 0, RoutingTier::PaidHeavy},
         TaskComplexity::Complex,
         RoutingTier::PaidHeavy},

        // ---------- RequiresDeepReasoning: PaidHeavy ----------
        {"requires-deep-reasoning fresh -> PaidHeavy",
         EscalationContext{},
         TaskComplexity::RequiresDeepReasoning,
         RoutingTier::PaidHeavy},

        {"RDR dengan max_tier=PaidLight -> tetap PaidHeavy",
         EscalationContext{0, 0, RoutingTier::PaidLight},
         TaskComplexity::RequiresDeepReasoning,
         RoutingTier::PaidHeavy},

        // ---------- Sticky di PaidLight ----------
        {"sticky PaidLight, light_fail=0 -> PaidLight",
         EscalationContext{0, 0, RoutingTier::PaidLight},
         TaskComplexity::Moderate,
         RoutingTier::PaidLight},

        {"sticky PaidLight, light_fail=1 -> PaidLight",
         EscalationContext{0, 1, RoutingTier::PaidLight},
         TaskComplexity::Moderate,
         RoutingTier::PaidLight},

        {"sticky PaidLight, light_fail=2 -> PaidHeavy",
         EscalationContext{0, 2, RoutingTier::PaidLight},
         TaskComplexity::Moderate,
         RoutingTier::PaidHeavy},

        {"sticky PaidLight, light_fail=3 -> PaidHeavy",
         EscalationContext{0, 3, RoutingTier::PaidLight},
         TaskComplexity::Trivial,
         RoutingTier::PaidHeavy},

        // ---------- Sticky di PaidHeavy (tidak turun) ----------
        {"sticky PaidHeavy, complexity=Trivial -> tetap PaidHeavy",
         EscalationContext{0, 0, RoutingTier::PaidHeavy},
         TaskComplexity::Trivial,
         RoutingTier::PaidHeavy},

        {"sticky PaidHeavy, complexity=Moderate -> tetap PaidHeavy",
         EscalationContext{0, 0, RoutingTier::PaidHeavy},
         TaskComplexity::Moderate,
         RoutingTier::PaidHeavy},

        {"sticky PaidHeavy abaikan local_fail tinggi",
         EscalationContext{99, 99, RoutingTier::PaidHeavy},
         TaskComplexity::Trivial,
         RoutingTier::PaidHeavy},
    };

    for (const auto& c : cases) {
        fails += runCase(c, policy);
    }

    if (fails > 0) {
        std::fprintf(stderr,
            "[escalation_policy_smoke] %d check(s) FAILED\n", fails);
        return 1;
    }
    std::printf("[escalation_policy_smoke] all %zu checks passed\n",
                sizeof(cases) / sizeof(cases[0]));
    return 0;
}