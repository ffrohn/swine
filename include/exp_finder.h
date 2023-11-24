#pragma once

#include "util.h"

class ExpGroup {

    const Term t;
    Util &util;

    const bool ground_base;
    const bool neg_base;

public:

    ExpGroup(const Term t, Util &util);

    TermVec all() const;
    TermVec maybe_non_neg_base() const;
    TermVec sym() const;
    bool has_ground_base() const;
    Term orig() const;

};

class ExpFinder {

public:

    Util &util;

    void find_exps(Term term, UnorderedTermSet &res);

public:

    ExpFinder(Util &util);

    std::vector<ExpGroup> find_exps(Term term);

};
