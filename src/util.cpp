#include "util.h"

Util::Util(const SmtSolver solver):
    solver(solver),
    int_sort(solver->make_sort(SortKind::INT)),
    exp(solver->make_symbol("exp", solver->make_sort(SortKind::FUNCTION, {int_sort, int_sort, int_sort}))) {}

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
