#pragma once

#include <smt-switch/solver.h>
#include <boost/multiprecision/cpp_int.hpp>

#include "config.h"

namespace swine {

using namespace smt;
using namespace boost::multiprecision;

class ExponentOverflow: public std::out_of_range {

    Term t;

public:

    ExponentOverflow(const Term t);

    Term get_t() const;

};

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
    Term make_exp(const Term base, const Term exponent);

    static long long to_int(const cpp_int &i);
    static long long to_int(const Term t);

};

}
