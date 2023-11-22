#include "exp_finder.h"

ExpFinder::ExpFinder(Util &util): util(util) {}

void ExpFinder::find_exps(Term term, UnorderedTermSet &res) {
    if (util.is_app(term)) {
        for (auto c: term) {
            find_exps(c, res);
        }
        if (util.is_abstract_exp(term)) {
            res.insert(term);
        }
    }
}

UnorderedTermSet ExpFinder::find_exps(Term term) {
    UnorderedTermSet res;
    find_exps(term, res);
    return res;
}
