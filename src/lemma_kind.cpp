#include "lemma_kind.h"

#include <stdexcept>
#include <boost/algorithm/string.hpp>

namespace swine {

std::string lemma_kind::str(const LemmaKind k) {
    switch (k) {
    case LemmaKind::Symmetry: return "symmetry";
    case LemmaKind::Modulo: return "modulo";
    case LemmaKind::Bounding: return "bounding";
    case LemmaKind::Interpolation: return "interpolation";
    case LemmaKind::Monotonicity: return "monotonicity";
    default: throw std::invalid_argument("unknown lemma kind");
    }
}

std::ostream& operator<<(std::ostream &s, const LemmaKind kind) {
    return s << lemma_kind::str(kind);
}

}
