#include "swine.h"

#include <smt-switch/smtlib_reader.h>
#include <boost/multiprecision/cpp_int.hpp>

using namespace boost::multiprecision;

int main(int argc, char *argv[]) {
}

Swine::Swine(const SmtSolver solver):
    AbsSmtSolver(solver->get_solver_enum()),
    solver(solver),
    exp(solver->make_symbol("exp", solver->make_sort(SortKind::INT, solver->make_sort(SortKind::INT), solver->make_sort(SortKind::INT)))),
    flattener(solver, exp) {}

void Swine::set_opt(const std::string option, const std::string value) {
    solver->set_opt(option, value);
}

void Swine::set_logic(const std::string logic) {
    solver->set_logic(logic);
}

void Swine::assert_formula(const Term & t) {
    const auto flat {flattener.flatten(t)};
    solver->assert_formula(t);
    const auto exps {flattener.clear_exps()};
    if (!exps.empty()) {
        auto &set {this->exps.emplace(level, UnorderedTermSet()).first->second};
        set.insert(exps.begin(), exps.end());
        for (const auto &e: exps) {
            auto it {e->begin()};
            ++it;
            const auto base {*it};
            ++it;
            const auto exp {*it};
            const auto zero {solver->make_term(0, solver->make_sort(SortKind::INT))};
            const auto one {solver->make_term(1, solver->make_sort(SortKind::INT))};
            const auto pos {solver->make_term(Op(Gt), exp, zero)};
            // TODO add each lemma only once?
            // exp = 0 ==> base^exp = 1
            solver->assert_formula(
                        solver->make_term(
                            Op(Implies),
                            solver->make_term(Op(Equal), exp, zero),
                            solver->make_term(Op(Equal), e, one)));
            // exp > 0 && base = 0 ==> base^exp = 0
            solver->assert_formula(
                        solver->make_term(
                            Op(Implies),
                            solver->make_term(
                                Op(And),
                                pos,
                                solver->make_term(Op(Equal), base, zero)),
                            solver->make_term(Op(Equal), e, zero)));
            // exp > 0 && base = 1 ==> base^exp = 1
            solver->assert_formula(
                        solver->make_term(
                            Op(Implies),
                            solver->make_term(
                                Op(And),
                                pos,
                                solver->make_term(Op(Equal), base, one)),
                            solver->make_term(Op(Equal), e, one)));
            // exp > 0 && base > 1 ==> base^exp > exp
            solver->assert_formula(
                        solver->make_term(
                            Op(Implies),
                            solver->make_term(
                                Op(And),
                                pos,
                                solver->make_term(Op(Gt), base, one)),
                            solver->make_term(Op(Gt), e, exp)));
        }
    }
}

Term Swine::secant_lemma(const Term expected, const Term expected_val, const Term exponent, const Term exponent_val, const Term other_exponent, const Term other_exponent_val) {
    const auto factor {solver->make_term(Op(Minus), other_exponent, exponent_val)};
    const auto secant {
        solver->make_term(
                    Op(Plus),
                    solver->make_term(
                        Op(Mult),
                        solver->make_term(Op(Minus), other_exponent_val, expected_val),
                        solver->make_term(Op(Minus), exponent, other_exponent)),
                    solver->make_term(Op(Mult), other_exponent_val, factor))};
    Term premise;
    if (cpp_int(other_exponent->to_string()) <= cpp_int(exponent_val->to_string())) {
        premise = solver->make_term(
                    Op(And),
                    solver->make_term(Op(Ge), exponent, other_exponent),
                    solver->make_term(Op(Le), exponent, exponent_val));
    } else {
        premise = solver->make_term(
                    Op(And),
                    solver->make_term(Op(Le), exponent, other_exponent),
                    solver->make_term(Op(Ge), exponent, exponent_val));
    }
    return solver->make_term(
                Op(Implies),
                premise,
                solver->make_term(
                    Op(Le),
                    solver->make_term(Op(Mult), factor, expected),
                    secant));
}

Term Swine::extra_refinement(const Term exponent1, const Term exponent2, const Term expected1, const Term expected2) {
    return solver->make_term(
                Op(Implies),
                solver->make_term(
                    Op(And),
                    solver->make_term(
                        Op(Le),
                        solver->make_term(0, solver->make_sort(SortKind::INT)),
                        exponent1),
                    solver->make_term(Op(Lt), exponent1, exponent2)),
                solver->make_term(Op(Lt), expected1, expected2));
}

