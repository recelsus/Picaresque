#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "picaresque/access/types.hpp"

namespace picaresque::access {

bool IsValidIpAddress(std::string_view ip_text);
std::optional<IpRuleRecord> SelectMostSpecificIpMatch(
    std::string_view client_ip,
    RuleEffect effect,
    AccessSurface surface,
    const std::vector<IpRuleRecord>& rules);

}  // namespace picaresque::access
