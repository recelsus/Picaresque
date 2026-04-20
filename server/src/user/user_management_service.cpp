#include "picaresque/user/user_management_service.hpp"

#include <algorithm>
#include <mutex>
#include <stdexcept>

namespace picaresque::user {
namespace {

struct InMemoryUserStore {
  mutable std::mutex mutex;
  std::vector<UserDetails> users;
};

InMemoryUserStore& GetStore() {
  static InMemoryUserStore store;
  return store;
}

std::string BuildInitialUserId() {
  return "user_initial_admin";
}

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
  auto& store = GetStore();
  std::scoped_lock lock(store.mutex);
  return store.users.size();
}

std::vector<UserSummary> UserManagementService::ListUsers(const UserListFilter& filter) const {
  std::vector<UserSummary> users;
  auto& store = GetStore();
  std::scoped_lock lock(store.mutex);

  for (const auto& entry : store.users) {
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
  auto& store = GetStore();
  std::scoped_lock lock(store.mutex);

  for (const auto& entry : store.users) {
    if (entry.summary.user_id == user_id) {
      return entry;
    }
  }

  throw std::runtime_error("user not found");
}

UserDetails UserManagementService::CreateInitialAdmin(const CreateUserCommand& command) const {
  ValidateCreateUserCommand(command);

  auto& store = GetStore();
  std::scoped_lock lock(store.mutex);

  if (!store.users.empty()) {
    throw std::runtime_error("setup already completed");
  }

  UserDetails admin{
      .summary =
          {
              .user_id = BuildInitialUserId(),
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

  store.users.push_back(admin);
  return admin;
}

}  // namespace picaresque::user
