#pragma once

enum SolverKind {
    Z3, CVC5, Yices
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
};
