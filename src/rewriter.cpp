#include "rewriter.h"

namespace swine {

Rewriter::Rewriter(Util &util): util(util) {}

Term Rewriter::rewrite(Term t) {
    Term res;
    if (!util.is_app(t)) {
        return t;
    } else {
        TermVec children;
        for (const auto &c: t) {
            children.push_back(rewrite(c));
        }
        if (util.is_abstract_exp(t)) {
            const auto base {children.at(1)};
            const auto exp {children.at(2)};
            if ((base == util.term(1) && util.config.semantics == Semantics::Total) || exp == util.term(0)) {
                res = util.term(1);
            } else if (exp == util.term(1) || (exp == util.term(-1) && util.config.semantics == Semantics::Total)) {
                res = base;
            } else if (exp->is_value() && (util.value(exp) >= 0 || util.config.semantics == Semantics::Total)) {
                if (util.config.solver_kind == SolverKind::Z3) {
                    if (util.value(exp) <= util.config.rewrite_threshold) {
                        const auto e {util.to_int(exp)};
                        res = util.term(1);
                        for (auto i = 0; i < e; ++i) {
                            res = util.solver->make_term(Mult, res, base);
                        }
                        return res;
                    }
                } else {
                res = util.solver->make_term(Pow, base, util.term(abs(util.value(exp))));
                }
            } else if (util.is_abstract_exp(base) && util.config.semantics == Semantics::Total) {
                const auto [inner_base, inner_exp] {util.decompose_exp(base)};
                res = util.solver->make_term(
                    Apply,
                    util.exp,
                    inner_base,
                    util.solver->make_term(Mult, children.at(2), inner_exp));
            }
        } else if (t->get_op().prim_op == Negate) {
            if (util.is_app(children.at(0)) && children.at(0)->get_op().prim_op == Negate) {
                res = *children.at(0)->begin();
            }
        } else if (t->get_op().prim_op == Mult && util.config.semantics == Semantics::Total) {
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
                        const auto [base, exp] {util.decompose_exp(e)};
                        bases.push_back(base);
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
                res = util.solver->make_term(Mult, new_children);
            }
        } else if (t->get_op().prim_op == Pow && util.config.semantics == Semantics::Total) {
            const auto fst {children.at(1)};
            const auto snd {children.at(2)};
            if (util.is_abstract_exp(fst) && snd->is_value() && util.value(snd) >= 0) {
                const auto [base, exp] {util.decompose_exp(fst)};
                res = util.make_exp(base, util.solver->make_term(Mult, exp, snd));
            }
        }
        if (!res) {
            res = util.solver->make_term(t->get_op(), children);
        }
        return res;
    }
}

}
