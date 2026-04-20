#include "picaresque/access/in_memory_repository.hpp"

namespace picaresque::access {

InMemoryAccessRepository::InMemoryAccessRepository(std::vector<IpRuleRecord> ip_rules)
    : ip_rules_(std::move(ip_rules)) {}

std::vector<IpRuleRecord> InMemoryAccessRepository::ListIpRules() const {
  return ip_rules_;
}

}  // namespace picaresque::access
