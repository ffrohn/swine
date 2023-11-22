#pragma once

#include "util.h"

class Rewriter {

    Util &util;

public:

    Rewriter(Util &util);

    Term rewrite(Term t);

};
