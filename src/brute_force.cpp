#include "brute_force.h"

#include <smt-switch/cvc5_factory.h>
#include <smt-switch/solver.h>

BruteForce::BruteForce(Util &util, const smt::TermVec &assertions, const smt::TermVec &exps): util(util), assertions(assertions), exps(exps) {
    util.solver->reset_assertions();
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
        util.solver->assert_formula(a);
    }
    for (const auto &e: exps) {
        current.emplace_back(e, 0);
    }
    const auto int_sort {util.solver->make_sort(SortKind::INT)};
    do {
        if (util.config.log) {
            std::cout << "brute force:";
            for (const auto &c: current) {
                std::cout << " " << c.first << "=" << c.second;
            }
            std::cout << std::endl;
        }
        util.solver->push();
        for (const auto &[exp, val]: current) {
            auto it {exp->begin()};
            const auto base {*(++it)};
            const auto exponent {*(++it)};
            const auto exp_eq {util.solver->make_term(
                Op(Equal),
                exponent,
                util.solver->make_term(val, int_sort))};
            util.solver->assert_formula(exp_eq);
            const auto res_eq {util.solver->make_term(
                Op(Equal),
                exp,
                util.solver->make_term(
                    Op(Pow),
                    base,
                    util.solver->make_term(val, int_sort)))};
            util.solver->assert_formula(res_eq);
        }
        const auto res {util.solver->check_sat()};
        if (res.is_sat()) {
            return true;
        }
        util.solver->pop();
    } while (next());
    return false;
}
