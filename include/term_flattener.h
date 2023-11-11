#pragma once

#include <smt-switch/solver.h>
#include <boost/multiprecision/cpp_int.hpp>

#include "util.h"

using namespace smt;
using namespace boost::multiprecision;

class TermFlattener {

    UnorderedTermMap replacement_map;

    UnorderedTermSet exps;

    UnorderedTermMap map;

    Util &util;

    Term replacement_var(Term term);

    Term flatten(Term term, UnorderedTermSet &set);

public:

    TermFlattener(Util &util);

    Term flatten(Term term);

    UnorderedTermSet clear_exps();

};
