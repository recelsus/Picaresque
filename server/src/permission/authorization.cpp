#include "picaresque/permission/authorization.hpp"

#include "picaresque/permission/constants.hpp"
#include "picaresque/permission/resolver.hpp"

namespace picaresque::permission {
namespace {

bool CanManageGroup(const User& actor, const std::string& group_id) {
  if (actor.role == Role::Admin) {
    return true;
  }

  const auto resolved = ResolvePermissionForGroup(actor, group_id);
  if (actor.role == Role::Owner) {
    return resolved.source == PermissionSource::OwnedGroupOverride ||
        resolved.write >= kManagerPermission;
  }

  return resolved.write >= kManagerPermission;
}

}  // namespace

bool CanCreateGroup(const User& actor) {
  return actor.role == Role::Admin || actor.role == Role::Owner;
}

bool CanDeleteGroup(const User& actor, const std::string& group_id) {
  if (actor.role == Role::Admin) {
    return true;
  }

  return actor.role == Role::Owner &&
      ResolvePermissionForGroup(actor, group_id).source == PermissionSource::OwnedGroupOverride;
}

bool CanInviteToGroup(const User& actor, const std::string& group_id) {
  return CanManageGroup(actor, group_id);
}

bool CanAssignScopedPermission(
    const User& actor,
    const User& target,
    const std::string& group_id,
    const ScopedPermission& assignment) {
  if (assignment.group_id == "*") {
    return actor.role == Role::Admin;
  }

  if (assignment.group_id != group_id) {
    return false;
  }

  if (!IsMemberOfGroup(target, group_id)) {
    return false;
  }

  if (actor.role == Role::Admin) {
    return true;
  }

  if (!CanManageGroup(actor, group_id)) {
    return false;
  }

  return assignment.read <= kManagerPermission && assignment.write <= kManagerPermission;
}

bool CanAssignOwnerRole(const User& actor) {
  return actor.role == Role::Admin;
}

}  // namespace picaresque::permission
