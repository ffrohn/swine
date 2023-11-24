#include "exp_finder.h"

ExpFinder::ExpFinder(Util &util): util(util) {}

void ExpFinder::find_exps(Term term, UnorderedTermSet &res) {
    if (util.is_app(term)) {
        for (auto c: term) {
            find_exps(c, res);
        }
        if (util.is_abstract_exp(term)) {
            auto [base, exp] {util.decompose_exp(term)};
            if (base->is_value() && util.value(base) < 0) {
                base = util.solver->make_term(Negate, base);
            }
            const auto neg_exp {util.solver->make_term(Negate, exp)};
            res.insert(util.make_exp(base, exp));
            res.insert(util.make_exp(base, neg_exp));
            if (!base->is_value()) {
                const auto neg_base {util.solver->make_term(Negate, base)};
                res.insert(util.make_exp(neg_base, exp));
                res.insert(util.make_exp(neg_base, neg_exp));
            }
        }
    }
}

UnorderedTermSet ExpFinder::find_exps(Term term) {
    UnorderedTermSet res;
    find_exps(term, res);
    return res;
}
