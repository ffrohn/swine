#include "swine.h"

#include "brute_force.h"

#include <smt-switch/smtlib_reader.h>
#include <smt-switch/cvc5_factory.h>
#include <smt-switch/z3_factory.h>
//#include "smt-switch/yices2_factory.h"
#include <boost/algorithm/string.hpp>
#include <assert.h>

std::ostream& operator<<(std::ostream &s, const LemmaKind kind) {
    switch (kind) {
    case Initial: s << "initial";
        break;
    case Tangent: s << "tangent";
        break;
    case Secant: s << "secant";
        break;
    case Monotonicity: s << "monotonicity";
        break;
    default: throw std::invalid_argument("unknown lemma kind");
    }
    return s;
}

bool Swine::validate {false};
bool Swine::log {false};
bool Swine::statistics {false};

Swine::Swine(const SmtSolver solver, const SolverKind solver_kind):
    AbsSmtSolver(solver->get_solver_enum()),
    solver(solver),
    solver_kind(solver_kind),
    int_sort(solver->make_sort(SortKind::INT)),
    exp(solver->make_symbol("exp", solver->make_sort(SortKind::FUNCTION, {int_sort, int_sort, int_sort}))),
    flattener(solver, exp) {
    solver->set_opt("produce-models", "true");
    frames.emplace_back();
}

void Swine::set_opt(const std::string option, const std::string value) {
    solver->set_opt(option, value);
}

void Swine::set_logic(const std::string logic) {
    solver->set_logic(logic);
}

void Swine::add_lemma(const Term t, const LemmaKind kind) {
    solver->assert_formula(t);
    if (validate) frames.back().lemmas.emplace(t, kind);
    switch (kind) {
    case Tangent: ++stats.tangent_lemmas;
        break;
    case Secant: ++stats.secant_lemmas;
        break;
    case Initial: ++stats.initial_lemmas;
        break;
    case Monotonicity: ++stats.monotonicity_lemmas;
        break;
    default: throw std::invalid_argument("unknown lemma kind");
    }
}

void Swine::add_initial_lemmas(const Term e) {
    auto it {e->begin()};
    ++it;
    const auto base {*it};
    if (!base->is_value()) {
        stats.non_constant_base = true;
    }
    ++it;
    const auto exp {*it};
    const auto pos {make_term(Op(Gt), exp, term(0))};
    if (log) std::cout << "initial lemmas" << std::endl;
    Term lemma;
    // exp = 0 ==> base^exp = 1
    lemma =
        make_term(
            Op(Implies),
            make_term(Op(Equal), exp, term(0)),
            make_term(Op(Equal), e, term(1)));
    if (log) std::cout << lemma << std::endl;
    add_lemma(lemma, Initial);
    // exp > 0 && base = 0 ==> base^exp = 0
    lemma =
        make_term(
            Op(Implies),
            make_term(
                Op(And),
                pos,
                make_term(Op(Equal), base, term(0))),
            make_term(Op(Equal), e, term(0)));
    if (log) std::cout << lemma << std::endl;
    add_lemma(lemma, Initial);
    // exp > 0 && base = 1 ==> base^exp = 1
    lemma =
        make_term(
            Op(Implies),
            make_term(
                Op(And),
                pos,
                make_term(Op(Equal), base, term(1))),
            make_term(Op(Equal), e, term(1)));
    if (log) std::cout << lemma << std::endl;
    add_lemma(lemma, Initial);
    // exp > 0 && base > 1 ==> base^exp > exp
    lemma =
        make_term(
            Op(Implies),
            make_term(
                Op(And),
                pos,
                make_term(Op(Gt), base, term(1))),
            make_term(Op(Gt), e, exp));
    if (log) std::cout << lemma << std::endl;
    add_lemma(lemma, Initial);
}

