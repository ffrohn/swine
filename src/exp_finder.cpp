#include "exp_finder.h"

ExpGroup::ExpGroup(Term t, Util &util):
    t(t),
    util(util),
    ground_base(util.decompose_exp(t).first->is_value()),
    neg_base(ground_base && util.value(util.decompose_exp(t).first) < 0) {}

TermVec ExpGroup::all() const {
    TermVec res;
    const auto [base, exp] {util.decompose_exp(t)};
    res.push_back(t);
    res.push_back(util.make_exp(base, util.solver->make_term(Negate, exp)));
    if (neg_base || !ground_base) {
        res.push_back(util.make_exp(util.solver->make_term(Negate, base), exp));
        res.push_back(util.make_exp(util.solver->make_term(Negate, base), util.solver->make_term(Negate, exp)));
    }
    return res;
}

TermVec ExpGroup::maybe_non_neg_base() const {
    if (!neg_base) {
        return all();
    }
    TermVec res;
    const auto [base, exp] {util.decompose_exp(t)};
    res.push_back(util.make_exp(util.solver->make_term(Negate, base), exp));
    res.push_back(util.make_exp(util.solver->make_term(Negate, base), util.solver->make_term(Negate, exp)));
    return res;
}

TermVec ExpGroup::sym() const {
    // TODO the implementaion below makes more sense, but for some reasons, more (redundant) symmetry lemmas help in some cases...
    return all();
    //    TermVec res;
    //    res.push_back(t);
    //    if (neg_base || !ground_base) {
    //        const auto [base, exp] {util.decompose_exp(t)};
    //        res.push_back(util.make_exp(util.solver->make_term(Negate, base), util.solver->make_term(Negate, exp)));
    //    }
    //    return res;
}

bool ExpGroup::has_ground_base() const {
    return ground_base;
}

Term ExpGroup::orig() const {
    return t;
}

ExpFinder::ExpFinder(Util &util): util(util) {}

void ExpFinder::find_exps(Term term, UnorderedTermSet &res) {
    if (util.is_app(term)) {
        for (auto c: term) {
            find_exps(c, res);
        }
        if (util.is_abstract_exp(term)) {
            res.emplace(term);
        }
    }
}

std::vector<ExpGroup> ExpFinder::find_exps(Term term) {
    UnorderedTermSet res;
    find_exps(term, res);
    std::vector<ExpGroup> groups;
    for (const auto &t: res) {
        groups.emplace_back(t, util);
    }
    return groups;
}
