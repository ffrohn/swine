#include "swine.h"

#include "brute_force.h"

#include <assert.h>

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
    s << "assertions:           " << stats.num_assertions << std::endl;
    s << "iterations:           " << stats.iterations << std::endl;
    s << "symmetry lemmas:      " << stats.symmetry_lemmas << std::endl;
    s << "modulo lemmas:        " << stats.modulo_lemmas << std::endl;
    s << "bounding lemmas:      " << stats.bounding_lemmas << std::endl;
    s << "interpolation lemmas: " << stats.interpolation_lemmas << std::endl;
    s << "monotonicity lemmas:  " << stats.monotonicity_lemmas << std::endl;
    s << "non constant base:    " << (stats.non_constant_base ? "true" : "false") << std::endl;
    return s;
}

Swine::Swine(const SmtSolver solver, const Config &config):
    AbsSmtSolver(solver->get_solver_enum()),
    util(solver, config),
    preproc(util),
    exp_finder(util),
    eval(util),
    config(config) {
    solver->set_opt("produce-models", "true");
    if (config.get_lemmas) {
        solver->set_opt("produce-unsat-assumptions", "true");
    }
    frames.emplace_back();
}

void Swine::set_opt(const std::string option, const std::string value) {
    util.solver->set_opt(option, value);
}

void Swine::set_logic(const std::string logic) {
    util.solver->set_logic(logic);
}

