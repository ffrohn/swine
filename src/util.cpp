#include "util.h"

Util::Util(SmtSolver solver):
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

std::optional<cpp_int> Util::evaluate_ground_int(Term expression) const {
    if (expression->is_value()) {
        return value(expression);
    } else if (expression->is_symbol()) {
        return {};
    } else if (expression->get_op() == Op(Apply) && *expression->begin() == exp) {
        auto it {expression->begin()};
        const auto fst {evaluate_ground_int(*(++it))};
        if (!fst) {
            return {};
        }
        const auto snd {evaluate_ground_int(*(++it))};
        if (!snd) {
            return {};
        }
        return pow(*fst, stol(snd->str()));
    } else if (expression->get_op() == Op(Pow)) {
        auto it {expression->begin()};
        const auto fst {evaluate_ground_int(*it)};
        if (!fst) {
            return {};
        }
        const auto snd {evaluate_ground_int(*(++it))};
        if (!snd) {
            return {};
        }
        return pow(*fst, stol(snd->str()));
    } else if (expression->get_op() == Op(Negate)) {
        const auto arg {evaluate_ground_int(*expression->begin())};
        if (!arg) {
            return {};
        }
        return -(*arg);
    } else {
        const auto eval = [&](const std::function<cpp_int(cpp_int&, cpp_int&)> &fun, const cpp_int neutral) {
            auto cur {neutral};
            for (auto it = expression->begin(); it != expression->end(); ++it) {
                auto next {evaluate_ground_int(*it)};
                if (!next) {
                    return std::optional<cpp_int>();
                }
                cur = fun(cur, *next);
            }
            return std::optional<cpp_int>(cur);
        };
        if (expression->get_op() == Op(Plus)) {
            return eval([&](auto &x, auto &y) {
                return x + y;
            }, 0);
        } else if (expression->get_op() == Op(Mult)) {
            return eval([&](auto &x, auto &y) {
                return x * y;
            }, 1);
        } else if (expression->get_op() == Op(Minus)) {
            return eval([&](auto &x, auto &y) {
                return x - y;
            }, 0);
        } else {
            std::cout << expression->get_op() << std::endl;
            throw std::invalid_argument("failed to evaluate " + expression->to_string());
        }
    }
}

Term Util::term(const cpp_int &value) const {
    return solver->make_term(value.str(), int_sort);
}
