#pragma once

#include <string>

#include "picaresque/permission/types.hpp"

namespace picaresque::permission {

bool CanCreateGroup(const User& actor);
bool CanDeleteGroup(const User& actor, const std::string& group_id);
bool CanInviteToGroup(const User& actor, const std::string& group_id);
bool CanAssignScopedPermission(
    const User& actor,
    const User& target,
    const std::string& group_id,
    const ScopedPermission& assignment);
bool CanAssignOwnerRole(const User& actor);

}  // namespace picaresque::permission
