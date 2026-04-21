#include "picaresque/user/user_management_service.hpp"

#include <stdexcept>

namespace picaresque::user {
namespace {

void ValidateCreateUserCommand(const CreateUserCommand& command) {
  if (command.login_id.empty()) {
    throw std::runtime_error("login_id is required");
  }

  if (command.user_name.empty()) {
    throw std::runtime_error("user_name is required");
  }

  if (command.email.empty()) {
    throw std::runtime_error("email is required");
  }

  if (command.password.empty()) {
    throw std::runtime_error("password is required");
  }
}

}  // namespace

UserManagementService::UserManagementService(UserGroupRepository& repository) : repository_(repository) {}

bool UserManagementService::IsSetupComplete() const {
  return CountUsers() > 0;
}

std::size_t UserManagementService::CountUsers() const {
  return repository_.CountUsers();
}

std::vector<UserSummary> UserManagementService::ListUsers(const UserListFilter& filter) const {
  return repository_.ListUsers(filter);
}

UserDetails UserManagementService::GetUserDetails(const std::string& user_id) const {
  const auto details = repository_.FindUserDetailsById(user_id);
  if (!details.has_value()) {
    throw std::runtime_error("user not found");
  }
  return *details;
}

UserDetails UserManagementService::CreateInitialAdmin(const CreateUserCommand& command) const {
  ValidateCreateUserCommand(command);

  if (repository_.CountUsers() > 0) {
    throw std::runtime_error("setup already completed");
  }

  return repository_.CreateUser(
      command,
      {
          {"*", 99, 99},
      });
}

UserDetails UserManagementService::CreateUser(const CreateUserCommand& command) const {
  ValidateCreateUserCommand(command);

  if (repository_.CountUsers() == 0) {
    throw std::runtime_error("setup_not_completed");
  }

  if (repository_.UserExistsByLoginIdOrEmail(command.login_id, command.email)) {
    throw std::runtime_error("user_already_exists");
  }

  return repository_.CreateUser(command, {});
}

}  // namespace picaresque::user
