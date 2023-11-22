#pragma once

#include "util.h"

class TermEvaluator {

    Util &util;

public:

    TermEvaluator(Util &util);

    cpp_int evaluate_int(Term expression) const;

    bool evaluate_bool(Term expression) const;

};
