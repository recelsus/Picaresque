#include "picaresque/permission/validation.hpp"

#include <unordered_set>

#include "picaresque/permission/constants.hpp"
#include "picaresque/permission/errors.hpp"

namespace picaresque::permission {
namespace {

void ValidateName(const std::string& name, const char* field_name) {
  if (name.empty()) {
    throw ValidationError(std::string(field_name) + " must not be empty");
  }
}

void ValidateReadWrite(std::uint8_t read, std::uint8_t write, const char* label) {
  if (read == 0 || write == 0) {
    throw ValidationError(std::string(label) + " must not contain 0");
  }

  if (read > kPrivilegedPermission || write > kPrivilegedPermission) {
    throw ValidationError(std::string(label) + " must be within 1-99");
  }

  if (read < write) {
    throw ValidationError(std::string(label) + " must satisfy read >= write");
  }
}

}  // namespace

void ValidateScopedPermission(const ScopedPermission& permission) {
  ValidateName(permission.name, "scope name");
  ValidateReadWrite(permission.read, permission.write, "scoped permission");
}

void ValidateAccessRequirement(const AccessRequirement& requirement) {
  ValidateName(requirement.name, "requirement name");
  ValidateReadWrite(requirement.read, requirement.write, "access requirement");
}

void ValidateUser(const User& user) {
  if (user.user_id.empty()) {
    throw ValidationError("user_id must not be empty");
  }

  if (user.user_name.empty()) {
    throw ValidationError("user_name must not be empty");
  }

  std::unordered_set<std::string> owned_group_names;
  for (const auto& owned_group : user.owned_groups) {
    ValidateName(owned_group, "owned group");
    if (!owned_group_names.insert(owned_group).second) {
      throw ValidationError("owned_groups must not contain duplicates");
    }
  }

  std::unordered_set<std::string> scoped_names;
  for (const auto& scope : user.scoped_permissions) {
    ValidateScopedPermission(scope);
    if (!scoped_names.insert(scope.name).second) {
      throw ValidationError("scoped_permissions must not contain duplicate names");
    }
    if (scope.name == "*" && user.role != Role::Admin) {
      throw ValidationError("wildcard scope can only be assigned to admin users");
    }
  }
}

}  // namespace picaresque::permission
