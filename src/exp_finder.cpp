#include "exp_finder.h"

ExpFinder::ExpFinder(Util &util): util(util) {}

void ExpFinder::find_exps(Term term, std::unordered_map<Term, bool> &res) {
    if (util.is_app(term)) {
        for (auto c: term) {
            find_exps(c, res);
        }
        if (util.is_abstract_exp(term)) {
            auto [base, exp] {util.decompose_exp(term)};
            res.emplace(util.make_exp(base, exp), true);
            bool base_is_neg {base->is_value() && util.value(base) < 0};
            const auto neg_base {util.solver->make_term(Negate, base)};
            const auto neg_exp {util.solver->make_term(Negate, exp)};
            if (!base_is_neg) {
                res.emplace(util.make_exp(base, neg_exp), false);
            }
            if (!base->is_value() || base_is_neg) {
                res.emplace(util.make_exp(neg_base, exp), false);
                res.emplace(util.make_exp(neg_base, neg_exp), false);
            }
        }
    }
}

std::unordered_map<Term, bool> ExpFinder::find_exps(Term term) {
    std::unordered_map<Term, bool> res;
    find_exps(term, res);
    return res;
}
