#pragma once

#include <smt-switch/solver.h>

using namespace smt;

class TermFlattener {

    SmtSolver solver;

    const Term exp;

    UnorderedTermMap replacement_map;

    UnorderedTermSet exps;

    UnorderedTermMap map;

    Term replacement_var(Term term);

    Term flatten(Term term, UnorderedTermSet &set);

public:

    TermFlattener(SmtSolver solver, const Term exp);

    Term flatten(Term term);

    UnorderedTermSet clear_exps();

};