void Swine::add_lemma(const Term t, const LemmaKind kind) {
    if (config.log) {
        std::cout << kind << " lemma:" << std::endl;
        std::cout << t << std::endl;
    }
    const auto pp {preproc.preprocess(t)};
    if (config.validate || config.get_lemmas) frames.back().lemmas.emplace(pp, kind);
    if (config.get_lemmas) {
        static uint count {0};
        const auto assumption {make_symbol("assumption_" + std::to_string(count), util.solver->make_sort(BOOL))};
        ++count;
        frames.back().assumptions.emplace(assumption, pp);
        util.solver->assert_formula(util.solver->make_term(Equal, assumption, pp));
    } else {
        util.solver->assert_formula(pp);
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
    if (!config.is_active(LemmaKind::Symmetry) || config.eager_symmetry_lemmas) {
        return;
    }
    TermVec sym_lemmas;
    for (const auto &f: frames) {
        for (const auto &e: f.exps) {
            const auto ee {evaluate_exponential(e)};
            if (ee->base_val < 0) {
                base_symmetry_lemmas(e, sym_lemmas);
            }
            if (ee->exponent_val < 0) {
                exp_symmetry_lemmas(e, sym_lemmas);
            }
            if (ee->base_val < 0 && ee->exponent_val < 0) {
                const auto neg {util.make_exp(util.solver->make_term(Negate, ee->base), util.solver->make_term(Negate, ee->exponent))};
                base_symmetry_lemmas(neg, sym_lemmas);
                exp_symmetry_lemmas(neg, sym_lemmas);
            }
        }
    }
    for (const auto &l: sym_lemmas) {
        if (util.solver->get_value(l) != util.True) {
            lemmas.emplace(l, LemmaKind::Symmetry);
        }
    }
}

void Swine::base_symmetry_lemmas(const Term e, TermVec &lemmas) {
    if (!config.is_active(LemmaKind::Symmetry)) {
        return;
    }
    const auto [base, exp] {util.decompose_exp(e)};
    if (!base->is_value() || util.value(base) < 0) {
        const auto conclusion_even {make_term(
            Op(Equal),
            e,
            util.make_exp(
                make_term(Op(Negate), base),
                exp))};
        const auto conclusion_odd {make_term(
            Op(Equal),
            e,
            make_term(
                Op(Negate),
                util.make_exp(
                    make_term(Op(Negate), base),
                    exp)))};
        auto premise_even {make_term(
            Op(Equal),
            make_term(Op(Mod), exp, util.term(2)),
            util.term(0))};
        auto premise_odd {make_term(
            Op(Equal),
            make_term(Op(Mod), exp, util.term(2)),
            util.term(1))};
        if (config.semantics == Semantics::Partial) {
            premise_even = make_term(
                Op(And),
                premise_even,
                make_term(Op(Ge), exp, util.term(0)));
            premise_odd = make_term(
                Op(And),
                premise_odd,
                make_term(Op(Gt), exp, util.term(0)));
        }
        lemmas.push_back(make_term(Op(Implies), premise_even, conclusion_even));
        lemmas.push_back(make_term(Op(Implies), premise_odd, conclusion_odd));
    }
}

void Swine::exp_symmetry_lemmas(const Term e, TermVec &lemmas) {
    if (!config.is_active(LemmaKind::Symmetry)) {
        return;
    }
    if (config.semantics == Semantics::Total) {
        const auto [base, exp] {util.decompose_exp(e)};
        const auto lemma {make_term(
            Op(Equal),
            e,
            util.make_exp(base, make_term(Op(Negate), exp)))};
        lemmas.push_back(lemma);
    }
}

void Swine::add_symmetry_lemmas(const ExpGroup &g) {
    TermVec sym_lemmas;
    for (const auto &e: g.all()) {
        base_symmetry_lemmas(e, sym_lemmas);
        exp_symmetry_lemmas(e, sym_lemmas);
    }
    for (const auto &l: sym_lemmas) {
        add_lemma(l, LemmaKind::Symmetry);
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
        const auto [base, exp] {util.decompose_exp(e)};
        Term lemma;
        // exp = 0 ==> base^exp = 1
        lemma = make_term(
            Op(Implies),
            make_term(Op(Equal), exp, util.term(0)),
            make_term(Op(Equal), e, util.term(1)));
        set.push_back(lemma);
        // exp = 1 ==> base^exp = base
        lemma = make_term(
            Op(Implies),
            make_term(Op(Equal), exp, util.term(1)),
            make_term(Op(Equal), e, base));
        set.push_back(lemma);
        if (!base->is_value() || util.value(base) == 0) {
            // base = 0 && ... ==> base^exp = 0
            const auto conclusion {make_term(Op(Equal), e, util.term(0))};
            TermVec premises {make_term(Op(Equal), base, util.term(0))};
            PrimOp op;
            if (config.semantics == Semantics::Total) {
                premises.push_back(make_term(Op(Distinct), base, util.term(0)));
                op = Equal;
            } else {
                op = Implies;
                premises.push_back(make_term(Op(Gt), exp, util.term(0)));
            }
            lemma = make_term(
                op,
                make_term(Op(And), premises),
                conclusion);
            set.push_back(lemma);
        }
        if (!base->is_value() || util.value(base) == 1) {
            // base = 1 && ... ==> base^exp = 1
            const auto conclusion {make_term(Op(Equal), e, util.term(1))};
            Term premise {make_term(Op(Equal), base, util.term(1))};
            if (config.semantics == Semantics::Partial) {
                premise = make_term(
                    And,
                    premise,
                    make_term(Op(Ge), exp, util.term(0)));
            }
            lemma = make_term(Op(Implies), premise, conclusion);
            set.push_back(lemma);
        }
        if (!base->is_value() || util.value(base) > 1) {
            // exp + base > 4 && s > 1 && t > 1 ==> base^exp > s * t + 1
            lemma = make_term(
                Op(Implies),
                make_term(
                    Op(And),
                    make_term(
                        Op(Gt),
                        make_term(Op(Plus), base, exp),
                        util.term(4)),
                    make_term(Op(Gt), base, util.term(1)),
                    make_term(Op(Gt), exp, util.term(1))),
                make_term(
                    Op(Gt),
                    e,
                    make_term(
                        Op(Plus),
                        make_term(
                            Op(Mult),
                            base,
                            exp),
                        util.term(1))));
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
            if (seen.contains(g.orig())) {
                continue;
            }
            seen.emplace(g.orig());
            for (const auto &e: g.maybe_non_neg_base()) {
                const auto ee {evaluate_exponential(e)};
                if (ee && ee->exp_expression_val != ee->expected_val && ee->base_val >= 0) {
                    for (const auto &l: f.bounding_lemmas.at(e)) {
                        if (get_value(l) != util.True) {
                            lemmas.emplace(l, LemmaKind::Bounding);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void Swine::assert_formula(const Term & t) {
    ++stats.num_assertions;
    if (config.log) {
        std::cout << "assertion:" << std::endl;
        std::cout << t << std::endl;
    }
    const auto preprocessed {preproc.preprocess(t)};
    if (config.validate || config.get_lemmas) {
        frames.back().preprocessed_assertions.emplace(preprocessed, t);
    }
    util.solver->assert_formula(preprocessed);
    for (const auto &g: exp_finder.find_exps(preprocessed)) {
        if (frames.back().exps.insert(g.orig()).second) {
            frames.back().exp_groups.push_back(g);
            stats.non_constant_base |= !g.has_ground_base();
            if (config.eager_symmetry_lemmas) {
                add_symmetry_lemmas(g);
            }
            compute_bounding_lemmas(g);
        }
    }
}

long to_int(const std::string &s) {
    if (s.starts_with("(- ")) {
        return -to_int(s.substr(3, s.size() - 4));
    } else {
        return stol(s);
    }
}

long to_int(const Term &t) {
    return to_int(t->to_string());
}

std::optional<Swine::EvaluatedExponential> Swine::evaluate_exponential(const Term exp_expression) const {
    EvaluatedExponential res;
    res.exp_expression = exp_expression;
    res.exp_expression_val = util.value(get_value(exp_expression));
    auto it {exp_expression->begin()};
    ++it;
    res.base = *it;
    res.base_val = util.value(get_value(*it));
    ++it;
    res.exponent = *it;
    res.exponent_val = to_int(get_value(*it));
    if (util.config.semantics == Semantics::Partial && res.exponent_val < 0) {
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
    children[pos] = util.term(x1);
    const auto at_x1 {make_term(t->get_op(), children)};
    children[pos] = util.term(x2);
    const auto at_x2 {make_term(t->get_op(), children)};
    res.factor = abs(x2 - x1);
    res.t = make_term(
        Op(Plus),
        make_term(
            Op(Mult),
            util.term(res.factor),
            at_x1),
        make_term(
            Op(Mult),
            make_term(Op(Minus), at_x2, at_x1),
            make_term(Op(Minus), x, util.term(x1))));
    return res;
}

Term Swine::interpolation_lemma(Term t, const bool upper, const std::pair<cpp_int, long> a, const std::pair<cpp_int, long> b) {
    const auto x1 {min(a.first, b.first)};
    const auto x2 {max(a.first, b.first)};
    const auto y1 {std::min(a.second, b.second)};
    const auto y2 {std::max(a.second, b.second)};
    const auto [base, exp] {util.decompose_exp(t)};
    const auto op = upper ? Le : Ge;
    const auto gey {make_term(Op(Le), util.term(y1), exp, util.term(y2))};
    auto ley {make_term(Op(Gt), exp, util.term(0))};
    if (y2 > y1 + 1) {
        ley = make_term(
            Op(And),
            ley,
            make_term(
                Op(Or),
                make_term(Op(Ge), util.term(y1), exp),
                make_term(Op(Ge), exp, util.term(y2))));
    }
    if (base->is_value()) {
        const auto i {interpolate(t, 2, y1, y2)};
        const auto premise = upper ? gey : ley;
        return make_term(
            Op(Implies),
            premise,
            make_term(
                Op(op),
                make_term(Op(Mult), t, util.term(i.factor)),
                i.t));
    } else {
        const auto at_y1 {util.make_exp(base, util.term(y1))};
        const auto at_y2 {util.make_exp(base, util.term(y2))};
        const auto i1 {interpolate(at_y1, 1, x1, x2)};
        const auto i2 {interpolate(at_y1, 1, x1, x2)};
        Term premise;
        if (upper) {
            const auto gex {make_term(Op(Le), util.term(x1), base, util.term(x2))};
            premise = make_term(Op(And), gex, gey);
        } else {
            auto lex {make_term(Op(Gt), base, util.term(0))};
            if (x2 > x1 + 1) {
                lex = make_term(
                    Op(And),
                    lex,
                    make_term(
                        Op(Or),
                        make_term(Op(Ge), util.term(x1), base),
                        make_term(Op(Ge), base, util.term(x2))));
            }
            premise = make_term(Op(And), lex, ley);
        }
        const auto y_diff {util.term(y2 - y1)};
        const auto conclusion {make_term(
            Op(op),
            make_term(Op(Mult), t, util.term(i1.factor), y_diff),
            make_term(
                Op(Plus),
                make_term(Op(Mult), i1.t, y_diff),
                make_term(
                    Op(Mult),
                    make_term(Op(Minus), i2.t, i1.t),
                    make_term(Op(Minus), exp, util.term(y1)))))};
        return make_term(Op(Implies), premise, conclusion);
    }
}

void Swine::interpolation_lemma(const EvaluatedExponential &e, std::unordered_map<Term, LemmaKind> &lemmas) {
    Term lemma;
    auto &vec {interpolation_points.emplace(e.exp_expression, std::vector<std::pair<cpp_int, long>>()).first->second};
    if (e.exp_expression_val < e.expected_val) {
        const auto min_base = e.base_val > 1 ? e.base_val - 1 : e.base_val;
        const auto min_exp = e.exponent_val > 1 ? e.exponent_val - 1 : e.exponent_val;
        lemma = interpolation_lemma(e.exp_expression, false, {min_base, min_exp}, {min_base + 1, min_exp + 1});
    } else {
        std::pair<cpp_int, long> nearest {1, 1};
        auto min_dist {e.base_val * e.base_val + e.exponent_val * e.exponent_val};
        for (const auto &[x, y]: vec) {
            const auto x_dist {x - e.base_val};
            const auto y_dist {y - e.exponent_val};
            const auto dist {x_dist * x_dist + y_dist * y_dist};
            if (0 < dist && dist <= min_dist) {
                nearest = {x, y};
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
            for (const auto &e: g.maybe_non_neg_base()) {
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
        Op(Lt),
        smaller.exponent,
        greater.exponent)};
    const Term non_strict_exp_premise {make_term(
        Op(Le),
        smaller.exponent,
        greater.exponent)};
    if (!smaller.base->is_value() || !greater.base->is_value()) {
        const Term strict_base_premise {make_term(
            Op(Lt),
            smaller.base,
            greater.base)};
        const Term non_strict_base_premise {make_term(
            Op(Le),
            smaller.base,
            greater.base)};
        premise =
            make_term(
                Op(And),
                non_strict_base_premise,
                non_strict_exp_premise,
                make_term(
                    Op(Or),
                    strict_base_premise,
                    strict_exp_premise));
    } else if (smaller.base_val < greater.base_val) {
        premise = non_strict_exp_premise;
    } else {
        premise = strict_exp_premise;
    }
    return make_term(
        Op(Implies),
        make_term(
            Op(And),
            make_term(Lt, util.term(0), smaller.base),
            make_term(
                Op(Le),
                util.term(0),
                smaller.exponent),
            premise),
        make_term(Op(Lt), smaller.exp_expression, greater.exp_expression));
}

void Swine::monotonicity_lemmas(std::unordered_map<Term, LemmaKind> &lemmas) {
    if (!config.is_active(LemmaKind::Monotonicity)) {
        return;
    }
    // search for pairs exp(b,e1), exp(b,e2) whose models violate monotonicity of exp
    TermVec exps;
    for (const auto &f: frames) {
        for (const auto &g: f.exp_groups) {
            for (const auto &e: g.maybe_non_neg_base()) {
                const auto [base, exp] {util.decompose_exp(e)};
                if (util.value(get_value(base)) > 0 && util.value(get_value(exp)) >= 0) {
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
            if (ee->exponent_val > 0 && ee->exp_expression_val % abs(ee->base_val) != 0) {
                auto l {make_term(
                    Equal,
                    util.term(0),
                    make_term(Mod, ee->exp_expression, ee->base))};
                if (config.semantics == Semantics::Partial) {
                    l = make_term(
                        Implies,
                        make_term(Gt, ee->exponent, util.term(0)),
                        l);
                } else {
                    l = make_term(
                        Implies,
                        make_term(Distinct, ee->exponent, util.term(0)),
                        l);
                }
                lemmas.emplace(l, LemmaKind::Modulo);
            }
        }
    }
}

Result Swine::check_sat() {
    Result res;
    while (true) {
        ++stats.iterations;
        if (config.get_lemmas) {
            TermVec assumptions;
            for (const auto &f: frames) {
                for (const auto &[a,_]: f.assumptions) {
                    assumptions.push_back(a);
                }
            }
            res = util.solver->check_sat_assuming(assumptions);
        } else {
            res = util.solver->check_sat();
        }
        if (res.is_unsat()) {
            if (config.validate) {
                brute_force();
            }
            if (config.get_lemmas) {
                UnorderedTermSet core;
                util.solver->get_unsat_assumptions(core);
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
                if (config.validate) {
                    verify();
                }
                break;
            }
            if (lemmas.empty()) {
                symmetry_lemmas(lemmas);
            }
            if (lemmas.empty()) {
                bounding_lemmas(lemmas);
            }
            if (lemmas.empty()) {
                monotonicity_lemmas(lemmas);
            }
            if (lemmas.empty()) {
                mod_lemmas(lemmas);
            }
            if (lemmas.empty()) {
                interpolation_lemmas(lemmas);
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
    }
    if (config.statistics) {
        std::cout << stats << std::endl;
    }
    return res;
}

Result Swine::check_sat_assuming(const TermVec & assumptions) {
    return util.solver->check_sat_assuming(assumptions);
}

Result Swine::check_sat_assuming_list(const TermList & assumptions) {
    return util.solver->check_sat_assuming_list(assumptions);
}

Result Swine::check_sat_assuming_set(const UnorderedTermSet & assumptions) {
    return util.solver->check_sat_assuming_set(assumptions);
}

void Swine::push(uint64_t num) {
    util.solver->push(num);
    for (unsigned i = 0; i < num; ++i) {
        frames.emplace_back();
    }
}

void Swine::pop(uint64_t num) {
    util.solver->pop(num);
    for (unsigned i = 0; i < num; ++i) {
        frames.pop_back();
    }
}

uint64_t Swine::get_context_level() const {
    return util.solver->get_context_level();
}

Term Swine::get_value(const Term & t) const {
    return util.solver->get_value(t);
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
    for (const auto &f: frames) {
        for (const auto &x: f.symbols) {
            if (assumptions.contains(x)) {
                continue;
            }
            if (!uf && x->get_sort()->get_sort_kind() == SortKind::FUNCTION) {
                uf = true;
                std::cerr << "get_model does not support uninterpreted functions at the moment" << std::endl;
            } else {
                res.emplace(x, get_value(x));
            }
        }
        for (const auto &g: f.exp_groups) {
            for (const auto &e: g.all()) {
                res.emplace(e, get_value(e));
            }
        }
    }
    return res;
}

UnorderedTermMap Swine::get_array_values(const Term & arr,
                                         Term & out_const_base) const {
    return util.solver->get_array_values(arr, out_const_base);
}

void Swine::get_unsat_assumptions(UnorderedTermSet & out) {
    util.solver->get_unsat_assumptions(out);
}

Sort Swine::make_sort(const std::string name, uint64_t arity) const {
    return util.solver->make_sort(name, arity);
}

Sort Swine::make_sort(SortKind sk) const {
    return util.solver->make_sort(sk);
}

Sort Swine::make_sort(SortKind sk, uint64_t size) const {
    return util.solver->make_sort(sk, size);
}

Sort Swine::make_sort(SortKind sk, const Sort & sort1) const {
    return util.solver->make_sort(sk, sort1);
}

Sort Swine::make_sort(SortKind sk,
                      const Sort & sort1,
                      const Sort & sort2) const {
    return util.solver->make_sort(sk, sort1, sort2);
}

Sort Swine::make_sort(SortKind sk,
                      const Sort & sort1,
                      const Sort & sort2,
                      const Sort & sort3) const {
    return util.solver->make_sort(sk, sort1, sort2, sort3);
}

Sort Swine::make_sort(SortKind sk, const SortVec & sorts) const {
    return util.solver->make_sort(sk, sorts);
}

Sort Swine::make_sort(const Sort & sort_con, const SortVec & sorts) const {
    return util.solver->make_sort(sort_con, sorts);
}

Sort Swine::make_sort(const DatatypeDecl & d) const {
    return util.solver->make_sort(d);
}

DatatypeDecl Swine::make_datatype_decl(const std::string & s) {
    return util.solver->make_datatype_decl(s);
}

DatatypeConstructorDecl Swine::make_datatype_constructor_decl(
        const std::string s) {
    return util.solver->make_datatype_constructor_decl(s);
}

void Swine::add_constructor(DatatypeDecl & dt,
                            const DatatypeConstructorDecl & con) const {
    util.solver->add_constructor(dt, con);
}

void Swine::add_selector(DatatypeConstructorDecl & dt,
                         const std::string & name,
                         const Sort & s) const {
    util.solver->add_selector(dt, name, s);
}

void Swine::add_selector_self(DatatypeConstructorDecl & dt,
                              const std::string & name) const {
    util.solver->add_selector_self(dt, name);
}

Term Swine::get_constructor(const Sort & s, std::string name) const {
    return util.solver->get_constructor(s, name);
}

Term Swine::get_tester(const Sort & s, std::string name) const {
    return util.solver->get_tester(s, name);
}

Term Swine::get_selector(const Sort & s,
                         std::string con,
                         std::string name) const {
    return util.solver->get_selector(s, con, name);
}

Term Swine::make_term(bool b) const {
    return util.solver->make_term(b);
}

Term Swine::make_term(int64_t i, const Sort & sort) const {
    return util.solver->make_term(i, sort);
}

Term Swine::make_term(const std::string val,
                      const Sort & sort,
                      uint64_t base) const {
    return util.solver->make_term(val, sort, base);
}

Term Swine::make_term(const Term & val, const Sort & sort) const {
    return util.solver->make_term(val, sort);
}

Term Swine::make_symbol(const std::string name, const Sort & sort) {
    const auto res {util.solver->make_symbol(name, sort)};
    frames.back().symbols.insert(res);
    return res;
}

Term Swine::get_symbol(const std::string & name) {
    return util.solver->get_symbol(name);
}

Term Swine::make_param(const std::string name, const Sort & sort) {
    return util.solver->make_param(name, sort);
}

Term Swine::make_term(Op op, const Term & t) const {
    return util.solver->make_term(op, t);
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
    if (op == Op(Exp)) {
        return util.solver->make_term(Op(Apply), util.exp, terms.front(), *(++terms.begin()));
    } else if (config.solver_kind == SolverKind::Z3 && terms.size() > 2) {
        switch (op.prim_op) {
        case Mult: {
            auto it {terms.begin()};
            auto res {*it};
            while (++it != terms.end()) {
                res = util.solver->make_term(op, res, *it);
            }
            return res;
        }
        case Le:
        case Lt:
        case Ge:
        case Gt: {
                auto it {terms.begin()};
                auto last = *it;
                TermVec args;
                for (++it; it != terms.end(); ++it) {
                    args.push_back(util.solver->make_term(op, last, *it));
                    last = *it;
                }
                return util.solver->make_term(And, args);
            }
        default: break;
        }
    }
    return util.solver->make_term(op, terms);
}

void Swine::reset() {
    util.solver->reset();
    frames.clear();
    frames.emplace_back();
}

void Swine::reset_assertions() {
    util.solver->reset_assertions();
    frames.clear();
    frames.emplace_back();
}

Term Swine::substitute(const Term term,
                       const UnorderedTermMap & substitution_map) const {
    return util.solver->substitute(term, substitution_map);
}

void Swine::dump_smt2(std::string filename) const {
    util.solver->dump_smt2(filename);
}

void Swine::verify() const {
    for (const auto &f: frames) {
        for (const auto &[_,a]: f.preprocessed_assertions) {
            if (eval.evaluate(a) != util.True) {
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
    BruteForce bf(util, assertions, exps);
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
                if (get_value(l) != util.True) {
                    std::cout << "violated " << kind << " lemma" << std::endl;
                    std::cout << l << std::endl;
                }
            }
        }
        verify();
    }
}
