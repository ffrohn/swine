#pragma once

#include "util.h"

namespace swine {

class TermEvaluator {

    const Util &util;

public:

    TermEvaluator(const Util &util);

    Term evaluate(Term expression) const;

};

}
