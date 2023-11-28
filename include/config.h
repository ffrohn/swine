#pragma once

#include "lemma_kind.h"

#include <unordered_set>

enum SolverKind {
    Z3, CVC5
};

enum Semantics {
    Partial, Total
};

struct Config {
    bool validate {false};
    bool log {false};
    bool statistics {false};
    SolverKind solver_kind {SolverKind::CVC5};
    Semantics semantics {Partial};
    std::unordered_set<LemmaKind> active_lemma_kinds {lemma_kind::values};

    void deactivate(const LemmaKind k) {
        active_lemma_kinds.erase(k);
    }

    bool is_active(const LemmaKind k) const {
        return active_lemma_kinds.contains(k);
    }

};
