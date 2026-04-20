#pragma once

#include <vector>

#include "picaresque/permission/types.hpp"

namespace picaresque::permission {

bool CanRead(const User& user, const std::vector<AccessRequirement>& requirements);
bool CanWrite(const User& user, const std::vector<AccessRequirement>& requirements);
bool CanReadAndWrite(const User& user, const std::vector<AccessRequirement>& requirements);

}  // namespace picaresque::permission
