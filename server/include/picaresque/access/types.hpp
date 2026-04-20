#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace picaresque::access {

enum class DefaultPolicy {
  Deny,
  Allow,
};

enum class AccessSurface {
  Web,
  RestApi,
};

enum class AccessDecision {
  Allow,
  Deny,
};

enum class RuleEffect {
  Allow,
  Deny,
};

enum class AddressFamily {
  IPv4,
  IPv6,
};

enum class IpRuleType {
  Single,
  Cidr,
};

struct AccessConfiguration {
  DefaultPolicy web_default_policy = DefaultPolicy::Deny;
  DefaultPolicy rest_api_default_policy = DefaultPolicy::Deny;
};

struct AccessRequestContext {
  std::string client_ip;
  AccessSurface surface = AccessSurface::Web;
};

struct AccessDecisionResult {
  AccessDecision decision = AccessDecision::Deny;
  std::string reason;
  std::optional<std::int64_t> matched_ip_rule_id;
};

struct IpRuleRecord {
  std::int64_t id = 0;
  std::string value_text;
  AddressFamily address_family = AddressFamily::IPv4;
  IpRuleType rule_type = IpRuleType::Single;
  std::optional<int> prefix_length;
  RuleEffect effect = RuleEffect::Deny;
  bool enabled = true;
  std::optional<AccessSurface> surface;
  std::optional<std::string> note;
};

}  // namespace picaresque::access
