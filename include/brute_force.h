#pragma once

#include "util.h"

#include <smt-switch/term.h>
#include <map>

namespace swine {

using namespace smt;

class BruteForce {
    Util &util;
    TermVec assertions;
    TermVec exps;
    unsigned long bound {0};
    std::vector<std::pair<Term, unsigned long>> current;

    bool next();
    bool next(const unsigned long weight, std::vector<std::pair<Term, unsigned long>>::iterator begin);

public:

    BruteForce(Util &util, const TermVec &assertions, const TermVec &exps);
    bool check_sat();

};

}
