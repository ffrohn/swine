#include "term_evaluator.h"

#include <boost/algorithm/string.hpp>

TermEvaluator::TermEvaluator(const Util &util): util(util) {}

Term TermEvaluator::evaluate(Term expression) const {
    if (!util.is_app(expression)) {
        return util.solver->get_value(expression);
    } else if (expression->get_op() == Apply && *expression->begin() == util.exp) {
        auto it {expression->begin()};
        const auto fst {util.value(evaluate(*(++it)))};
        const auto snd {Util::to_int(evaluate(*(++it)))};
        if (snd >= 0 || util.config.semantics == Semantics::Total) {
            return util.term(pow(fst, abs(snd)));
        } else {
            return util.solver->get_value(expression);
        }
    } else {
        TermVec children;
        for (const auto &c: expression) {
            children.push_back(evaluate(c));
        }
        return util.solver->get_value(util.solver->make_term(expression->get_op(), children));
    }
}
