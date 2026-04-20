#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace picaresque::permission {

enum class Role {
  Admin,
  Owner,
  Member,
};

enum class PermissionSource {
  AdminOverride,
  OwnedGroupOverride,
  DirectScope,
  WildcardScope,
  DefaultScope,
};

struct ScopedPermission {
  std::string name;
  std::uint8_t read = 0;
  std::uint8_t write = 0;
};

struct User {
  std::string user_id;
  std::string user_name;
  Role role = Role::Member;
  std::vector<std::string> owned_groups;
  std::vector<ScopedPermission> scoped_permissions;
};

struct ResolvedPermission {
  std::string name;
  std::uint8_t read = 0;
  std::uint8_t write = 0;
  PermissionSource source = PermissionSource::DefaultScope;
};

struct AccessRequirement {
  std::string name;
  std::uint8_t read = 0;
  std::uint8_t write = 0;
};

}  // namespace picaresque::permission
