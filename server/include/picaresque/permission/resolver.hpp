#pragma once

#include <string>

#include "picaresque/permission/types.hpp"

namespace picaresque::permission {

bool IsMemberOfGroup(const User& user, const std::string& group_id);
ResolvedPermission ResolvePermissionForGroup(const User& user, const std::string& group_id);

}  // namespace picaresque::permission
