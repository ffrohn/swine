#include "preprocessor.h"

namespace swine {

Preprocessor::Preprocessor(Util &util): util(util), rewriter(util), constant_propagator(util) {}

Term Preprocessor::preprocess(Term term) {
    const auto log = [&](const Term term, const PreprocKind kind, const std::function<Term(Term)> &f){
        bool done {false};
        const auto res {f(term)};
        if (util.config.log && res != term) {
            if (!done) {
                std::cout << "preprocessing" << std::endl;
                std::cout << "original term:" << std::endl;
                std::cout << term << std::endl;
                done = true;
            }
            std::cout << kind << ":" << std::endl;
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
    const auto do_cp {[&](const Term term) {
            if (util.config.is_active(PreprocKind::ConstantFolding)) {
                return log(term, PreprocKind::ConstantFolding, cp);
            } else {
                return term;
            }
        }
    };
    const auto do_rw {[&](const Term term) {
            if (util.config.is_active(PreprocKind::Rewriting)) {
                return log(term, PreprocKind::Rewriting, rw);
            } else {
                return term;
            }
        }
    };
    auto last {term};
    auto cterm {do_cp(term)};
    auto rterm {do_rw(cterm)};
    auto res {rterm};
    while (res != last) {
        last = res;
        if (res != cterm) {
            cterm = do_cp(res);
            res = cterm;
        }
        if (res != rterm) {
            rterm = do_rw(res);
            res = rterm;
        }
    }
    if (util.config.log && term != res) std::cout << "preprocessing finished" << std::endl;
    return res;
}

}
