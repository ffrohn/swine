#include "preprocessor.h"

Preprocessor::Preprocessor(Util &util): util(util), rewriter(util), constant_propagator(util) {}

Term Preprocessor::exp_to_pow(Term term) {
    if (!util.is_app(term)) {
        return term;
    }
    TermVec children;
    for (const auto &c: term) {
        children.push_back(exp_to_pow(c));
    }
    if (util.is_abstract_exp(term)) {
        const auto exp {children.at(2)};
        if (exp->is_value()) {
            const auto val {util.value(exp)};
            if (val == 0) {
                return util.term(1);
            } else if (val == 1 || (val == -1 && util.config.semantics == Total)) {
                return children.at(1);
            } else if (val > 1 || util.config.semantics == Total) {
                return util.solver->make_term(Pow, children.at(1), util.term(abs(util.value(exp))));
            }
        }
    }
    return util.solver->make_term(term->get_op(), children);
}

Term Preprocessor::preprocess(Term term) {
    Term cterm {constant_propagator.propagate(term)};
    Term rterm {rewriter.rewrite(cterm)};
    while (term != cterm && cterm != rterm) {
        term = rterm;
        cterm = constant_propagator.propagate(term);
        rterm = term == cterm ? cterm : rewriter.rewrite(cterm);
    }
    return exp_to_pow(rterm);
}
