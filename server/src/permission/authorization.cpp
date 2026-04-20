#include "picaresque/permission/authorization.hpp"

#include "picaresque/permission/constants.hpp"
#include "picaresque/permission/resolver.hpp"

namespace picaresque::permission {
namespace {

bool CanManageGroup(const User& actor, const std::string& group_name) {
  if (actor.role == Role::Admin) {
    return true;
  }

  const auto resolved = ResolvePermissionForGroup(actor, group_name);
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

bool CanDeleteGroup(const User& actor, const std::string& group_name) {
  if (actor.role == Role::Admin) {
    return true;
  }

  return actor.role == Role::Owner &&
      ResolvePermissionForGroup(actor, group_name).source == PermissionSource::OwnedGroupOverride;
}

bool CanInviteToGroup(const User& actor, const std::string& group_name) {
  return CanManageGroup(actor, group_name);
}

bool CanAssignScopedPermission(
    const User& actor,
    const User& target,
    const std::string& group_name,
    const ScopedPermission& assignment) {
  if (assignment.name == "*") {
    return actor.role == Role::Admin;
  }

  if (assignment.name != group_name) {
    return false;
  }

  if (!IsMemberOfGroup(target, group_name)) {
    return false;
  }

  if (actor.role == Role::Admin) {
    return true;
  }

  if (!CanManageGroup(actor, group_name)) {
    return false;
  }

  return assignment.read <= kManagerPermission && assignment.write <= kManagerPermission;
}

bool CanAssignOwnerRole(const User& actor) {
  return actor.role == Role::Admin;
}

}  // namespace picaresque::permission