Result Swine::check_sat() {
    const auto zero {solver->make_term(0, solver->make_sort(SortKind::INT))};
    const auto one {solver->make_term(1, solver->make_sort(SortKind::INT))};
    const auto int_sort {solver->make_sort(SortKind::INT)};
    std::unordered_map<Term, std::vector<cpp_int>> secant_points;
    while (true) {
        const auto res {solver->check_sat()};
        if (res.is_unsat()) {
            if (log) std::cout << "unsat" << std::endl;
            return res;
        } else if (res.is_unknown()) {
            if (log) std::cout << "unknown" << std::endl;
            return res;
        } else if (res.is_sat()) {
            bool sat {true};
            // check if the model can be lifted, add refinement lemmas otherwise
            for (const auto &[_,es]: exps) {
                for (const auto &e: es) {
                    auto it {e->begin()};
                    ++it;
                    const auto base {*it};
                    const cpp_int ginac_base {base->to_string()};
                    ++it;
                    const auto exp {*it};
                    const auto exp_val {solver->get_value(exp)};
                    // TODO catch exception
                    const ulong ginac_exp_val {exp_val->to_int()};
                    // if the exponent is negative, integer exponentiation is undefined
                    // in smtlib, this means that the result can be arbitrary
                    if (ginac_exp_val >= 0) {
                        const cpp_int ginac_actual {solver->get_value(e)->to_string()};
                        const auto ginac_expected {pow(ginac_base, ginac_exp_val)};
                        const auto expected {solver->make_term(Op(Pow), base, exp_val)};
                        if (ginac_actual < ginac_expected) {
                            if (log) std::cout << "tangent refinement" << std::endl;
                            // the actual value is too small --> add "tangent" refinement lemmas
                            const auto next {solver->make_term(Op(Mult), expected, base)};
                            const auto diff {solver->make_term(Op(Minus), next, expected)};
                            // a line through the current and its successor
                            const auto tangent {
                                solver->make_term(
                                            Op(Plus),
                                            expected,
                                            solver->make_term(
                                                Op(Mult),
                                                diff,
                                                solver->make_term(Op(Minus), exp, exp_val)))};
                            if (log) std::cout << solver->make_term(Op(Ge), e, tangent) << std::endl;
                            solver->assert_formula(
                                        solver->make_term(
                                            Op(Implies),
                                            solver->make_term(Op(Ge), exp, zero),
                                            solver->make_term(Op(Ge), e, tangent)));
                            if (ginac_actual > 1) {
                                // as the actual result is greater than one, expected / base is an integer
                                const auto prev {solver->make_term(Op(IntDiv), expected, base)};
                                auto diff {solver->make_term(Op(Minus), expected, prev)};
                                // a line through the current value and its predecessor
                                auto tangent {
                                    solver->make_term(
                                                Op(Plus),
                                                prev,
                                                solver->make_term(
                                                    Op(Mult),
                                                    diff,
                                                    solver->make_term(
                                                        Op(Minus),
                                                        solver->make_term(Op(Minus), exp, exp_val),
                                                        one)))};
                                if (log) std::cout << solver->make_term(Op(Ge), e, tangent) << std::endl;
                                solver->assert_formula(
                                            solver->make_term(
                                                Op(Implies),
                                                solver->make_term(Op(Ge), exp, zero),
                                                solver->make_term(Op(Ge), e, tangent)));
                            }
                            sat = false;
                        } else if (ginac_actual > ginac_expected) {
                            if (log) std::cout << "secant refinement" << std::endl;
                            // the actual value is too large --> add secant refinement lemmas
                            cpp_int ginac_prev {-1};
                            cpp_int ginac_next {-1};
                            Term prev;
                            Term next;
                            // look for the closest existing secant points
                            auto it {secant_points.emplace(e, std::vector<cpp_int>()).first};
                            for (const auto &ginac_other: it->second) {
                                if (ginac_other < ginac_exp_val) {
                                    if (ginac_other > ginac_prev) {
                                        ginac_prev = ginac_other;
                                        prev = solver->make_term(ginac_other.str(), int_sort);
                                    }
                                } else if (ginac_next < 0 || ginac_other < ginac_next) {
                                    ginac_next = ginac_other;
                                    next = solver->make_term(ginac_other.str(), int_sort);
                                }
                            }
                            // if there are none, use the neighbours
                            if (ginac_prev < 0) {
                                prev = solver->make_term(Op(Minus), exp_val, one);
                            }
                            if (ginac_next < 0) {
                                next = solver->make_term(Op(Plus), exp_val, one);
                            }
                            // store the current secant point
                            it->second.emplace_back(ginac_exp_val);
                            const auto prev_val {solver->make_term(Op(Pow), base, prev)};
                            const auto next_val {solver->make_term(Op(Pow), base, next)};
                            const auto prev_secant_lemma {secant_lemma(e, expected, exp, exp_val, prev, prev_val)};
                            const auto next_secant_lemma {secant_lemma(e, expected, exp, exp_val, next, next_val)};
                            if (log) std::cout << prev_secant_lemma << std::endl;
                            if (log) std::cout << next_secant_lemma << std::endl;
                            solver->assert_formula(prev_secant_lemma);
                            solver->assert_formula(next_secant_lemma);
                            sat = false;
                        }
                    }
                }
            }
            if (sat) {
                if (log) std::cout << "sat" << std::endl;
                return res;
            } else {
                // search for pairs exp(b,e1), exp(b,e2) whose models violate monotonicity of exp
                auto outer_it1 {exps.begin()};
                while (outer_it1 != exps.end()) {
                    auto inner_it1 {outer_it1->second.begin()};
                    while (inner_it1 != outer_it1->second.end()) {
                        const auto e1 {*inner_it1};
                        auto it1 {e1->begin()};
                        ++it1;
                        const auto base1 {*it1};
                        const cpp_int ginac_base1 {base1->to_string()};
                        if (ginac_base1 > 1) {
                            ++it1;
                            const auto exp1 {*it1};
                            auto outer_it2 = outer_it1;
                            while (outer_it2 != exps.end()) {
                                auto inner_it2 {outer_it2->second.begin()};
                                if (outer_it1 == outer_it2) {
                                    inner_it2 = inner_it1;
                                    ++inner_it2;
                                }
                                while (inner_it2 != outer_it2->second.end()) {
                                    const auto e2 {*inner_it2};
                                    auto it2 {e2->begin()};
                                    ++it2;
                                    const auto base2 {*it2};
                                    const cpp_int ginac_base2 {base2->to_string()};
                                    if (ginac_base1 == ginac_base2) {
                                        ++it2;
                                        const auto exp2 {*it2};
                                        const cpp_int ginac_exp1 {solver->get_value(exp1)->to_string()};
                                        const cpp_int ginac_exp2 {solver->get_value(exp2)->to_string()};
                                        const cpp_int ginac_e1 {solver->get_value(e1)->to_string()};
                                        const cpp_int ginac_e2 {solver->get_value(e2)->to_string()};
                                        if (ginac_exp1 < ginac_exp2 && ginac_e1 >= ginac_e2) {
                                            if (log) std::cout << "extra refinement 1" << std::endl;
                                            solver->assert_formula(extra_refinement(exp1, exp2, e1, e2));
                                        } else if (ginac_exp2 < ginac_exp1 && ginac_e2 >= ginac_e1) {
                                            if (log) std::cout << "extra refinement 2" << std::endl;
                                            solver->assert_formula(extra_refinement(exp2, exp1, e2, e1));
                                        }
                                    }
                                    ++inner_it2;
                                }
                                ++outer_it2;
                            }
                        }
                        ++inner_it1;
                    }
                    ++outer_it1;
                }
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
    level += num;
}

void Swine::pop(uint64_t num) {
    solver->pop(num);
    for (unsigned i = 0; i < num; ++i) {
        exps.erase(level);
        --level;
    }
}

uint64_t Swine::get_context_level() const {
    return solver->get_context_level();
}

Term Swine::get_value(const Term & t) const {
    return solver->get_value(t);
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
    return solver->make_symbol(name, sort);
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
    return solver->make_term(op, t0, t1);
}

Term Swine::make_term(Op op,
               const Term & t0,
               const Term & t1,
               const Term & t2) const {
    return solver->make_term(op, t0, t1, t2);
}

Term Swine::make_term(Op op, const TermVec & terms) const {
    return solver->make_term(op, terms);
}

void Swine::reset() {
    solver->reset();
}

void Swine::reset_assertions() {
    solver->reset_assertions();
}

Term Swine::substitute(const Term term,
                const UnorderedTermMap & substitution_map) const {
    return solver->substitute(term, substitution_map);
}

void Swine::dump_smt2(std::string filename) const {
    solver->dump_smt2(filename);
}
