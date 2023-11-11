#include "term_evaluator.h"

#include <boost/algorithm/string.hpp>

TermEvaluator::TermEvaluator(Util &util): util(util) {}

cpp_int TermEvaluator::evaluate_int(Term expression) const {
    if (expression->is_value()) {
        return util.value(expression);
    } else if (expression->is_symbol()) {
        return evaluate_int(util.solver->get_value(expression));
    } else if (expression->get_op() == Op(Apply) && *expression->begin() == util.exp) {
        auto it {expression->begin()};
        const auto fst {evaluate_int(*(++it))};
        const auto snd {stol(evaluate_int(*(++it)).str())};
        return pow(fst, snd);
    } else if (expression->get_op() == Op(Pow)) {
        auto it {expression->begin()};
        const auto fst {evaluate_int(*it)};
        const auto snd {stol(evaluate_int(*(++it)).str())};
        return pow(fst, snd);
    } else if (expression->get_op() == Op(Negate)) {
        return -evaluate_int(*expression->begin());
    } else {
        const auto eval = [&](const std::function<cpp_int(cpp_int&, cpp_int&)> &fun, const cpp_int neutral) {
            auto cur {neutral};
            for (auto it = expression->begin(); it != expression->end(); ++it) {
                auto next {evaluate_int(*it)};
                cur = fun(cur, next);
            }
            return cur;
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

bool TermEvaluator::evaluate_bool(Term expression) const {
    if (expression->is_value()) {
        return boost::iequals(expression->to_string(), "true");
    } else if (expression->is_symbol()) {
        return evaluate_bool(util.solver->get_value(expression));
    } else if (expression->get_op() == Op(And)) {
        for (const auto e: expression) {
            if (!evaluate_bool(e)) {
                return false;
            }
        }
        return true;
    } else if (expression->get_op() == Op(Or)) {
        for (const auto e: expression) {
            if (evaluate_bool(e)) {
                return true;
            }
        }
        return false;
    } else if (expression->get_op() == Op(Not)) {
        return !evaluate_bool(*expression->begin());
    } else if (expression->get_op() == Op(Equal) && (*expression->begin())->get_sort() != util.int_sort) {
        auto it {expression->begin()};
        const auto fst {evaluate_bool(*it)};
        while (++it != expression->end()) {
            if (evaluate_bool(*it) != fst) {
                return false;
            }
        }
        return true;
    } else {
        const auto test_comparison = [&](const std::function<bool(cpp_int, cpp_int)> &pred) {
            auto it {expression->begin()};
            auto cur {evaluate_int(*it)};
            while (++it != expression->end()) {
                const auto next {evaluate_int(*it)};
                if (!pred(cur, next)) {
                    return false;
                } else {
                    cur = next;
                }
            }
            return true;
        };
        if (expression->get_op() == Op(Lt)) {
            return test_comparison([](auto x, auto y) {
                return x < y;
            });
        } else if (expression->get_op() == Op(Le)) {
            return test_comparison([](auto x, auto y) {
                return x <= y;
            });
        } else if (expression->get_op() == Op(Gt)) {
            return test_comparison([](auto x, auto y) {
                return x > y;
            });
        } else if (expression->get_op() == Op(Ge)) {
            return test_comparison([](auto x, auto y) {
                return x >= y;
            });
        } else if (expression->get_op() == Op(Equal)) {
            return test_comparison([](auto x, auto y) {
                return x == y;
            });
        } else {
            throw std::invalid_argument("failed to evaluate " + expression->to_string());
        }
    }
}

std::optional<cpp_int> TermEvaluator::evaluate_ground_int(Term expression) const {
    if (expression->is_value()) {
        return util.value(expression);
    } else if (expression->is_symbol()) {
        return {};
    } else if (expression->get_op() == Op(Apply) && *expression->begin() == util.exp) {
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
