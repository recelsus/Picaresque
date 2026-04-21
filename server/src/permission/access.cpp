#include "picaresque/permission/access.hpp"

#include "picaresque/permission/resolver.hpp"

namespace picaresque::permission {
namespace {

PermissionCheckResult EvaluateRequirements(
    const User& user,
    const std::vector<AccessRequirement>& requirements,
    bool require_read,
    bool require_write) {
  if (requirements.empty()) {
    return {
        .status = PermissionCheckStatus::Allow,
        .reason = PermissionCheckReason::NoRequirement,
    };
  }

  std::optional<ResolvedPermission> last_resolved_permission;

  for (const auto& requirement : requirements) {
    const auto resolved = ResolvePermissionForGroup(user, requirement.group_id);
    last_resolved_permission = resolved;
    const bool read_ok = !require_read || resolved.read >= requirement.read;
    const bool write_ok = !require_write || resolved.write >= requirement.write;

    if (read_ok && write_ok) {
      return {
          .status = PermissionCheckStatus::Allow,
          .reason = PermissionCheckReason::RequirementSatisfied,
          .matched_requirement = requirement,
          .resolved_permission = resolved,
      };
    }
  }

  return {
      .status = PermissionCheckStatus::Deny,
      .reason = PermissionCheckReason::RequirementNotSatisfied,
      .resolved_permission = last_resolved_permission,
  };
}

}  // namespace

PermissionCheckResult EvaluateRead(
    const User& user,
    const std::vector<AccessRequirement>& requirements) {
  return EvaluateRequirements(user, requirements, true, false);
}

PermissionCheckResult EvaluateWrite(
    const User& user,
    const std::vector<AccessRequirement>& requirements) {
  return EvaluateRequirements(user, requirements, false, true);
}

PermissionCheckResult EvaluateReadAndWrite(
    const User& user,
    const std::vector<AccessRequirement>& requirements) {
  return EvaluateRequirements(user, requirements, true, true);
}

bool CanRead(const User& user, const std::vector<AccessRequirement>& requirements) {
  return EvaluateRead(user, requirements).IsAllowed();
}

bool CanWrite(const User& user, const std::vector<AccessRequirement>& requirements) {
  return EvaluateWrite(user, requirements).IsAllowed();
}

bool CanReadAndWrite(const User& user, const std::vector<AccessRequirement>& requirements) {
  return EvaluateReadAndWrite(user, requirements).IsAllowed();
}

}  // namespace picaresque::permission
