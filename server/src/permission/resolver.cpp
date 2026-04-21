#include "picaresque/permission/resolver.hpp"

#include <algorithm>

#include "picaresque/permission/constants.hpp"

namespace picaresque::permission {
namespace {

const ScopedPermission* FindScopedPermission(const User& user, const std::string& group_id) {
  const auto it = std::find_if(
      user.scoped_permissions.begin(),
      user.scoped_permissions.end(),
      [&group_id](const ScopedPermission& permission) {
        return permission.group_id == group_id;
      });

  if (it == user.scoped_permissions.end()) {
    return nullptr;
  }

  return &(*it);
}

}  // namespace

bool IsMemberOfGroup(const User& user, const std::string& group_id) {
  if (std::find(user.owned_groups.begin(), user.owned_groups.end(), group_id) !=
      user.owned_groups.end()) {
    return true;
  }

  return FindScopedPermission(user, group_id) != nullptr;
}

ResolvedPermission ResolvePermissionForGroup(const User& user, const std::string& group_id) {
  if (user.role == Role::Admin) {
    return {
        .group_id = group_id,
        .read = kPrivilegedPermission,
        .write = kPrivilegedPermission,
        .source = PermissionSource::AdminOverride,
    };
  }

  if (user.role == Role::Owner &&
      std::find(user.owned_groups.begin(), user.owned_groups.end(), group_id) !=
          user.owned_groups.end()) {
    return {
        .group_id = group_id,
        .read = kPrivilegedPermission,
        .write = kPrivilegedPermission,
        .source = PermissionSource::OwnedGroupOverride,
    };
  }

  if (const auto* direct_scope = FindScopedPermission(user, group_id); direct_scope != nullptr) {
    return {
        .group_id = group_id,
        .read = direct_scope->read,
        .write = direct_scope->write,
        .source = PermissionSource::DirectScope,
    };
  }

  if (const auto* wildcard_scope = FindScopedPermission(user, "*"); wildcard_scope != nullptr) {
    return {
        .group_id = group_id,
        .read = wildcard_scope->read,
        .write = wildcard_scope->write,
        .source = PermissionSource::WildcardScope,
    };
  }

  return {
      .group_id = group_id,
      .read = kDefaultPermission,
      .write = kDefaultPermission,
      .source = PermissionSource::DefaultScope,
  };
}

}  // namespace picaresque::permission
