#pragma once

#include <vector>

#include "picaresque/user/user_types.hpp"

namespace picaresque::user {

class UserManagementService {
 public:
  bool IsSetupComplete() const;
  std::size_t CountUsers() const;
  std::vector<UserSummary> ListUsers(const UserListFilter& filter) const;
  UserDetails GetUserDetails(const std::string& user_id) const;
  UserDetails CreateInitialAdmin(const CreateUserCommand& command) const;
};

}  // namespace picaresque::user
