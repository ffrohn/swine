#pragma once

#include "util.h"

namespace swine {

class Rewriter {

    Util &util;

public:

    Rewriter(Util &util);

    Term rewrite(Term t);

};

}
