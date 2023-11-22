#pragma once

#include "util.h"

class ConstantPropagator {

    Util &util;

public:

    ConstantPropagator(Util &util);

    Term propagate(Term expression) const;

};
