#pragma once

#include "picaresque/user/user_group_repository.hpp"

namespace picaresque::user {

UserGroupRepository& GetMySqlUserGroupRepository();

}  // namespace picaresque::user
