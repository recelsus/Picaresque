#pragma once

#include <vector>

#include "picaresque/access/types.hpp"

namespace picaresque::access {

class AccessRepository {
 public:
  virtual ~AccessRepository() = default;

  virtual std::vector<IpRuleRecord> ListIpRules() const = 0;
};

}  // namespace picaresque::access
