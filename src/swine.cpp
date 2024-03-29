#include "swine.h"

#include "brute_force.h"
#include "preprocessor.h"
#include "exp_finder.h"
#include "term_evaluator.h"
#include "util.h"

#include <assert.h>
#include <limits>

namespace swine {

using namespace smt;
using namespace boost::multiprecision;

std::ostream& operator<<(std::ostream &s, const Swine::EvaluatedExponential &exp) {
    return s <<
           "abstract: exp(" <<
           exp.base <<
           ", " <<
           exp.exponent <<
           "); concrete: exp(" <<
           exp.base_val <<
           ", " <<
           exp.exponent_val <<
           ") = " <<
           exp.exp_expression_val;
}

std::ostream& operator<<(std::ostream &s, const Swine::Statistics &stats) {
    s << "assertions           : " << stats.num_assertions << std::endl;
    s << "iterations           : " << stats.iterations << std::endl;
    s << "symmetry lemmas      : " << stats.symmetry_lemmas << std::endl;
    s << "bounding lemmas      : " << stats.bounding_lemmas << std::endl;
    s << "monotonicity lemmas  : " << stats.monotonicity_lemmas << std::endl;
    s << "modulo lemmas        : " << stats.modulo_lemmas << std::endl;
    s << "interpolation lemmas : " << stats.interpolation_lemmas << std::endl;
    s << "non constant base    : " << (stats.non_constant_base ? "true" : "false") << std::endl;
    return s;
}

Swine::Swine(const SmtSolver solver, const Config &config):
    AbsSmtSolver(solver->get_solver_enum()),
    config(config),
    util(std::make_unique<Util>(solver, this->config)),
    solver(solver) {
    solver->set_opt("produce-models", "true");
    if (config.get_lemmas) {
        solver->set_opt("produce-unsat-assumptions", "true");
    }
    frames.emplace_back();
}

Swine::~Swine(){}

void Swine::set_opt(const std::string option, const std::string value) {
    solver->set_opt(option, value);
}

void Swine::set_logic(const std::string logic) {
    solver->set_logic(logic);
}

void Swine::add_lemma(const Term t, const LemmaKind kind) {
    if (config.log) {
        std::cout << kind << " lemma:" << std::endl;
        std::cout << t << std::endl;
    }
    if (config.validate_unsat || config.get_lemmas) frames.back().lemmas.emplace(t, kind);
    if (config.get_lemmas) {
        static unsigned int count {0};
        const auto assumption {make_symbol("assumption_" + std::to_string(count), solver->make_sort(BOOL))};
        ++count;
        frames.back().assumptions.emplace(assumption, t);
        solver->assert_formula(solver->make_term(Equal, assumption, t));
    } else {
        solver->assert_formula(t);
    }
    switch (kind) {
    case LemmaKind::Interpolation: ++stats.interpolation_lemmas;
        break;
    case LemmaKind::Symmetry: ++stats.symmetry_lemmas;
        break;
    case LemmaKind::Modulo: ++stats.modulo_lemmas;
        break;
    case LemmaKind::Bounding: ++stats.bounding_lemmas;
        break;
    case LemmaKind::Monotonicity: ++stats.monotonicity_lemmas;
        break;
    default: throw std::invalid_argument("unknown lemma kind");
    }
}

void Swine::symmetry_lemmas(std::unordered_map<Term, LemmaKind> &lemmas) {
    if (!config.is_active(LemmaKind::Symmetry)) {
        return;
    }
    TermVec sym_lemmas;
    for (const auto &f: frames) {
        for (const auto &e: f.exps) {
            const auto ee {evaluate_exponential(e)};
            if (ee) {
                if (ee->base_val < 0) {
                    base_symmetry_lemmas(e, sym_lemmas);
                }
                if (ee->exponent_val < 0) {
                    exp_symmetry_lemmas(e, sym_lemmas);
                }
                if (ee->base_val < 0 && ee->exponent_val < 0) {
                    const auto neg {util->make_exp(solver->make_term(Negate, ee->base), solver->make_term(Negate, ee->exponent))};
                    base_symmetry_lemmas(neg, sym_lemmas);
                    exp_symmetry_lemmas(neg, sym_lemmas);
                }
            }
        }
    }
    for (const auto &l: sym_lemmas) {
        lemmas.emplace(l, LemmaKind::Symmetry);
    }
}

void Swine::base_symmetry_lemmas(const Term e, TermVec &lemmas) {
    if (!config.is_active(LemmaKind::Symmetry)) {
        return;
    }
    const auto [base, exp] {util->decompose_exp(e)};
    if (!base->is_value() || util->value(base) < 0) {
        const auto conclusion_even {make_term(
            Equal,
            e,
            util->make_exp(
                make_term(Negate, base),
                exp))};
        const auto conclusion_odd {make_term(
            Equal,
            e,
            make_term(
                Negate,
                util->make_exp(
                    make_term(Negate, base),
                    exp)))};
        auto premise_even {make_term(
            Equal,
            make_term(Mod, exp, util->term(2)),
            util->term(0))};
        auto premise_odd {make_term(
            Equal,
            make_term(Mod, exp, util->term(2)),
            util->term(1))};
        if (config.semantics == Semantics::Partial) {
            premise_even = make_term(
                And,
                premise_even,
                make_term(Ge, exp, util->term(0)));
            premise_odd = make_term(
                And,
                premise_odd,
                make_term(Gt, exp, util->term(0)));
        }
        lemmas.push_back(make_term(Implies, premise_even, conclusion_even));
        lemmas.push_back(make_term(Implies, premise_odd, conclusion_odd));
    }
}

void Swine::exp_symmetry_lemmas(const Term e, TermVec &lemmas) {
    if (!config.is_active(LemmaKind::Symmetry)) {
        return;
    }
    if (config.semantics == Semantics::Total) {
        const auto [base, exp] {util->decompose_exp(e)};
        const auto lemma {make_term(
            Equal,
            e,
            util->make_exp(base, make_term(Negate, exp)))};
        lemmas.push_back(lemma);
    }
}

void Swine::compute_bounding_lemmas(const ExpGroup &g) {
    if (!config.is_active(LemmaKind::Bounding)) {
        return;
    }
    for (const auto &e: g.maybe_non_neg_base()) {
        auto [it, inserted] {frames.back().bounding_lemmas.emplace(e, TermVec())};
        if (!inserted) {
            return;
        }
        auto &set {it->second};
        const auto [base, exp] {util->decompose_exp(e)};
        Term lemma;
        // exp = 0 ==> base^exp = 1
        lemma = make_term(
            Implies,
            make_term(Equal, exp, util->term(0)),
            make_term(Equal, e, util->term(1)));
        set.push_back(lemma);
        // exp = 1 ==> base^exp = base
        lemma = make_term(
            Implies,
            make_term(Equal, exp, util->term(1)),
            make_term(Equal, e, base));
        set.push_back(lemma);
        if (!base->is_value() || util->value(base) == 0) {
            // base = 0 && ... ==> base^exp = 0
            const auto conclusion {make_term(Equal, e, util->term(0))};
            TermVec premises {make_term(Equal, base, util->term(0))};
            PrimOp op;
            if (config.semantics == Semantics::Total) {
                premises.push_back(make_term(Distinct, exp, util->term(0)));
                op = Equal;
            } else {
                op = Implies;
                premises.push_back(make_term(Gt, exp, util->term(0)));
            }
            lemma = make_term(
                op,
                make_term(And, premises),
                conclusion);
            set.push_back(lemma);
        }
        if (!base->is_value() || util->value(base) == 1) {
            // base = 1 && ... ==> base^exp = 1
            const auto conclusion {make_term(Equal, e, util->term(1))};
            Term premise {make_term(Equal, base, util->term(1))};
            if (config.semantics == Semantics::Partial) {
                premise = make_term(
                    And,
                    premise,
                    make_term(Ge, exp, util->term(0)));
            }
            lemma = make_term(Implies, premise, conclusion);
            set.push_back(lemma);
        }
        if (!base->is_value() || util->value(base) > 1) {
            // exp + base > 4 && s > 1 && t > 1 ==> base^exp > s * t + 1
            lemma = make_term(
                Implies,
                make_term(
                    And,
                    make_term(
                        Gt,
                        make_term(Plus, base, exp),
                        util->term(4)),
                    make_term(Gt, base, util->term(1)),
                    make_term(Gt, exp, util->term(1))),
                make_term(
                    Gt,
                    e,
                    make_term(
                        Plus,
                        make_term(
                            Mult,
                            base,
                            exp),
                        util->term(1))));
            set.push_back(lemma);
        }
    }
}

void Swine::bounding_lemmas(std::unordered_map<Term, LemmaKind> &lemmas) {
    if (!config.is_active(LemmaKind::Bounding)) {
        return;
    }
    UnorderedTermSet seen;
    for (const auto &f: frames) {
        for (const auto &g: f.exp_groups) {
            if (seen.contains(g->orig())) {
                continue;
            }
            seen.emplace(g->orig());
            for (const auto &e: g->maybe_non_neg_base()) {
                const auto ee {evaluate_exponential(e)};
                if (ee && ee->exp_expression_val != ee->expected_val && ee->base_val >= 0) {
                    for (const auto &l: f.bounding_lemmas.at(e)) {
                        lemmas.emplace(l, LemmaKind::Bounding);
                    }
                }
            }
        }
    }
}

void Swine::assert_formula(const Term & t) {
    static Preprocessor preproc(*util);
    static ExpFinder exp_finder(*util);
    try {
        ++stats.num_assertions;
        if (config.log) {
            std::cout << "assertion:" << std::endl;
            std::cout << t << std::endl;
        }
        const auto preprocessed {preproc.preprocess(t)};
        if (config.validate_sat || config.validate_unsat || config.get_lemmas) {
            frames.back().preprocessed_assertions.emplace(preprocessed, t);
        }
        solver->assert_formula(preprocessed);
        for (const auto &g: exp_finder.find_exps(preprocessed)) {
            if (frames.back().exps.insert(g.orig()).second) {
                frames.back().exp_groups.emplace_back(std::make_shared<ExpGroup>(g));
                stats.non_constant_base |= !g.has_ground_base();
                compute_bounding_lemmas(g);
            }
        }
    } catch (const ExponentOverflow &e) {
        frames.back().assert_failed = true;
    }
}

std::optional<Swine::EvaluatedExponential> Swine::evaluate_exponential(const Term exp_expression) const {
    EvaluatedExponential res;
    res.exp_expression = exp_expression;
    res.exp_expression_val = util->value(solver->get_value(exp_expression));
    auto it {exp_expression->begin()};
    ++it;
    res.base = *it;
    res.base_val = util->value(solver->get_value(*it));
    ++it;
    res.exponent = *it;
    res.exponent_val = Util::to_int(solver->get_value(*it));
    if (util->config.semantics == Semantics::Partial && res.exponent_val < 0) {
        return {};
    }
    res.expected_val = boost::multiprecision::pow(res.base_val, abs(res.exponent_val));
    return res;
}

Swine::Interpolant Swine::interpolate(Term t, const unsigned pos, const cpp_int x1, const cpp_int x2) {
    Interpolant res;
    TermVec children;
    for (const auto &c: t) {
        children.push_back(c);
    }
    auto x {children[pos]};
    children[pos] = util->term(x1);
    const auto at_x1 {make_term(t->get_op(), children)};
    res.factor = abs(x2 - x1);
    if (res.factor == 0) {
        res.factor = 1;
        res.t = at_x1;
    } else {
        children[pos] = util->term(x2);
        const auto at_x2 {make_term(t->get_op(), children)};
        res.t = make_term(
            Plus,
            make_term(
                Mult,
                util->term(res.factor),
                at_x1),
            make_term(
                Mult,
                make_term(Minus, at_x2, at_x1),
                make_term(Minus, x, util->term(x1))));
    }
    return res;
}

Term Swine::interpolation_lemma(Term t, const bool upper, const std::pair<cpp_int, long long> a, const std::pair<cpp_int, long long> b) {
    const auto x1 {min(a.first, b.first)};
    const auto x2 {max(a.first, b.first)};
    const auto y1 {std::min(a.second, b.second)};
    const auto y2 {std::max(a.second, b.second)};
    const auto [base, exp] {util->decompose_exp(t)};
    const auto op = upper ? Le : Ge;
    // y1 <= exponent <= y2
    const auto exponent_in_bounds {make_term(Le, util->term(y1), exp, util->term(y2))};
    // exponent > 0
    const auto exponent_positive {make_term(Gt, exp, util->term(0))};
    if (base->is_value()) {
        const auto i {interpolate(t, 2, y1, y2)};
        const auto premise = upper ? exponent_in_bounds : exponent_positive;
        return make_term(
            Implies,
            premise,
            make_term(
                op,
                make_term(Mult, t, util->term(i.factor)),
                i.t));
    } else {
        const auto at_y1 {util->make_exp(base, util->term(y1))};
        const auto at_y2 {util->make_exp(base, util->term(y2))};
        const auto i1 {interpolate(at_y1, 1, x1, x2)};
        const auto i2 {interpolate(at_y2, 1, x1, x2)};
        Term premise;
        if (upper) {
            // x1 <= base <= x2
            const auto base_in_bounds {make_term(Le, util->term(x1), base, util->term(x2))};
            premise = make_term(And, base_in_bounds, exponent_in_bounds);
        } else {
            // exponent >= y1
            const auto exponent_above_threshold {make_term(Ge, exp, util->term(y1))};
            // base > 0
            const auto base_positive {make_term(Gt, base, util->term(0))};
            premise = make_term(And, base_positive, exponent_above_threshold);
        }
        Term conclusion;
        if (y2 == y1) {
            conclusion = make_term(
                op,
                make_term(Mult, t, util->term(i1.factor)),
                i1.t);
        } else {
            const auto y_diff {util->term(y2 - y1)};
            conclusion = make_term(
                op,
                make_term(Mult, t, util->term(i1.factor), y_diff),
                make_term(
                    Plus,
                    make_term(Mult, i1.t, y_diff),
                    make_term(
                        Mult,
                        make_term(Minus, i2.t, i1.t),
                        make_term(Minus, exp, util->term(y1)))));
        }
        return make_term(Implies, premise, conclusion);
    }
}

void Swine::interpolation_lemma(const EvaluatedExponential &e, std::unordered_map<Term, LemmaKind> &lemmas) {
    Term lemma;
    auto &vec {interpolation_points.emplace(e.exp_expression, std::vector<std::pair<cpp_int, long long>>()).first->second};
    if (e.exp_expression_val < e.expected_val) {
        const auto min_base = e.base_val > 1 ? e.base_val - 1 : e.base_val;
        const auto min_exp = e.exponent_val > 1 ? e.exponent_val - 1 : e.exponent_val;
        lemma = interpolation_lemma(e.exp_expression, false, {min_base, min_exp}, {min_base + 1, min_exp + 1});
    } else {
        std::pair<cpp_int, long long> nearest;
        if (vec.empty()) {
            nearest = {e.base_val, e.exponent_val};
        } else {
            nearest = vec.front();
            auto min_dist {e.base_val * e.base_val + e.exponent_val * e.exponent_val};
            for (const auto &[x, y]: vec) {
                const auto x_dist {x - e.base_val};
                const auto y_dist {y - e.exponent_val};
                const auto dist {x_dist * x_dist + y_dist * y_dist};
                if (0 < dist && dist <= min_dist) {
                    nearest = {x, y};
                }
            }
        }
        lemma = interpolation_lemma(e.exp_expression, true, {e.base_val, e.exponent_val}, nearest);
    }
    vec.emplace_back(e.base_val, e.exponent_val);
    lemmas.emplace(lemma, LemmaKind::Interpolation);
}

void Swine::interpolation_lemmas(std::unordered_map<Term, LemmaKind> &lemmas) {
    if (!config.is_active(LemmaKind::Interpolation)) {
        return;
    }
    for (const auto &f: frames) {
        for (const auto &g: f.exp_groups) {
            for (const auto &e: g->maybe_non_neg_base()) {
                const auto ee {evaluate_exponential(e)};
                if (ee && ee->exp_expression_val != ee->expected_val && ee->base_val > 0 && ee->exponent_val > 0) {
                    interpolation_lemma(*ee, lemmas);
                }
            }
        }
    }
}

std::optional<Term> Swine::monotonicity_lemma(const EvaluatedExponential &e1, const EvaluatedExponential &e2) {
    if ((e1.base_val > e2.base_val && e1.exponent_val < e2.exponent_val)
        || (e1.base_val < e2.base_val && e1.exponent_val > e2.exponent_val)
        || (e1.base_val == e2.base_val && e1.exponent_val == e2.exponent_val)) {
        return {};
    }
    bool is_smaller = e1.base_val < e2.base_val || e1.exponent_val < e2.exponent_val;
    const auto [smaller, greater] = is_smaller ? std::pair(e1, e2) : std::pair(e2, e1);
    if (smaller.exp_expression_val < greater.exp_expression_val) {
        return {};
    }
    Term premise;
    const Term strict_exp_premise {make_term(
        Lt,
        smaller.exponent,
        greater.exponent)};
    const Term non_strict_exp_premise {make_term(
        Le,
        smaller.exponent,
        greater.exponent)};
    if (!smaller.base->is_value() || !greater.base->is_value()) {
        const Term strict_base_premise {make_term(
            Lt,
            smaller.base,
            greater.base)};
        const Term non_strict_base_premise {make_term(
            Le,
            smaller.base,
            greater.base)};
        premise =
            make_term(
                And,
                non_strict_base_premise,
                non_strict_exp_premise,
                make_term(
                    Or,
                    strict_base_premise,
                    strict_exp_premise));
    } else if (smaller.base_val < greater.base_val) {
        premise = non_strict_exp_premise;
    } else {
        premise = strict_exp_premise;
    }
    return make_term(
        Implies,
        make_term(
            And,
            make_term(Lt, util->term(1), smaller.base),
            make_term(
                Lt,
                util->term(0),
                smaller.exponent),
            premise),
        make_term(Lt, smaller.exp_expression, greater.exp_expression));
}

void Swine::monotonicity_lemmas(std::unordered_map<Term, LemmaKind> &lemmas) {
    if (!config.is_active(LemmaKind::Monotonicity)) {
        return;
    }
    // search for pairs exp(b,e1), exp(b,e2) whose models violate monotonicity of exp
    TermVec exps;
    for (const auto &f: frames) {
        for (const auto &g: f.exp_groups) {
            for (const auto &e: g->maybe_non_neg_base()) {
                const auto [base, exp] {util->decompose_exp(e)};
                if (util->value(solver->get_value(base)) > 1 && util->value(solver->get_value(exp)) > 0) {
                    exps.push_back(e);
                }
            }
        }
    }
    for (auto it1 = exps.begin(); it1 != exps.end(); ++it1) {
        const auto e1 {evaluate_exponential(*it1)};
        if (e1) {
            for (auto it2 = it1; ++it2 != exps.end();) {
                const auto e2 {evaluate_exponential(*it2)};
                if (e2) {
                    const auto mon_lemma {monotonicity_lemma(*e1, *e2)};
                    if (mon_lemma) {
                        lemmas.emplace(*mon_lemma, LemmaKind::Monotonicity);
                    }
                }
            }
        }
    }
}

void Swine::mod_lemmas(std::unordered_map<Term, LemmaKind> &lemmas) {
    if (!config.is_active(LemmaKind::Modulo)) {
        return;
    }
    for (auto f: frames) {
        for (auto e: f.exps) {
            const auto ee {evaluate_exponential(e)};
            if (ee && ee->exponent_val > 0 && ee->exp_expression_val % abs(ee->base_val) != 0) {
                auto l {make_term(
                    Equal,
                    util->term(0),
                    make_term(Mod, ee->exp_expression, ee->base))};
                if (config.semantics == Semantics::Partial) {
                    l = make_term(
                        Implies,
                        make_term(Gt, ee->exponent, util->term(0)),
                        l);
                } else {
                    l = make_term(
                        Implies,
                        make_term(Distinct, ee->exponent, util->term(0)),
                        l);
                }
                lemmas.emplace(l, LemmaKind::Modulo);
            }
        }
    }
}

std::unordered_map<smt::Term, LemmaKind> Swine::preprocess_lemmas(const std::unordered_map<smt::Term, LemmaKind> &lemmas) {
    static Preprocessor preproc(*util);
    std::unordered_map<smt::Term, LemmaKind> res;
    for (const auto &[l,k]: lemmas) {
        const auto p {preproc.preprocess(l)};
        if (solver->get_value(p) == util->False) {
            res.emplace(p, k);
        }
    }
    return res;
}

Result Swine::check_sat(TermVec assumptions) {
    Result res;
    for (const auto &f: frames) {
        if (f.assert_failed) {
            res = Result(ResultType::UNKNOWN, "assert failed");
            return res;
        }
    }
    while (true) {
        try {
            ++stats.iterations;
            if (config.get_lemmas) {
                for (const auto &f: frames) {
                    for (const auto &[a,_]: f.assumptions) {
                        assumptions.push_back(a);
                    }
                }
            }
            if (!assumptions.empty()) {
                res = solver->check_sat_assuming(assumptions);
            } else {
                res = solver->check_sat();
            }
            if (res.is_unsat()) {
                if (config.get_lemmas) {
                    UnorderedTermSet core;
                    solver->get_unsat_assumptions(core);
                    std::cout << "===== lemmas =====" << std::endl;
                    for (const auto &k: lemma_kind::values) {
                        auto first {true};
                        for (const auto &f: frames) {
                            for (const auto &[a,l]: f.assumptions) {
                                if (core.contains(a) && f.lemmas.at(l) == k) {
                                    if (first) {
                                        std::cout << "----- " << k << " lemmas -----" << std::endl;
                                        first = false;
                                    }
                                    std::cout << l << std::endl;
                                }
                            }
                        }
                    }
                }
                for (const auto &f: frames) {
                    if (f.has_overflow) {
                        res = Result(ResultType::UNKNOWN, "overflow");
                        break;
                    }
                }
                if (config.validate_unsat) {
                    brute_force();
                }
                break;
            } else if (res.is_unknown()) {
                break;
            } else if (res.is_sat()) {
                bool sat {true};
                if (config.log) {
                    std::cout << "candidate model:" << std::endl;
                    for (const auto &[key,val]: get_model()) {
                        std::cout << key << " = " << val << std::endl;
                    }
                }
                std::unordered_map<Term, LemmaKind> lemmas;
                // check if the model can be lifted
                for (const auto &f: frames) {
                    for (const auto &e: f.exps) {
                        const auto ee {evaluate_exponential(e)};
                        if (ee && ee->exp_expression_val != ee->expected_val) {
                            sat = false;
                            break;
                        }
                    }
                    if (!sat) {
                        break;
                    }
                }
                if (sat) {
                    if (config.validate_sat) {
                        verify();
                    }
                    break;
                }
                symmetry_lemmas(lemmas);
                lemmas = preprocess_lemmas(lemmas);
                if (lemmas.empty()) {
                    bounding_lemmas(lemmas);
                    lemmas = preprocess_lemmas(lemmas);
                }
                if (lemmas.empty()) {
                    monotonicity_lemmas(lemmas);
                    lemmas = preprocess_lemmas(lemmas);
                }
                if (lemmas.empty()) {
                    mod_lemmas(lemmas);
                    lemmas = preprocess_lemmas(lemmas);
                }
                if (lemmas.empty()) {
                    interpolation_lemmas(lemmas);
                    lemmas = preprocess_lemmas(lemmas);
                }
                if (lemmas.empty()) {
                    if (config.is_active(LemmaKind::Interpolation)) {
                        throw std::logic_error("refinement failed, but interpolation is enabled");
                    } else {
                        return Result(ResultType::UNKNOWN, "Refinement failed, please activate interpolation lemmas.");
                    }
                }
                for (const auto &[l, kind]: lemmas) {
                    add_lemma(l, kind);
                }
            }
        } catch (const ExponentOverflow &e) {
            frames.back().has_overflow = true;
            solver->assert_formula(solver->make_term(Ge, e.get_t(), util->term(std::numeric_limits<long long>::min())));
            solver->assert_formula(solver->make_term(Le, e.get_t(), util->term(std::numeric_limits<long long>::max())));
        }
    }
    if (config.statistics) {
        std::cout << stats << std::endl;
    }
    return res;
}

Result Swine::check_sat() {
    return check_sat({});
}

Result Swine::check_sat_assuming(const TermVec & assumptions) {
    return check_sat(assumptions);
}

Result Swine::check_sat_assuming_list(const TermList & assumptions) {
    return check_sat(TermVec(assumptions.begin(), assumptions.end()));
}

Result Swine::check_sat_assuming_set(const UnorderedTermSet & assumptions) {
    return check_sat(TermVec(assumptions.begin(), assumptions.end()));
}

void Swine::push(uint64_t num) {
    solver->push(num);
    for (unsigned i = 0; i < num; ++i) {
        frames.emplace_back();
    }
}

void Swine::pop(uint64_t num) {
    solver->pop(num);
    for (unsigned i = 0; i < num; ++i) {
        frames.pop_back();
    }
}

uint64_t Swine::get_context_level() const {
    return solver->get_context_level();
}

Term Swine::get_value(const Term & t) const {
    const static TermEvaluator eval(*util);
    return eval.evaluate(t);
}

UnorderedTermMap Swine::get_model() const {
    UnorderedTermMap res;
    bool uf {false};
    UnorderedTermSet assumptions;
    if (config.get_lemmas) {
        for (const auto &f: frames) {
            for (const auto &[a,_]: f.assumptions) {
                assumptions.emplace(a);
            }
        }
    }
    for (const auto &x: symbols) {
        if (assumptions.contains(x)) {
            continue;
        }
        if (!uf && x->get_sort()->get_sort_kind() == SortKind::FUNCTION) {
            uf = true;
            std::cerr << "get_model does not support uninterpreted functions at the moment" << std::endl;
        } else {
            res.emplace(x, solver->get_value(x));
        }
    }
    if (config.debug) {
        for (const auto &f: frames) {
            for (const auto &g: f.exp_groups) {
                for (const auto &e: g->all()) {
                    res.emplace(e, solver->get_value(e));
                }
            }
        }
    }
    return res;
}

UnorderedTermMap Swine::get_array_values(const Term & arr,
                                         Term & out_const_base) const {
    return solver->get_array_values(arr, out_const_base);
}

void Swine::get_unsat_assumptions(UnorderedTermSet & out) {
    solver->get_unsat_assumptions(out);
    if (config.get_lemmas) {
        for (const auto &f: frames) {
            for (const auto &[a,_]: f.assumptions) {
                out.erase(a);
            }
        }
    }
}

Sort Swine::make_sort(const std::string name, uint64_t arity) const {
    return solver->make_sort(name, arity);
}

Sort Swine::make_sort(SortKind sk) const {
    return solver->make_sort(sk);
}

Sort Swine::make_sort(SortKind sk, uint64_t size) const {
    return solver->make_sort(sk, size);
}

Sort Swine::make_sort(SortKind sk, const Sort & sort1) const {
    return solver->make_sort(sk, sort1);
}

Sort Swine::make_sort(SortKind sk,
                      const Sort & sort1,
                      const Sort & sort2) const {
    return solver->make_sort(sk, sort1, sort2);
}

Sort Swine::make_sort(SortKind sk,
                      const Sort & sort1,
                      const Sort & sort2,
                      const Sort & sort3) const {
    return solver->make_sort(sk, sort1, sort2, sort3);
}

Sort Swine::make_sort(SortKind sk, const SortVec & sorts) const {
    return solver->make_sort(sk, sorts);
}

Sort Swine::make_sort(const Sort & sort_con, const SortVec & sorts) const {
    return solver->make_sort(sort_con, sorts);
}

Sort Swine::make_sort(const DatatypeDecl & d) const {
    return solver->make_sort(d);
}

DatatypeDecl Swine::make_datatype_decl(const std::string & s) {
    return solver->make_datatype_decl(s);
}

DatatypeConstructorDecl Swine::make_datatype_constructor_decl(
        const std::string s) {
    return solver->make_datatype_constructor_decl(s);
}

void Swine::add_constructor(DatatypeDecl & dt,
                            const DatatypeConstructorDecl & con) const {
    solver->add_constructor(dt, con);
}

void Swine::add_selector(DatatypeConstructorDecl & dt,
                         const std::string & name,
                         const Sort & s) const {
    solver->add_selector(dt, name, s);
}

void Swine::add_selector_self(DatatypeConstructorDecl & dt,
                              const std::string & name) const {
    solver->add_selector_self(dt, name);
}

Term Swine::get_constructor(const Sort & s, std::string name) const {
    return solver->get_constructor(s, name);
}

Term Swine::get_tester(const Sort & s, std::string name) const {
    return solver->get_tester(s, name);
}

Term Swine::get_selector(const Sort & s,
                         std::string con,
                         std::string name) const {
    return solver->get_selector(s, con, name);
}

Term Swine::make_term(bool b) const {
    return solver->make_term(b);
}

Term Swine::make_term(int64_t i, const Sort & sort) const {
    return solver->make_term(i, sort);
}

Term Swine::make_term(const std::string val,
                      const Sort & sort,
                      uint64_t base) const {
    return solver->make_term(val, sort, base);
}

Term Swine::make_term(const Term & val, const Sort & sort) const {
    return solver->make_term(val, sort);
}

Term Swine::make_symbol(const std::string name, const Sort & sort) {
    const auto res {solver->make_symbol(name, sort)};
    symbols.insert(res);
    return res;
}

Term Swine::get_symbol(const std::string & name) {
    return solver->get_symbol(name);
}

Term Swine::make_param(const std::string name, const Sort & sort) {
    return solver->make_param(name, sort);
}

Term Swine::make_term(Op op, const Term & t) const {
    return solver->make_term(op, t);
}

Term Swine::make_term(Op op, const Term & t0, const Term & t1) const {
    return make_term(op, {t0, t1});
}

Term Swine::make_term(Op op,
                      const Term & t0,
                      const Term & t1,
                      const Term & t2) const {
    return make_term(op, {t0, t1, t2});
}

Term Swine::make_term(Op op, const TermVec & terms) const {
    if (op == Exp) {
        return solver->make_term(Apply, util->exp, terms.front(), *(++terms.begin()));
    } else if (config.solver_kind == SolverKind::Z3 && terms.size() > 2) {
        switch (op.prim_op) {
        case Le:
        case Lt:
        case Ge:
        case Gt: {
                auto it {terms.begin()};
                auto last = *it;
                TermVec args;
                for (++it; it != terms.end(); ++it) {
                    args.push_back(solver->make_term(op, last, *it));
                    last = *it;
                }
                return solver->make_term(And, args);
            }
        default: break;
        }
    }
    return solver->make_term(op, terms);
}

void Swine::reset() {
    solver->reset();
    frames.clear();
    frames.emplace_back();
}

void Swine::reset_assertions() {
    solver->reset_assertions();
    frames.clear();
    frames.emplace_back();
}

Term Swine::substitute(const Term term,
                       const UnorderedTermMap & substitution_map) const {
    return solver->substitute(term, substitution_map);
}

// TODO
void Swine::dump_smt2(std::string filename) const {
    solver->dump_smt2(filename);
}

void Swine::verify() const {
    for (const auto &f: frames) {
        for (const auto &[_,a]: f.preprocessed_assertions) {
            if (get_value(a) != util->True) {
                std::cout << "Validation of the following assertion failed:" << std::endl;
                std::cout << a << std::endl;
                std::cout << "model:" << std::endl;
                for (const auto &[key, value]: get_model()) {
                    std::cout << key << " = " << value << std::endl;
                }
                return;
            }
        }
    }
}

void Swine::brute_force() {
    TermVec assertions;
    for (const auto &f: frames) {
        for (const auto &[a,_]: f.preprocessed_assertions) {
            assertions.push_back(a);
        }
    }
    TermVec exps;
    for (const auto &f: frames) {
        for (const auto &e: f.exps) {
            exps.push_back(e);
        }
    }
    BruteForce bf(*util, assertions, exps);
    if (bf.check_sat()) {
        std::cout << "sat via brute force" << std::endl;
        if (config.log) {
            std::cout << "candidate model:" << std::endl;
            for (const auto &[key,val]: get_model()) {
                std::cout << key << " = " << val << std::endl;
            }
        }
        for (const auto &f: frames) {
            for (const auto &[l, kind]: f.lemmas) {
                if (solver->get_value(l) != util->True) {
                    std::cout << "violated " << kind << " lemma" << std::endl;
                    std::cout << l << std::endl;
                }
            }
        }
        verify();
    }
}

}
