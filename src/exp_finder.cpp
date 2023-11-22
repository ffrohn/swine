#include "exp_finder.h"

ExpFinder::ExpFinder(Util &util): util(util) {}

void ExpFinder::find_exps(Term term, UnorderedTermSet &res) {
    if (util.is_app(term)) {
        for (auto c: term) {
            find_exps(c, res);
        }
        if (util.is_abstract_exp(term)) {
            const auto [base, exp] {util.decompose_exp(term)};
            if (base->is_value() && util.value(base) < 0) {
                res.insert(util.make_exp(
                    util.solver->make_term(Negate, base),
                    exp
                    ));
            } else {
                res.insert(term);
            }
        }
    }
}

UnorderedTermSet ExpFinder::find_exps(Term term) {
    UnorderedTermSet res;
    find_exps(term, res);
    return res;
}
