#pragma once

#include "util.h"

class TermEvaluator {

    Util &util;

public:

    TermEvaluator(Util &util);

    Term evaluate(Term expression) const;

};
