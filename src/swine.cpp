#include "swine.h"

#include <smt-switch/smtlib_reader.h>
#include "smt-switch/cvc5_factory.h"
#include "smt-switch/z3_factory.h"
//#include "smt-switch/yices2_factory.h"
#include <boost/algorithm/string.hpp>
#include <assert.h>

Swine::Swine(const SmtSolver solver):
    AbsSmtSolver(solver->get_solver_enum()),
    solver(solver),
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

void Swine::add_initial_lemmas(const Term e) {
    auto it {e->begin()};
    ++it;
    const auto base {*it};
    ++it;
    const auto exp {*it};
    const auto pos {make_term(Op(Gt), exp, term(0))};
    if (log) std::cout << "initial lemmas" << std::endl;
    Term lemma;
    // exp = 0 ==> base^exp = 1
    lemma = make_term(
                Op(Implies),
                make_term(Op(Equal), exp, term(0)),
                make_term(Op(Equal), e, term(1)));
    if (log) std::cout << lemma << std::endl;
    solver->assert_formula(lemma);
    // exp > 0 && base = 0 ==> base^exp = 0
    lemma = make_term(
                Op(Implies),
                make_term(
                    Op(And),
                    pos,
                    make_term(Op(Equal), base, term(0))),
                make_term(Op(Equal), e, term(0)));
    if (log) std::cout << lemma << std::endl;
    solver->assert_formula(lemma);
    // exp > 0 && base = 1 ==> base^exp = 1
    lemma = make_term(
                Op(Implies),
                make_term(
                    Op(And),
                    pos,
                    make_term(Op(Equal), base, term(1))),
                make_term(Op(Equal), e, term(1)));
    if (log) std::cout << lemma << std::endl;
    solver->assert_formula(lemma);
    // exp > 0 && base > 1 ==> base^exp > exp
    lemma = make_term(
                Op(Implies),
                make_term(
                    Op(And),
                    pos,
                    make_term(Op(Gt), base, term(1))),
                make_term(Op(Gt), e, exp));
    if (log) std::cout << lemma << std::endl;
    solver->assert_formula(lemma);
}

void Swine::assert_formula(const Term & t) {
    const auto flat {flattener.flatten(t)};
    solver->assert_formula(t);
    const auto exps {flattener.clear_exps()};
    if (!exps.empty()) {
        auto &frame {frames.emplace_back()};
        frame.exps.insert(exps.begin(), exps.end());
        for (const auto &e: exps) {
            add_initial_lemmas(e);
        }
    }
}

cpp_int value(const Term term) {
    return cpp_int(term->to_string());
}

Term Swine::term(const cpp_int &value) {
    return solver->make_term(value.str(), int_sort);
}

long to_int(const std::string &s) {
    if (s.starts_with("(- ")) {
        return -to_int(s.substr(3, s.size() - 1));
    } else {
        return stol(s);
    }
}

std::optional<Swine::EvaluatedExp> Swine::evaluate(const Term exp_expression) const {
    EvaluatedExp res;
    res.exp_expression = exp_expression;
    res.exp_expression_val = value(get_value(exp_expression));
    auto it {exp_expression->begin()};
    ++it;
    res.base = *it;
    res.base_val = value(get_value(*it));
    ++it;
    res.exponent = *it;
    res.exponent_val = to_int(get_value(*it)->to_string());
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
    const auto tangent {
        make_term(
                    Op(Plus),
                    term(fst_val),
                    make_term(
                        Op(Mult),
                        term(diff),
                        make_term(Op(Minus), e.exponent, term(fst_exponent))))};
    const auto tangent_lemma {
        make_term(
                    Op(Implies),
                    make_term(Op(Ge), e.exponent, term(0)),
                    make_term(Op(Ge), e.exp_expression, tangent))};
    if (log) std::cout << tangent_lemma << std::endl;
    return tangent_lemma;
}

void Swine::tangent_lemmas(const EvaluatedExp &e, TermVec &lemmas) {
    lemmas.push_back(tangent_lemma(e, true));
    if (e.exponent_val > 1) {
        lemmas.push_back(tangent_lemma(e, false));
    }
}

