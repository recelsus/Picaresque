#pragma once

#include "picaresque/permission/types.hpp"

namespace picaresque::permission {

void ValidateScopedPermission(const ScopedPermission& permission);
void ValidateAccessRequirement(const AccessRequirement& requirement);
void ValidateUser(const User& user);

}  // namespace picaresque::permission
