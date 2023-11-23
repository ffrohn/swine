#include "term_evaluator.h"

#include <boost/algorithm/string.hpp>

TermEvaluator::TermEvaluator(Util &util): util(util) {}

Term TermEvaluator::evaluate(Term expression) const {
    if (!util.is_app(expression)) {
        return util.solver->get_value(expression);
    } else if (expression->get_op() == Op(Apply) && *expression->begin() == util.exp) {
        auto it {expression->begin()};
        const auto fst {util.value(evaluate(*(++it)))};
        const auto snd {stol(util.value(evaluate(*(++it))).str())};
        if (snd >= 0 || util.config.semantics == Total) {
            return util.term(pow(fst, abs(snd)));
        } else {
            return util.solver->get_value(expression);
        }
    } else {
        TermVec children;
        for (const auto &c: expression) {
            children.push_back(evaluate(c));
        }
        return util.solver->make_term(expression->get_op(), children);
    }
}