Term Swine::secant_lemma(const EvaluatedExp &e, const long other_exponent_val) {
    const auto other_val {pow(e.base_val, other_exponent_val)};
    const auto factor {other_exponent_val - e.exponent_val};
    const auto secant {
        make_term(
                    Op(Plus),
                    make_term(
                        Op(Mult),
                        term(other_val - e.expected_val),
                        make_term(Op(Minus), e.exponent, term(other_exponent_val))),
                    term(other_val * factor))};
    Term premise;
    if (other_exponent_val <= e.exponent_val) {
        premise = make_term(
                    Op(And),
                    make_term(Op(Ge), e.exponent, term(other_exponent_val)),
                    make_term(Op(Le), e.exponent, term(e.exponent_val)));
    } else {
        premise = make_term(
                    Op(And),
                    make_term(Op(Le), e.exponent, term(other_exponent_val)),
                    make_term(Op(Ge), e.exponent, term(e.exponent_val)));
    }
    const auto res {
        make_term(
                    Op(Implies),
                    premise,
                    make_term(
                        Op(Le),
                        make_term(Op(Mult), term(factor), e.exp_expression),
                        secant))};
    if (log) std::cout << res << std::endl;
    return res;
}

void Swine::secant_lemmas(const EvaluatedExp &e, TermVec &lemmas) {
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
    lemmas.push_back(secant_lemma(e, *prev));
    lemmas.push_back(secant_lemma(e, *next));
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
    const Term strict_exp_premise {
        make_term(
                    Op(Lt),
                    smaller.exponent,
                    greater.exponent)};
    const Term non_strict_exp_premise {
        make_term(
                    Op(Le),
                    smaller.exponent,
                    greater.exponent)};
    if (!smaller.base->is_value() || !greater.base->is_value()) {
        const Term strict_base_premise {
            make_term(
                        Op(Lt),
                        smaller.base,
                        greater.base)};
        const Term non_strict_base_premise {
            make_term(
                        Op(Le),
                        smaller.base,
                        greater.base)};
        premise = make_term(
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
    const auto monotonicity_lemma {
        make_term(
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

void Swine::monotonicity_lemmas(TermVec &lemmas) {
    // search for pairs exp(b,e1), exp(b,e2) whose models violate monotonicity of exp
    for (auto it1 = frames.begin(); it1 != frames.end(); ++it1) {
        for (const auto &exp1: it1->exps) {
            const auto e1 {evaluate(exp1)};
            if (e1 && e1->base_val > 1) {
                for (auto it2 = it1; ++it2 != frames.end();) {
                    for (const auto &exp2: it2->exps) {
                        const auto e2 {evaluate(exp2)};
                        if (e2 && e2->base_val > 1) {
                            const auto mon_lemma {monotonicity_lemma(*e1, *e2)};
                            if (mon_lemma) {
                                lemmas.push_back(*mon_lemma);
                            }
                        }
                    }
                }
            }
        }
    }
}

Result Swine::check_sat() {
    while (true) {
        const auto res {solver->check_sat()};
        if (res.is_unsat()) {
            return res;
        } else if (res.is_unknown()) {
            return res;
        } else if (res.is_sat()) {
            bool sat {true};
            if (log) {
                std::cout << "candidate model:" << std::endl;
                for (const auto &[key,val]: get_model()) {
                    std::cout << key << " = " << val << std::endl;
                }
            }
            TermVec lemmas;
            // check if the model can be lifted, add refinement lemmas otherwise
            for (const auto &f: frames) {
                for (const auto &e: f.exps) {
                    const auto ee {evaluate(e)};
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
                return res;
            }
            monotonicity_lemmas(lemmas);
            for (const auto &l: lemmas) {
                solver->assert_formula(l);
            }
        }
    }
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
        return solver->make_term(Op(Apply), exp, t0, t1);
    } else {
        return solver->make_term(op, t0, t1);
    }
}

Term Swine::make_term(Op op,
                      const Term & t0,
                      const Term & t1,
                      const Term & t2) const {
    return solver->make_term(op, t0, t1, t2);
}

Term Swine::make_term(Op op, const TermVec & terms) const {
    if (op == Op(Exp)) {
        assert(terms.size() == 2);
        return solver->make_term(Op(Apply), exp, terms[0], terms[1]);
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
    std::optional<std::string> input;
    while (++arg < argc) {
        if (boost::iequals(argv[arg], "--solver")) {
            const std::string solver_str {get_next()};
            if (boost::iequals(solver_str, "z3")) {
                solver = smt::Z3SolverFactory::create(false);
            } else if (boost::iequals(solver_str, "cvc5")) {
                solver = smt::Cvc5SolverFactory::create(false);
                //            } else if (boost::iequals(solver_str, "yices")) {
                //                solver = smt::Yices2SolverFactory::create(false);
            } else {
                throw std::invalid_argument("unknown solver " + solver_str);
            }
        } else {
            input = argv[arg];
        }
    }
    if (!solver) {
        solver = smt::Z3SolverFactory::create(false);
    }
    if (!input) {
        throw std::invalid_argument("missing input file");
    }
    SmtSolver swine = std::make_shared<Swine>(solver);
    return SmtLibReader(swine).parse(*input);
}
