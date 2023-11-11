#pragma once

#include <smt-switch/solver.h>
#include <boost/multiprecision/cpp_int.hpp>

using namespace smt;
using namespace boost::multiprecision;

class Util {

    SmtSolver solver;

public:

    const Sort int_sort;
    const Term exp;

    Util(SmtSolver solver);
    cpp_int value(const Term term) const;
    std::optional<cpp_int> evaluate_ground_int(Term expression) const;
    Term term(const cpp_int &value) const;

};
