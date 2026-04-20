#include "picaresque/permission/resolver.hpp"

#include <algorithm>

#include "picaresque/permission/constants.hpp"

namespace picaresque::permission {
namespace {

const ScopedPermission* FindScopedPermission(const User& user, const std::string& scope_name) {
  const auto it = std::find_if(
      user.scoped_permissions.begin(),
      user.scoped_permissions.end(),
      [&scope_name](const ScopedPermission& permission) {
        return permission.name == scope_name;
      });

  if (it == user.scoped_permissions.end()) {
    return nullptr;
  }

  return &(*it);
}

}  // namespace

bool IsMemberOfGroup(const User& user, const std::string& group_name) {
  if (std::find(user.owned_groups.begin(), user.owned_groups.end(), group_name) !=
      user.owned_groups.end()) {
    return true;
  }

  return FindScopedPermission(user, group_name) != nullptr;
}

ResolvedPermission ResolvePermissionForGroup(const User& user, const std::string& group_name) {
  if (user.role == Role::Admin) {
    return {
        .name = group_name,
        .read = kPrivilegedPermission,
        .write = kPrivilegedPermission,
        .source = PermissionSource::AdminOverride,
    };
  }

  if (user.role == Role::Owner &&
      std::find(user.owned_groups.begin(), user.owned_groups.end(), group_name) !=
          user.owned_groups.end()) {
    return {
        .name = group_name,
        .read = kPrivilegedPermission,
        .write = kPrivilegedPermission,
        .source = PermissionSource::OwnedGroupOverride,
    };
  }

  if (const auto* direct_scope = FindScopedPermission(user, group_name); direct_scope != nullptr) {
    return {
        .name = group_name,
        .read = direct_scope->read,
        .write = direct_scope->write,
        .source = PermissionSource::DirectScope,
    };
  }

  if (const auto* wildcard_scope = FindScopedPermission(user, "*"); wildcard_scope != nullptr) {
    return {
        .name = group_name,
        .read = wildcard_scope->read,
        .write = wildcard_scope->write,
        .source = PermissionSource::WildcardScope,
    };
  }

  return {
      .name = group_name,
      .read = kDefaultPermission,
      .write = kDefaultPermission,
      .source = PermissionSource::DefaultScope,
  };
}

}  // namespace picaresque::permission
