#include "util.h"

ExponentOverflow::ExponentOverflow(const Term t): std::out_of_range(""), t(t) {}

Util::Util(const SmtSolver solver, const Config &config):
    config(config),
    solver(solver),
    int_sort(solver->make_sort(SortKind::INT)),
    exp(solver->make_symbol("exp", solver->make_sort(SortKind::FUNCTION, {int_sort, int_sort, int_sort}))),
    True(solver->make_term(true)),
    False(solver->make_term(false)) {}

Term ExponentOverflow::get_t() const {
    return t;
}

cpp_int to_cpp_int(const std::string &s) {
    if (s.starts_with("(- ")) {
        return -to_cpp_int(s.substr(3, s.size() - 4));
    } else {
        return cpp_int(s);
    }
}

cpp_int Util::value(const Term term) const {
    return to_cpp_int(term->to_string());
}

Term Util::term(const cpp_int &value) const {
    return solver->make_term(value.str(), int_sort);
}

bool Util::is_app(const Term term) const {
    return !term->is_symbolic_const() && !term->is_param() && !term->is_symbol() && !term->is_value();
}

bool Util::is_abstract_exp(const Term term) const {
    return is_app(term) && term->get_op().prim_op == Apply && *term->begin() == exp;
}

std::pair<Term, Term> Util::decompose_exp(const Term term) const {
    if (!is_abstract_exp(term)) {
        throw std::invalid_argument("called decompose_exp with " + term->to_string());
    }
    auto it {term->begin()};
    return {*(++it), *(++it)};
}

Term Util::make_exp(const Term base, const Term exponent) {
    return solver->make_term(Apply, exp, base, exponent);
}

long long Util::to_int(const Term t) {
    try {
        return stoll(to_cpp_int(t->to_string()).str());
    } catch (const std::out_of_range &e) {
        throw ExponentOverflow(t);
    }
}
