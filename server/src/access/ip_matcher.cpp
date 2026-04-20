#include "picaresque/access/ip_matcher.hpp"

#include <arpa/inet.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace picaresque::access {
namespace {

struct ParsedIp {
  AddressFamily family;
  std::array<std::uint8_t, 16> bytes;
};

std::optional<ParsedIp> ParseIp(std::string_view ip_text) {
  std::array<std::uint8_t, 16> bytes{};

  if (inet_pton(AF_INET, std::string(ip_text).c_str(), bytes.data()) == 1) {
    return ParsedIp{
        .family = AddressFamily::IPv4,
        .bytes = bytes,
    };
  }

  if (inet_pton(AF_INET6, std::string(ip_text).c_str(), bytes.data()) == 1) {
    return ParsedIp{
        .family = AddressFamily::IPv6,
        .bytes = bytes,
    };
  }

  return std::nullopt;
}

bool IsSurfaceApplicable(const std::optional<AccessSurface>& rule_surface, AccessSurface request_surface) {
  return !rule_surface.has_value() || *rule_surface == request_surface;
}

int PrefixLengthForRule(const IpRuleRecord& rule) {
  if (rule.prefix_length.has_value()) {
    return *rule.prefix_length;
  }

  return rule.rule_type == IpRuleType::Single
      ? (rule.address_family == AddressFamily::IPv4 ? 32 : 128)
      : 0;
}

bool BitsMatch(
    const std::array<std::uint8_t, 16>& left,
    const std::array<std::uint8_t, 16>& right,
    int prefix_length) {
  const int full_bytes = prefix_length / 8;
  const int partial_bits = prefix_length % 8;

  if (full_bytes > 0 &&
      std::memcmp(left.data(), right.data(), static_cast<std::size_t>(full_bytes)) != 0) {
    return false;
  }

  if (partial_bits == 0) {
    return true;
  }

  const auto mask = static_cast<std::uint8_t>(0xFFu << (8 - partial_bits));
  return (left[full_bytes] & mask) == (right[full_bytes] & mask);
}

bool IpMatchesRule(const ParsedIp& client_ip, const IpRuleRecord& rule) {
  if (client_ip.family != rule.address_family) {
    return false;
  }

  const auto host_text = std::string_view(rule.value_text).substr(0, rule.value_text.find('/'));
  const auto rule_ip = ParseIp(host_text);
  if (!rule_ip.has_value()) {
    return false;
  }

  return BitsMatch(client_ip.bytes, rule_ip->bytes, PrefixLengthForRule(rule));
}

}  // namespace

bool IsValidIpAddress(std::string_view ip_text) {
  return ParseIp(ip_text).has_value();
}

std::optional<IpRuleRecord> SelectMostSpecificIpMatch(
    std::string_view client_ip,
    RuleEffect effect,
    AccessSurface surface,
    const std::vector<IpRuleRecord>& rules) {
  const auto parsed_client_ip = ParseIp(client_ip);
  if (!parsed_client_ip.has_value()) {
    return std::nullopt;
  }

  std::optional<IpRuleRecord> best_rule;
  int best_prefix = -1;

  for (const auto& rule : rules) {
    if (!rule.enabled || rule.effect != effect || !IsSurfaceApplicable(rule.surface, surface)) {
      continue;
    }

    if (!IpMatchesRule(*parsed_client_ip, rule)) {
      continue;
    }

    const int current_prefix = PrefixLengthForRule(rule);
    if (!best_rule.has_value() || current_prefix > best_prefix ||
        (current_prefix == best_prefix && rule.id < best_rule->id)) {
      best_rule = rule;
      best_prefix = current_prefix;
    }
  }

  return best_rule;
}

}  // namespace picaresque::access
