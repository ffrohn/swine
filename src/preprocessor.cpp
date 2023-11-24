#include "preprocessor.h"

Preprocessor::Preprocessor(Util &util): util(util), rewriter(util), constant_propagator(util) {}

Term Preprocessor::exp_to_pow(Term term) {
    if (!util.is_app(term)) {
        return term;
    }
    TermVec children;
    for (const auto &c: term) {
        children.push_back(exp_to_pow(c));
    }
    if (util.is_abstract_exp(term)) {
        const auto exp {children.at(2)};
        if (exp->is_value()) {
            const auto val {util.value(exp)};
            if (val == 0) {
                return util.term(1);
            } else if (val == 1 || (val == -1 && util.config.semantics == Total)) {
                return children.at(1);
            } else if (val > 1 || util.config.semantics == Total) {
                return util.solver->make_term(Pow, children.at(1), util.term(abs(util.value(exp))));
            }
        }
    }
    return util.solver->make_term(term->get_op(), children);
}

Term Preprocessor::preprocess(Term term) {
    const auto log = [&](const Term term, const std::string &transformation, const std::function<Term(Term)> &f){
        static bool done {false};
        const auto res {f(term)};
        if (util.config.log && res != term) {
            if (!done) {
                std::cout << "preprocessing" << std::endl;
                std::cout << "original term:" << std::endl;
                std::cout << term << std::endl;
                done = true;
            }
            std::cout << transformation << ":" << std::endl;
            std::cout << res << std::endl;
        }
        return res;
    };
    const auto cp {[&](const Term term) {
            return constant_propagator.propagate(term);
        }
    };
    const auto rw {[&](const Term term) {
            return rewriter.rewrite(term);
        }
    };
    const auto ep {[&](const Term term) {
            return exp_to_pow(term);
        }
    };
    const auto do_cp {[&](const Term term) {
            return log(term, "constant folding", cp);
        }
    };
    const auto do_rw {[&](const Term term) {
            return log(term, "rewriting", rw);
        }
    };
    const auto do_ep {[&](const Term term) {
            return log(term, "exp to pow", ep);
        }
    };
    auto last {term};
    auto cterm {do_cp(term)};
    auto eterm {do_ep(cterm)};
    auto rterm {do_rw(eterm)};
    auto res {rterm};
    while (res != last) {
        last = res;
        if (res != cterm) {
            cterm = do_cp(res);
            res = cterm;
        }
        if (res != eterm) {
            eterm = do_ep(res);
            res = eterm;
        }
        if (res != rterm) {
            rterm = do_rw(res);
            res = rterm;
        }
    }
    return res;
}