void Swine::assert_formula(const Term & t) {
    ++stats.num_assertions;
    const auto flat {flattener.flatten(t)};
    if (log && flat != t) {
        std::cout << "flattening" << std::endl;
        std::cout << "original term:" << std::endl;
        std::cout << t << std::endl;
        std::cout << "new term" << std::endl;
        std::cout << flat << std::endl;
    }
    if (validate) {
        frames.back().assertions.push_back(t);
        frames.back().flat_assertions.push_back(flat);
    }
    solver->assert_formula(flat);
    const auto exps {flattener.clear_exps()};
    if (!exps.empty()) {
        frames.back().exps.insert(exps.begin(), exps.end());
        for (const auto &e: exps) {
            add_initial_lemmas(e);
        }
    }
}

cpp_int to_cpp_int(const std::string &s) {
    if (s.starts_with("(- ")) {
        return -to_cpp_int(s.substr(3, s.size() - 4));
    } else {
        return cpp_int(s);
    }
}

cpp_int value(const Term term) {
    return to_cpp_int(term->to_string());
}

Term Swine::term(const cpp_int &value) {
    return solver->make_term(value.str(), int_sort);
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

std::optional<Swine::EvaluatedExp> Swine::evaluate_exponential(const Term exp_expression) const {
    EvaluatedExp res;
    res.exp_expression = exp_expression;
    res.exp_expression_val = value(get_value(exp_expression));
    auto it {exp_expression->begin()};
    ++it;
    res.base = *it;
    res.base_val = value(get_value(*it));
    ++it;
    res.exponent = *it;
    res.exponent_val = to_int(get_value(*it));
    if (res.exponent_val < 0) {
        return {};
    }
    res.expected_val = boost::multiprecision::pow(res.base_val, res.exponent_val);
    return res;
}

Term Swine::tangent_lemma(const EvaluatedExp &e, const bool next) {
    const auto other_val {pow(e.base_val, next ? e.exponent_val + 1 : e.exponent_val - 1)};
    const auto diff {abs(e.expected_val - other_val)};
    const auto [fst_exponent, fst_val] = next
            ? std::pair(e.exponent_val, e.expected_val)
            : std::pair(e.exponent_val - 1, other_val);
    const auto tangent {make_term(
        Op(Plus),
        term(fst_val),
        make_term(
            Op(Mult),
            term(diff),
            make_term(Op(Minus), e.exponent, term(fst_exponent))))};
    const auto tangent_lemma {make_term(
        Op(Implies),
        make_term(Op(Ge), e.exponent, term(0)),
        make_term(Op(Ge), e.exp_expression, tangent))};
    if (log) std::cout << tangent_lemma << std::endl;
    return tangent_lemma;
}

void Swine::tangent_lemmas(const EvaluatedExp &e, std::unordered_map<Term, LemmaKind> &lemmas) {
    if (!e.base->is_value()) {
        return;
    }
    lemmas.emplace(tangent_lemma(e, true), Tangent);
    if (e.exponent_val > 1) {
        lemmas.emplace(tangent_lemma(e, false), Tangent);
    }
}

Term Swine::secant_lemma(const EvaluatedExp &e, const long other_exponent_val) {
    const auto other_val {pow(e.base_val, other_exponent_val)};
    const auto factor {other_exponent_val - e.exponent_val};
    const auto secant {make_term(
        Op(Plus),
        make_term(
            Op(Mult),
            term(other_val - e.expected_val),
            make_term(Op(Minus), e.exponent, term(other_exponent_val))),
        term(other_val * factor))};
    Term premise;
    if (other_exponent_val <= e.exponent_val) {
        premise =
            make_term(
                Op(And),
                make_term(Op(Ge), e.exponent, term(other_exponent_val)),
                make_term(Op(Le), e.exponent, term(e.exponent_val)));
    } else {
        premise =
            make_term(
                Op(And),
                make_term(Op(Le), e.exponent, term(other_exponent_val)),
                make_term(Op(Ge), e.exponent, term(e.exponent_val)));
    }
    const auto res {make_term(
        Op(Implies),
        premise,
        make_term(
            Op(Le),
            make_term(Op(Mult), term(factor), e.exp_expression),
            secant))};
    if (log) std::cout << res << std::endl;
    return res;
}

void Swine::secant_lemmas(const EvaluatedExp &e, std::unordered_map<Term, LemmaKind> &lemmas) {
    if (!e.base->is_value()) {
        return;
    }
    // the actual value is too large --> add secant refinement lemmas
    std::optional<long> prev;
    std::optional<long> next;
    // look for the closest existing secant points
    auto it {secant_points.emplace(e.exp_expression, std::vector<long>()).first};
    for (const auto &other: it->second) {
        if (other < e.exponent_val) {
            if (!prev || other > *prev) {
                prev = other;
            }
        } else if (!next || other < *next) {
            next = other;
        }
    }
    // if there are none, use the neighbors
    if (!prev) {
        prev = e.exponent_val - 1;
    }
    if (!next) {
        next = e.exponent_val + 1;
    }
    // store the current secant point
    it->second.emplace_back(e.exponent_val);
    lemmas.emplace(secant_lemma(e, *prev), Secant);
    lemmas.emplace(secant_lemma(e, *next), Secant);
}

std::optional<Term> Swine::monotonicity_lemma(const EvaluatedExp &e1, const EvaluatedExp &e2) {
    if ((e1.base_val > e2.base_val && e1.exponent_val < e2.exponent_val)
            || (e1.base_val < e2.base_val && e1.exponent_val > e2.exponent_val)
            || (e1.base_val == e2.base_val && e1.exponent_val == e2.exponent_val)) {
        return {};
    }
    bool is_smaller = e1.base_val < e2.base_val || e1.exponent_val < e2.exponent_val;
    const auto [smaller, greater] = is_smaller ? std::pair(e1, e2) : std::pair(e2, e1);
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
    const auto monotonicity_lemma {make_term(
        Op(Implies),
        make_term(
            Op(And),
            make_term(
                Op(Le),
                term(0),
                smaller.exponent),
            premise),
        make_term(Op(Lt), smaller.exp_expression, greater.exp_expression))};
    if (log) {
        std::cout << "monotonicity lemma" << std::endl;
        std::cout << monotonicity_lemma << std::endl;
    }
    return monotonicity_lemma;
}

void Swine::monotonicity_lemmas(std::unordered_map<Term, LemmaKind> &lemmas) {
    // search for pairs exp(b,e1), exp(b,e2) whose models violate monotonicity of exp
    for (auto it1 = frames.begin(); it1 != frames.end(); ++it1) {
        for (const auto &exp1: it1->exps) {
            const auto e1 {evaluate_exponential(exp1)};
            if (e1 && e1->base_val > 1) {
                for (auto it2 = it1; ++it2 != frames.end();) {
                    for (const auto &exp2: it2->exps) {
                        const auto e2 {evaluate_exponential(exp2)};
                        if (e2 && e2->base_val > 1) {
                            const auto mon_lemma {monotonicity_lemma(*e1, *e2)};
                            if (mon_lemma) {
                                lemmas.emplace(*mon_lemma, Monotonicity);
                            }
                        }
                    }
                }
            }
        }
    }
}

Result Swine::check_sat() {
    Result res;
    while (true) {
        ++stats.iterations;
        res = solver->check_sat();
        if (res.is_unsat()) {
            if (validate) {
                brute_force();
            }
            break;
        } else if (res.is_unknown()) {
            break;
        } else if (res.is_sat()) {
            bool sat {true};
            if (log) {
                std::cout << "candidate model:" << std::endl;
                for (const auto &[key,val]: get_model()) {
                    std::cout << key << " = " << val << std::endl;
                }
            }
            std::unordered_map<Term, LemmaKind> lemmas;
            // check if the model can be lifted, add refinement lemmas otherwise
            for (const auto &f: frames) {
                for (const auto &e: f.exps) {
                    const auto ee {evaluate_exponential(e)};
                    if (log) {
                        std::cout << "trying to lift model for " << e << std::endl;
                        std::cout << "base: " << ee->base_val << std::endl;
                        std::cout << "exponent: " << ee->exponent_val << std::endl;
                        std::cout << "e: " << ee->exp_expression_val << std::endl;
                    }
                    // if the exponent is negative, integer exponentiation is undefined
                    // in smtlib, this means that the result can be arbitrary
                    if (ee && ee->exponent_val >= 0) {
                        if (ee->exp_expression_val < ee->expected_val) {
                            if (log) std::cout << "tangent refinement" << std::endl;
                            tangent_lemmas(*ee, lemmas);
                            sat = false;
                        } else if (ee->exp_expression_val > ee->expected_val) {
                            if (log) std::cout << "secant refinement" << std::endl;
                            secant_lemmas(*ee, lemmas);
                            sat = false;
                        }
                    }
                }
            }
            if (sat) {
                if (validate) {
                    verify();
                }
                break;
            }
            monotonicity_lemmas(lemmas);
            for (const auto &[l, kind]: lemmas) {
                add_lemma(l, kind);
            }
        }
    }
    if (statistics) {
        std::cout << stats << std::endl;
    }
    return res;
}

Result Swine::check_sat_assuming(const TermVec & assumptions) {
    return solver->check_sat_assuming(assumptions);
}

Result Swine::check_sat_assuming_list(const TermList & assumptions) {
    return solver->check_sat_assuming_list(assumptions);
}

Result Swine::check_sat_assuming_set(const UnorderedTermSet & assumptions) {
    return solver->check_sat_assuming_set(assumptions);
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
    return solver->get_value(t);
}

UnorderedTermMap Swine::get_model() const {
    UnorderedTermMap res;
    bool uf {false};
    for (const auto &f: frames) {
        for (const auto &x: f.symbols) {
            if (!uf && x->get_sort()->get_sort_kind() == SortKind::FUNCTION) {
                uf = true;
                std::cerr << "get_model does not support uninterpreted functions at the moment" << std::endl;
            } else {
                const auto val {get_value(x)};
                res.emplace(x, val);
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
    frames.back().symbols.insert(res);
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
    if (op == Op(Exp)) {
        if (t1->is_value() && to_int(t1) >= 0) {
            return solver->make_term(Op(Pow), t0, t1);
        } else {
            return solver->make_term(Op(Apply), exp, t0, t1);
        }
    } else {
        return solver->make_term(op, t0, t1);
    }
}

Term Swine::make_term(Op op,
                      const Term & t0,
                      const Term & t1,
                      const Term & t2) const {
    if (op == Op(Mult) && solver_kind == SolverKind::Z3) {
        return make_term(Op(Mult), {t0, t1, t2});
    } else {
        return solver->make_term(op, t0, t1, t2);
    }
}

Term Swine::make_term(Op op, const TermVec & terms) const {
    if (op == Op(Exp)) {
        assert(terms.size() == 2);
        return make_term(op, terms[0], terms[1]);
    } else if (op == Op(Mult) && solver_kind == SolverKind::Z3 && terms[0]->get_sort() == int_sort) {
        // for some reason, n-ary integer multiplication doesn't work with Z3
        // fall back to binary multiplication
        auto it {terms.begin()};
        auto res {*it};
        while (++it != terms.end()) {
            res = solver->make_term(op, res, *it);
        }
        return res;
    } else {
        return solver->make_term(op, terms);
    }
}

void Swine::reset() {
    solver->reset();
    frames = {};
    frames.emplace_back();
}

void Swine::reset_assertions() {
    solver->reset_assertions();
    frames = {};
    frames.emplace_back();
}

Term Swine::substitute(const Term term,
                       const UnorderedTermMap & substitution_map) const {
    return solver->substitute(term, substitution_map);
}

void Swine::dump_smt2(std::string filename) const {
    solver->dump_smt2(filename);
}

cpp_int Swine::evaluate_int(Term expression) const {
    if (expression->is_value()) {
        return value(expression);
    } else if (expression->is_symbol()) {
        return evaluate_int(solver->get_value(expression));
    } else if (expression->get_op() == Op(Apply) && *expression->begin() == exp) {
        auto it {expression->begin()};
        const auto fst {evaluate_int(*(++it))};
        const auto snd {stol(evaluate_int(*(++it)).str())};
        return pow(fst, snd);
    } else if (expression->get_op() == Op(Pow)) {
        auto it {expression->begin()};
        const auto fst {evaluate_int(*it)};
        const auto snd {stol(evaluate_int(*(++it)).str())};
        return pow(fst, snd);
    } else if (expression->get_op() == Op(Negate)) {
        return -evaluate_int(*expression->begin());
    } else {
        const auto eval = [&](const std::function<cpp_int(cpp_int&, cpp_int&)> &fun, const cpp_int neutral) {
            auto cur {neutral};
            for (auto it = expression->begin(); it != expression->end(); ++it) {
                auto next {evaluate_int(*it)};
                cur = fun(cur, next);
            }
            return cur;
        };
        if (expression->get_op() == Op(Plus)) {
            return eval([&](auto &x, auto &y) {
                return x + y;
            }, 0);
        } else if (expression->get_op() == Op(Mult)) {
            return eval([&](auto &x, auto &y) {
                return x * y;
            }, 1);
        } else if (expression->get_op() == Op(Minus)) {
            return eval([&](auto &x, auto &y) {
                return x - y;
            }, 0);
        } else {
            std::cout << expression->get_op() << std::endl;
            throw std::invalid_argument("failed to evaluate " + expression->to_string());
        }
    }
}

bool Swine::evaluate_bool(Term expression) const {
    if (expression->is_value()) {
        return boost::iequals(expression->to_string(), "true");
    } else if (expression->is_symbol()) {
        return evaluate_bool(solver->get_value(expression));
    } else if (expression->get_op() == Op(And)) {
        for (const auto e: expression) {
            if (!evaluate_bool(e)) {
                return false;
            }
        }
        return true;
    } else if (expression->get_op() == Op(Or)) {
        for (const auto e: expression) {
            if (evaluate_bool(e)) {
                return true;
            }
        }
        return false;
    } else if (expression->get_op() == Op(Not)) {
        return !evaluate_bool(*expression->begin());
    } else if (expression->get_op() == Op(Equal) && (*expression->begin())->get_sort() != int_sort) {
        auto it {expression->begin()};
        const auto fst {evaluate_bool(*it)};
        while (++it != expression->end()) {
            if (evaluate_bool(*it) != fst) {
                return false;
            }
        }
        return true;
    } else {
        const auto test_comparison = [&](const std::function<bool(cpp_int, cpp_int)> &pred) {
            auto it {expression->begin()};
            auto cur {evaluate_int(*it)};
            while (++it != expression->end()) {
                const auto next {evaluate_int(*it)};
                if (!pred(cur, next)) {
                    return false;
                } else {
                    cur = next;
                }
            }
            return true;
        };
        if (expression->get_op() == Op(Lt)) {
            return test_comparison([](auto x, auto y) {
                return x < y;
            });
        } else if (expression->get_op() == Op(Le)) {
            return test_comparison([](auto x, auto y) {
                return x <= y;
            });
        } else if (expression->get_op() == Op(Gt)) {
            return test_comparison([](auto x, auto y) {
                return x > y;
            });
        } else if (expression->get_op() == Op(Ge)) {
            return test_comparison([](auto x, auto y) {
                return x >= y;
            });
        } else if (expression->get_op() == Op(Equal)) {
            return test_comparison([](auto x, auto y) {
                return x == y;
            });
        } else {
            throw std::invalid_argument("failed to evaluate " + expression->to_string());
        }
    }
}

void Swine::verify() const {
    for (const auto &f: frames) {
        for (const auto &a: f.assertions) {
            if (!evaluate_bool(a)) {
                std::cout << "Validation of the following assertion failed:" << std::endl;
                std::cout << a << std::endl;
                std::cout << "model:" << std::endl;
                for (const auto &[key, value]: get_model()) {
                    std::cout << key << " = " << value << std::endl;
                }
                for (const auto &f: frames) {
                    for (const auto &e: f.exps) {
                        const auto ee {evaluate_exponential(e)};
                        if (ee) {
                            std::cout << ee->exp_expression << ":" << std::endl;
                            std::cout << ee->base_val << "^" << ee->exponent_val << " = " << ee->exp_expression_val << std::endl;
                        }
                    }
                }
                return;
            }
        }
    }
}

void Swine::brute_force() const {
    TermVec assertions;
    for (const auto &f: frames) {
        for (const auto &a: f.flat_assertions) {
            assertions.push_back(a);
        }
    }
    TermVec exps;
    for (const auto &f: frames) {
        for (const auto &e: f.exps) {
            exps.push_back(e);
        }
    }
    BruteForce bf(solver, assertions, exps);
    if (bf.check_sat()) {
        std::cout << "sat via brute force" << std::endl;
        for (const auto &f: frames) {
            for (const auto &[l, kind]: f.lemmas) {
                if (!boost::iequals(solver->get_value(l)->to_string(), "true")) {
                    std::cout << "violated " << kind << " lemma" << std::endl;
                    std::cout << l << std::endl;
                }
            }
        }
        verify();
    }
}

std::ostream& operator<<(std::ostream &s, const Swine::Statistics &stats) {
    s << "assertions:          " << stats.num_assertions << std::endl;
    s << "iterations:          " << stats.iterations << std::endl;
    s << "initial lemmas:      " << stats.initial_lemmas << std::endl;
    s << "tangent lemmas:      " << stats.tangent_lemmas << std::endl;
    s << "secant lemmas:       " << stats.secant_lemmas << std::endl;
    s << "monotonicity lemmas: " << stats.monotonicity_lemmas << std::endl;
    s << "non constant base:   " << (stats.non_constant_base ? "true" : "false") << std::endl;
    return s;
}

int main(int argc, char *argv[]) {
    int arg = 0;
    auto get_next = [&]() {
        if (arg < argc-1) {
            return argv[++arg];
        } else {
            std::cout << "Error: Argument missing for " << argv[arg] << std::endl;
            exit(1);
        }
    };
    SmtSolver solver;
    SolverKind solver_kind {SolverKind::Z3};
    std::optional<std::string> input;
    while (++arg < argc) {
        if (boost::iequals(argv[arg], "--solver")) {
            const std::string solver_str {get_next()};
            if (boost::iequals(solver_str, "z3")) {
                solver = smt::Z3SolverFactory::create(false);
                solver_kind = SolverKind::Z3;
            } else if (boost::iequals(solver_str, "cvc5")) {
                solver = smt::Cvc5SolverFactory::create(false);
                solver_kind = SolverKind::CVC5;
                //            } else if (boost::iequals(solver_str, "yices")) {
                //                solver = smt::Yices2SolverFactory::create(false);
                //                solver_kind = SolverKind::Yices;
            } else {
                throw std::invalid_argument("unknown solver " + solver_str);
            }
        } else if (boost::iequals(argv[arg], "--validate")) {
            Swine::validate = true;
        } else if (boost::iequals(argv[arg], "--log")) {
            Swine::log = true;
        } else if (boost::iequals(argv[arg], "--stats")) {
            Swine::statistics = true;
        } else if (!input) {
            input = argv[arg];
        } else {
            throw std::invalid_argument("extra argument " + std::string(argv[arg]));
        }
    }
    if (!solver) {
        solver = smt::Z3SolverFactory::create(false);
    }
    if (!input) {
        throw std::invalid_argument("missing input file");
    }
    SmtSolver swine = std::make_shared<Swine>(solver, solver_kind);
    return SmtLibReader(swine).parse(*input);
}
