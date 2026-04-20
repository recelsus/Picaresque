#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "picaresque/permission/types.hpp"

namespace picaresque::user {

struct UserSummary {
  std::string user_id;
  std::string login_id;
  std::string user_name;
  std::string email;
  permission::Role role = permission::Role::Member;
  bool is_active = true;
};

struct UserDetails {
  UserSummary summary;
  std::vector<std::string> owned_groups;
  std::vector<permission::ScopedPermission> scoped_permissions;
};

struct CreateUserCommand {
  std::string login_id;
  std::string user_name;
  std::string email;
  std::string password;
  permission::Role role = permission::Role::Member;
};

struct UpdateUserRoleCommand {
  std::string user_id;
  permission::Role role = permission::Role::Member;
};

struct UpsertScopedPermissionCommand {
  std::string user_id;
  permission::ScopedPermission scoped_permission;
};

struct UserListFilter {
  std::optional<permission::Role> role;
  std::optional<bool> is_active;
};

}  // namespace picaresque::user
