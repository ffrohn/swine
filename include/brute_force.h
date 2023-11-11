#pragma once

#include <smt-switch/term.h>
#include <map>

using namespace smt;

class BruteForce {
    SmtSolver solver;
    TermVec assertions;
    TermVec exps;
    ulong bound {0};
    std::vector<std::pair<Term, ulong>> current;

    static const uint max_bound {10};

    bool next();
    bool next(const ulong weight, std::vector<std::pair<Term, ulong>>::iterator begin);

public:

    BruteForce(SmtSolver solver, const TermVec &assertions, const TermVec &exps);
    bool check_sat();

};
