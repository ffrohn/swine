#pragma once

#include "rewriter.h"
#include "constant_propagator.h"

namespace swine {

class Preprocessor {

    Util &util;
    Rewriter rewriter;
    ConstantPropagator constant_propagator;

public:

    Preprocessor(Util &util);

    Term preprocess(Term term);

};

}
