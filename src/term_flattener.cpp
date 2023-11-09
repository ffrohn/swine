#include "term_flattener.h"

TermFlattener::TermFlattener(SmtSolver solver, const Term exp): solver(solver), exp(exp) {}

bool is_fun_app(const Term term) {
    return !term->is_symbolic_const() && !term->is_param() && !term->is_symbol() && !term->is_value();
}

Term TermFlattener::next_var(const Term term, UnorderedTermMap &map) {
    static unsigned count {0};
    while (true) {
        try {
            const auto res {solver->make_symbol("swine_flat_" + std::to_string(count), term->get_sort())};
            ++count;
            map.emplace(res, term);
            return res;
        } catch (const IncorrectUsageException &e) {
            ++count;
        }
    }
}

Term TermFlattener::flatten(Term term, UnorderedTermMap &map) {
    if (!is_fun_app(term)) {
        return term;
    }
    TermVec children;
    for (const auto &c: term) {
        children.push_back(flatten(c, map));
    }
    if (term->get_op() == Op(Apply)) {
        auto it {term->begin()};
        if (*it == exp) {
            ++it;
            Term fst_arg;
            if (is_fun_app(*it)) {
                fst_arg = next_var(*it, map);
            } else {
                fst_arg = *it;
            }
            ++it;
            Term snd_arg;
            if (is_fun_app(*it)) {
                snd_arg = next_var(*it, map);
            } else {
                snd_arg = *it;
            }
            const auto res {solver->make_term(Op(Apply), exp, fst_arg, snd_arg)};
            exps.insert(res);
            return res;
        }
    }
    return solver->make_term(term->get_op(), children);
}

Term TermFlattener::flatten(Term term) {
    UnorderedTermMap map;
    TermVec vec;
    vec.push_back(flatten(term, map));
    if (map.empty()) {
        return vec.front();
    }
    for (const auto &[key, val]: map) {
        vec.push_back(solver->make_term(Op(Equal), key, val));
    }
    return solver->make_term(Op(And), vec);
}

UnorderedTermSet TermFlattener::clear_exps() {
    UnorderedTermSet res {exps};
    exps.clear();
    return res;
}
