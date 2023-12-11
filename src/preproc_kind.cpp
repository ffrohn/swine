#include "preproc_kind.h"

#include <stdexcept>
#include <boost/algorithm/string.hpp>

namespace swine {

std::string preproc_kind::str(const PreprocKind k) {
    switch (k) {
    case PreprocKind::ConstantFolding: return "constant-folding";
    case PreprocKind::Rewriting: return "rewriting";
    default: throw std::invalid_argument("unknown preprocessing");
    }
}

std::ostream& operator<<(std::ostream &s, const PreprocKind kind) {
    return s << preproc_kind::str(kind);
}

}
