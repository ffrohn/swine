#include "brute_force.h"

#include <smt-switch/cvc5_factory.h>
#include <smt-switch/solver.h>

BruteForce::BruteForce(SmtSolver solver, const smt::TermVec &assertions, const smt::TermVec &exps): solver(solver), assertions(assertions), exps(exps) {
    solver->reset_assertions();
}

bool BruteForce::next(const ulong weight, std::vector<std::pair<Term, ulong>>::iterator begin) {
    auto it = begin;
    if (++it == current.end()) {
        return false;
    } else if (next(weight - begin->second, it)) {
        return true;
    } else if (begin->second == 0) {
        return false;
    } else {
        begin->second--;
        it->second = weight - begin->second;
        while (++it != current.end()) {
            it->second = 0;
        }
        return true;
    }
}

bool BruteForce::next() {
    if (!next(bound, current.begin())) {
        if (bound == max_bound) {
            return false;
        }
        ++bound;
        auto it {current.begin()};
        it->second = bound;
        while (++it != current.end()) {
            it->second = 0;
        }
    }
    return true;
}

bool BruteForce::check_sat() {
    for (const auto &a: assertions) {
        solver->assert_formula(a);
    }
    for (const auto &e: exps) {
        current.emplace_back(e, 0);
    }
    const auto int_sort {solver->make_sort(SortKind::INT)};
    do {
        solver->push();
        for (const auto &[exp, val]: current) {
            auto it {exp->begin()};
            const auto base {*(++it)};
            const auto exponent {*(++it)};
            const auto exp_eq {solver->make_term(
                Op(Equal),
                exponent,
                solver->make_term(val, int_sort))};
            solver->assert_formula(exp_eq);
            const auto res_eq {solver->make_term(
                Op(Equal),
                exp,
                solver->make_term(
                    Op(Pow),
                    base,
                    solver->make_term(val, int_sort)))};
            solver->assert_formula(res_eq);
        }
        const auto res {solver->check_sat()};
        if (res.is_sat()) {
            return true;
        }
        solver->pop();
    } while (next());
    return false;
}
