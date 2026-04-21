#pragma once

#include <string>

#include "picaresque/auth/auth_types.hpp"
#include "picaresque/user/user_group_repository.hpp"
#include "picaresque/user/user_types.hpp"

namespace picaresque::auth {

class AuthService {
 public:
  explicit AuthService(user::UserGroupRepository& repository);

  std::optional<ApiKeyInfo> GetUserApiKey(const std::string& user_id) const;
  IssuedApiKey IssueUserApiKey(const std::string& user_id) const;
  void RevokeUserApiKey(const std::string& user_id) const;
  user::UserDetails AuthenticateApiKey(const std::string& api_key) const;

 private:
  user::UserGroupRepository& repository_;
};

std::string HashApiKey(std::string_view api_key);

}  // namespace picaresque::auth
