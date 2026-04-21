#include <cassert>
#include <stdexcept>

#include "picaresque/auth/auth_service.hpp"
#include "picaresque/user/in_memory_user_group_repository.hpp"
#include "picaresque/user/user_management_service.hpp"

namespace auth = picaresque::auth;
namespace user = picaresque::user;
namespace permission = picaresque::permission;

int main() {
  user::InMemoryUserGroupRepository repository;
  const user::UserManagementService user_service(repository);
  const auth::AuthService auth_service(repository);

  const auto created_user = user_service.CreateInitialAdmin({
      .login_id = "admin",
      .user_name = "Initial Admin",
      .email = "admin@example.local",
      .password = "change-me",
      .role = permission::Role::Admin,
  });

  assert(!auth_service.GetUserApiKey(created_user.summary.user_id).has_value());

  const auto issued = auth_service.IssueUserApiKey(created_user.summary.user_id);
  assert(!issued.plain_api_key.empty());
  assert(issued.info.user_id == created_user.summary.user_id);
  assert(!issued.info.key_prefix.empty());

  const auto authenticated = auth_service.AuthenticateApiKey(issued.plain_api_key);
  assert(authenticated.summary.user_id == created_user.summary.user_id);

  const auto rotated = auth_service.IssueUserApiKey(created_user.summary.user_id);
  assert(rotated.plain_api_key != issued.plain_api_key);

  bool old_key_rejected = false;
  try {
    static_cast<void>(auth_service.AuthenticateApiKey(issued.plain_api_key));
  } catch (const std::runtime_error&) {
    old_key_rejected = true;
  }
  assert(old_key_rejected);

  auth_service.RevokeUserApiKey(created_user.summary.user_id);
  assert(!auth_service.GetUserApiKey(created_user.summary.user_id).has_value());

  bool revoked_key_rejected = false;
  try {
    static_cast<void>(auth_service.AuthenticateApiKey(rotated.plain_api_key));
  } catch (const std::runtime_error&) {
    revoked_key_rejected = true;
  }
  assert(revoked_key_rejected);

  return 0;
}
