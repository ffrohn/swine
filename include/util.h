#pragma once

#include <smt-switch/solver.h>
#include <boost/multiprecision/cpp_int.hpp>

#include "config.h"

using namespace smt;
using namespace boost::multiprecision;

class Util {

public:

    const Config &config;
    const SmtSolver solver;
    const Sort int_sort;
    const Term exp;
    const Term True;
    const Term False;

    Util(const SmtSolver solver, const Config &config);
    cpp_int value(const Term term) const;
    Term term(const cpp_int &value) const;
    bool is_app(const Term term) const;
    bool is_abstract_exp(const Term term) const;
    std::pair<Term, Term> decompose_exp(const Term term) const;

};
