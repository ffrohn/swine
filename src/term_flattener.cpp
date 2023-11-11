#include "term_flattener.h"

TermFlattener::TermFlattener(SmtSolver solver, Util &util): solver(solver), util(util) {}

bool is_fun_app(const Term term) {
    return !term->is_symbolic_const() && !term->is_param() && !term->is_symbol() && !term->is_value();
}

Term TermFlattener::replacement_var(const Term term) {
    static unsigned count {0};
    const auto it {map.find(term)};
    if (it != map.end()) {
        return it->second;
    }
    while (true) {
        try {
            const auto res {solver->make_symbol("swine_flat_" + std::to_string(count), term->get_sort())};
            ++count;
            map.emplace(term, res);
            return res;
        } catch (const IncorrectUsageException &e) {
            ++count;
        }
    }
}

Term TermFlattener::flatten(Term term, UnorderedTermSet &set) {
    if (!is_fun_app(term)) {
        return term;
    }
    TermVec children;
    for (const auto &c: term) {
        children.push_back(flatten(c, set));
    }
    if (term->get_op() == Op(Apply)) {
        auto it {term->begin()};
        if (*it == util.exp) {
            Term fst_arg;
            ++it;
            if ((*it)->is_symbol() || (*it)->is_value()) {
                fst_arg = *it;
            } else {
                set.insert(*it);
                fst_arg = replacement_var(*it);
            }
            Term snd_arg;
            if ((*(++it))->is_symbol()) {
                snd_arg = *it;
            } else {
                set.insert(*it);
                snd_arg = replacement_var(*it);
            }
            const auto res {solver->make_term(Op(Apply), util.exp, fst_arg, snd_arg)};
            exps.insert(res);
            return res;
        }
    }
    return solver->make_term(term->get_op(), children);
}

Term TermFlattener::flatten(Term term) {
    UnorderedTermSet set;
    TermVec vec;
    vec.push_back(flatten(term, set));
    if (set.empty()) {
        return vec.front();
    }
    for (const auto &key: set) {
        vec.push_back(solver->make_term(Op(Equal), key, map.at(key)));
    }
    return solver->make_term(Op(And), vec);
}

UnorderedTermSet TermFlattener::clear_exps() {
    UnorderedTermSet res {exps};
    exps.clear();
    return res;
}
