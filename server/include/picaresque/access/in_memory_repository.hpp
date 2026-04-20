#pragma once

#include <utility>
#include <vector>

#include "picaresque/access/repository.hpp"

namespace picaresque::access {

class InMemoryAccessRepository : public AccessRepository {
 public:
  explicit InMemoryAccessRepository(std::vector<IpRuleRecord> ip_rules);

  std::vector<IpRuleRecord> ListIpRules() const override;

 private:
  std::vector<IpRuleRecord> ip_rules_;
};

}  // namespace picaresque::access
