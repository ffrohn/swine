#include "rewriter.h"

Rewriter::Rewriter(Util &util): util(util) {}

Term Rewriter::rewrite(Term t) {
    if (util.config.semantics == Partial) {
        return t;
    }
    if (!util.is_app(t)) {
        return t;
    } else {
        TermVec children;
        for (const auto &c: t) {
            children.push_back(rewrite(c));
        }
        if (util.is_abstract_exp(t)) {
            const auto base {children.at(1)};
            if (util.is_abstract_exp(base)) {
                const auto [inner_base, inner_exp] {util.decompose_exp(base)};
                return util.solver->make_term(
                    Apply,
                    util.exp,
                    inner_base,
                    util.solver->make_term(Mult, children.at(2), inner_exp));
            }
        } else if (t->get_op().prim_op == Negate) {
            if (util.is_app(children.at(0)) && children.at(0)->get_op().prim_op == Negate) {
                return *children.at(0)->begin();
            }
        } else if (t->get_op().prim_op == Mult) {
            std::unordered_map<Term, UnorderedTermSet> exp_map;
            TermVec new_children;
            for (const auto &c: children) {
                if (util.is_abstract_exp(c)) {
                    const auto [base, exp] {util.decompose_exp(c)};
                    auto &exp_set {exp_map.emplace(exp, UnorderedTermSet()).first->second};
                    exp_set.insert(c);
                } else {
                    new_children.push_back(c);
                }
            }
            bool changed {false};
            for (const auto &[exp,set]: exp_map) {
                if (set.size() > 1) {
                    changed = true;
                    TermVec bases;
                    for (const auto &e: set) {
                        bases.push_back(*(++(++e->begin())));
                    }
                    new_children.push_back(
                        util.solver->make_term(
                            Apply,
                            util.exp,
                            util.solver->make_term(Mult, bases),
                            exp));
                } else {
                    new_children.push_back(*set.begin());
                }
            }
            if (changed) {
                return util.solver->make_term(Mult, new_children);
            }
        }
        return util.solver->make_term(t->get_op(), children);
    }
}
