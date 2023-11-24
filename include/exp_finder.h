#pragma once

#include "util.h"

class ExpFinder {

    Util &util;

    void find_exps(Term term, std::unordered_map<Term, bool> &res);

public:

    ExpFinder(Util &util);

    std::unordered_map<Term, bool> find_exps(Term term);

};
