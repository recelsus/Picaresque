#include "picaresque/access/policy.hpp"

#include "picaresque/access/ip_matcher.hpp"

namespace picaresque::access {

AccessPolicyService::AccessPolicyService(
    const AccessRepository& repository,
    AccessConfiguration configuration)
    : repository_(repository), configuration_(configuration) {}

AccessDecisionResult AccessPolicyService::Evaluate(
    const AccessRequestContext& request_context,
    std::int64_t /*now_unix_seconds*/) const {
  if (!IsValidIpAddress(request_context.client_ip)) {
    return {
        .decision = AccessDecision::Deny,
        .reason = "invalid_client_ip",
    };
  }

  const auto ip_rules = repository_.ListIpRules();
  const auto deny_match = SelectMostSpecificIpMatch(
      request_context.client_ip,
      RuleEffect::Deny,
      request_context.surface,
      ip_rules);
  if (deny_match.has_value()) {
    return {
        .decision = AccessDecision::Deny,
        .reason = "ip_deny",
        .matched_ip_rule_id = deny_match->id,
    };
  }

  const auto allow_match = SelectMostSpecificIpMatch(
      request_context.client_ip,
      RuleEffect::Allow,
      request_context.surface,
      ip_rules);
  if (allow_match.has_value()) {
    return {
        .decision = AccessDecision::Allow,
        .reason = "ip_allow",
        .matched_ip_rule_id = allow_match->id,
    };
  }

  if (ResolveDefaultPolicy(request_context.surface) == DefaultPolicy::Allow) {
    return {
        .decision = AccessDecision::Allow,
        .reason = "default_allow",
    };
  }

  return {
      .decision = AccessDecision::Deny,
      .reason = "default_deny",
  };
}

DefaultPolicy AccessPolicyService::ResolveDefaultPolicy(AccessSurface surface) const {
  return surface == AccessSurface::Web
      ? configuration_.web_default_policy
      : configuration_.rest_api_default_policy;
}

}  // namespace picaresque::access
