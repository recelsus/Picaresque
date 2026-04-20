#include <cassert>
#include <vector>

#include "picaresque/access/api.hpp"

namespace access = picaresque::access;

int main() {
  const std::int64_t now = 1'800'000'000;

  const access::InMemoryAccessRepository repository(
      {
          {
              .id = 1,
              .value_text = "10.0.0.0/8",
              .address_family = access::AddressFamily::IPv4,
              .rule_type = access::IpRuleType::Cidr,
              .prefix_length = 8,
              .effect = access::RuleEffect::Allow,
              .enabled = true,
              .surface = std::nullopt,
          },
          {
              .id = 2,
              .value_text = "10.2.3.4",
              .address_family = access::AddressFamily::IPv4,
              .rule_type = access::IpRuleType::Single,
              .prefix_length = 32,
              .effect = access::RuleEffect::Deny,
              .enabled = true,
              .surface = std::nullopt,
          },
          {
              .id = 3,
              .value_text = "192.168.0.0/16",
              .address_family = access::AddressFamily::IPv4,
              .rule_type = access::IpRuleType::Cidr,
              .prefix_length = 16,
              .effect = access::RuleEffect::Allow,
              .enabled = true,
              .surface = access::AccessSurface::Web,
          },
      });

  const access::AccessPolicyService deny_by_default_service(
      repository,
      {
          .web_default_policy = access::DefaultPolicy::Deny,
          .rest_api_default_policy = access::DefaultPolicy::Deny,
      });

  const access::AccessPolicyService mixed_default_service(
      repository,
      {
          .web_default_policy = access::DefaultPolicy::Allow,
          .rest_api_default_policy = access::DefaultPolicy::Deny,
      });

  assert(access::IsValidIpAddress("127.0.0.1"));
  assert(access::IsValidIpAddress("2001:db8::1"));
  assert(!access::IsValidIpAddress("invalid-ip"));

  {
    const auto result = deny_by_default_service.Evaluate(
        {
            .client_ip = "10.0.1.2",
            .surface = access::AccessSurface::Web,
        },
        now);
    assert(result.decision == access::AccessDecision::Allow);
    assert(result.reason == "ip_allow");
    assert(result.matched_ip_rule_id == 1);
  }

  {
    const auto result = deny_by_default_service.Evaluate(
        {
            .client_ip = "10.2.3.4",
            .surface = access::AccessSurface::Web,
        },
        now);
    assert(result.decision == access::AccessDecision::Deny);
    assert(result.reason == "ip_deny");
    assert(result.matched_ip_rule_id == 2);
  }

  {
    const auto result = mixed_default_service.Evaluate(
        {
            .client_ip = "203.0.113.9",
            .surface = access::AccessSurface::Web,
        },
        now);
    assert(result.decision == access::AccessDecision::Allow);
    assert(result.reason == "default_allow");
  }

  {
    const auto result = mixed_default_service.Evaluate(
        {
            .client_ip = "203.0.113.9",
            .surface = access::AccessSurface::RestApi,
        },
        now);
    assert(result.decision == access::AccessDecision::Deny);
    assert(result.reason == "default_deny");
  }

  {
    const auto result = deny_by_default_service.Evaluate(
        {
            .client_ip = "invalid-ip",
            .surface = access::AccessSurface::Web,
        },
        now);
    assert(result.decision == access::AccessDecision::Deny);
    assert(result.reason == "invalid_client_ip");
  }

  return 0;
}
