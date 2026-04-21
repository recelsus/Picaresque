#include "picaresque/user/user_management_service.hpp"

#include <algorithm>
#include <stdexcept>

#include "in_memory_state.hpp"

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

bool UserManagementService::IsSetupComplete() const {
  return CountUsers() > 0;
}

std::size_t UserManagementService::CountUsers() const {
  auto& state = GetAppState();
  std::scoped_lock lock(state.mutex);
  return state.users.size();
}

std::vector<UserSummary> UserManagementService::ListUsers(const UserListFilter& filter) const {
  std::vector<UserSummary> users;
  auto& state = GetAppState();
  std::scoped_lock lock(state.mutex);

  for (const auto& entry : state.users) {
    if (filter.role.has_value() && entry.summary.role != *filter.role) {
      continue;
    }

    if (filter.is_active.has_value() && entry.summary.is_active != *filter.is_active) {
      continue;
    }

    users.push_back(entry.summary);
  }

  return users;
}

UserDetails UserManagementService::GetUserDetails(const std::string& user_id) const {
  auto& state = GetAppState();
  std::scoped_lock lock(state.mutex);

  for (const auto& entry : state.users) {
    if (entry.summary.user_id == user_id) {
      return entry;
    }
  }

  throw std::runtime_error("user not found");
}

UserDetails UserManagementService::CreateInitialAdmin(const CreateUserCommand& command) const {
  ValidateCreateUserCommand(command);

  auto& state = GetAppState();
  std::scoped_lock lock(state.mutex);

  if (!state.users.empty()) {
    throw std::runtime_error("setup already completed");
  }

  UserDetails admin{
      .summary =
          {
              .user_id = "user_initial_admin",
              .login_id = command.login_id,
              .user_name = command.user_name,
              .email = command.email,
              .role = permission::Role::Admin,
              .is_active = true,
          },
      .owned_groups = {},
      .scoped_permissions = {
          {"*", 99, 99},
      },
  };

  state.users.push_back(admin);
  return admin;
}

UserDetails UserManagementService::CreateUser(const CreateUserCommand& command) const {
  ValidateCreateUserCommand(command);

  auto& state = GetAppState();
  std::scoped_lock lock(state.mutex);

  if (state.users.empty()) {
    throw std::runtime_error("setup_not_completed");
  }

  const auto duplicate = std::find_if(
      state.users.begin(),
      state.users.end(),
      [&command](const UserDetails& details) {
        return details.summary.login_id == command.login_id || details.summary.email == command.email;
      });
  if (duplicate != state.users.end()) {
    throw std::runtime_error("user_already_exists");
  }

  UserDetails user{
      .summary =
          {
              .user_id = BuildNextUserId(state),
              .login_id = command.login_id,
              .user_name = command.user_name,
              .email = command.email,
              .role = command.role,
              .is_active = true,
          },
      .owned_groups = {},
      .scoped_permissions = {},
  };

  state.users.push_back(user);
  return user;
}

}  // namespace picaresque::user
