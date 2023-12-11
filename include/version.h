#pragma once

#include <string>

namespace swine {

struct Version {
  static const std::string GIT_SHA;
  static const std::string GIT_DIRTY;
};

}
