#pragma once

#include "lemma_kind.h"
#include "preproc_kind.h"

#include <unordered_set>

enum class SolverKind {
    Z3, CVC5
};

enum class Semantics {
    Partial, Total
};

struct Config {
    bool validate {false};
    bool log {false};
    bool statistics {false};
    bool get_lemmas {false};
    unsigned int rewrite_threshold {10};
    SolverKind solver_kind {SolverKind::Z3};
    Semantics semantics {Semantics::Partial};
    std::unordered_set<LemmaKind> active_lemma_kinds {lemma_kind::values};
    std::unordered_set<PreprocKind> active_preprocessings{preproc_kind::values};

    void deactivate(const LemmaKind k) {
        active_lemma_kinds.erase(k);
    }

    bool is_active(const LemmaKind k) const {
        return active_lemma_kinds.contains(k);
    }

    void deactivate(const PreprocKind k) {
        active_preprocessings.erase(k);
    }

    bool is_active(const PreprocKind k) const {
        return active_preprocessings.contains(k);
    }

};
