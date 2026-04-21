#pragma once

#include <optional>
#include <vector>

#include "picaresque/permission/types.hpp"

namespace picaresque::permission {

enum class PermissionCheckStatus {
  Allow,
  Deny,
};

enum class PermissionCheckReason {
  NoRequirement,
  RequirementSatisfied,
  RequirementNotSatisfied,
};

struct PermissionCheckResult {
  PermissionCheckStatus status = PermissionCheckStatus::Deny;
  PermissionCheckReason reason = PermissionCheckReason::RequirementNotSatisfied;
  std::optional<AccessRequirement> matched_requirement;
  std::optional<ResolvedPermission> resolved_permission;

  bool IsAllowed() const {
    return status == PermissionCheckStatus::Allow;
  }
};

PermissionCheckResult EvaluateRead(
    const User& user,
    const std::vector<AccessRequirement>& requirements);
PermissionCheckResult EvaluateWrite(
    const User& user,
    const std::vector<AccessRequirement>& requirements);
PermissionCheckResult EvaluateReadAndWrite(
    const User& user,
    const std::vector<AccessRequirement>& requirements);

bool CanRead(const User& user, const std::vector<AccessRequirement>& requirements);
bool CanWrite(const User& user, const std::vector<AccessRequirement>& requirements);
bool CanReadAndWrite(const User& user, const std::vector<AccessRequirement>& requirements);

}  // namespace picaresque::permission
