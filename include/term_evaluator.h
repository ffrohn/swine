#pragma once

#include "util.h"

class TermEvaluator {

    const Util &util;

public:

    TermEvaluator(const Util &util);

    Term evaluate(Term expression) const;

};
