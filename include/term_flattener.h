#pragma once

#include <smt-switch/solver.h>

using namespace smt;

class TermFlattener {

    SmtSolver solver;

    const Term exp;

    UnorderedTermSet exps;

    Term next_var(Term term, UnorderedTermMap &map);

    Term flatten(Term term, UnorderedTermMap &map);

public:

    TermFlattener(SmtSolver solver, const Term exp);

    Term flatten(Term term);

    UnorderedTermSet clear_exps();

};
