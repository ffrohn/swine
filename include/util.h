#pragma once

#include <smt-switch/solver.h>
#include <boost/multiprecision/cpp_int.hpp>

using namespace smt;
using namespace boost::multiprecision;

class Util {

public:

    const SmtSolver solver;
    const Sort int_sort;
    const Term exp;

    Util(const SmtSolver solver);
    cpp_int value(const Term term) const;
    Term term(const cpp_int &value) const;

};
