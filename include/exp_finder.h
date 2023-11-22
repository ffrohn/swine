#pragma once

#include "util.h"

class ExpFinder {

    Util &util;

    void find_exps(Term term, UnorderedTermSet &res);

public:

    ExpFinder(Util &util);

    UnorderedTermSet find_exps(Term term);

};
