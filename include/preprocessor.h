#pragma once

#include "rewriter.h"
#include "constant_propagator.h"

class Preprocessor {

    Util &util;
    Rewriter rewriter;
    ConstantPropagator constant_propagator;

public:

    Preprocessor(Util &util);

    Term preprocess(Term term);

    Term exp_to_pow(Term term);

};
