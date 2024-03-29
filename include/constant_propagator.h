#pragma once

#include "util.h"

namespace swine {

class ConstantPropagator {

    Util &util;

public:

    ConstantPropagator(Util &util);

    Term propagate(Term expression) const;

};

}
