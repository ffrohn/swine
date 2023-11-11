#pragma once

#include "util.h"

class TermEvaluator {

    Util &util;

public:

    TermEvaluator(Util &util);

    std::optional<cpp_int> evaluate_ground_int(Term expression) const;

    cpp_int evaluate_int(Term expression) const;

    bool evaluate_bool(Term expression) const;

};
