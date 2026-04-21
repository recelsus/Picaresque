#include "picaresque/user/in_memory_user_group_repository.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace picaresque::user {
namespace {

bool HasScopedPermissionForGroup(const UserDetails& details, const std::string& group_id) {
  return std::any_of(
      details.scoped_permissions.begin(),
      details.scoped_permissions.end(),
      [&group_id](const permission::ScopedPermission& scoped_permission) {
        return scoped_permission.group_id == group_id;
      });
}

void UpsertScopedPermissionInUser(
    UserDetails& details,
    const permission::ScopedPermission& scoped_permission) {
  const auto it = std::find_if(
      details.scoped_permissions.begin(),
      details.scoped_permissions.end(),
      [&scoped_permission](const permission::ScopedPermission& current) {
        return current.group_id == scoped_permission.group_id;
      });

  if (it == details.scoped_permissions.end()) {
    details.scoped_permissions.push_back(scoped_permission);
    return;
  }

  *it = scoped_permission;
}

}  // namespace

std::size_t InMemoryUserGroupRepository::CountUsers() const {
  std::scoped_lock lock(mutex_);
  return users_.size();
}

std::vector<UserSummary> InMemoryUserGroupRepository::ListUsers(const UserListFilter& filter) const {
  std::vector<UserSummary> users;
  std::scoped_lock lock(mutex_);

  for (const auto& entry : users_) {
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

std::optional<UserDetails> InMemoryUserGroupRepository::FindUserDetailsById(
    const std::string& user_id) const {
  std::scoped_lock lock(mutex_);
  const auto it = std::find_if(
      users_.begin(),
      users_.end(),
      [&user_id](const UserDetails& details) { return details.summary.user_id == user_id; });
  if (it == users_.end()) {
    return std::nullopt;
  }
  return *it;
}

std::optional<UserDetails> InMemoryUserGroupRepository::FindUserDetailsByApiKeyHash(
    const std::string& key_hash) const {
  std::scoped_lock lock(mutex_);
  const auto hash_it = std::find_if(
      api_key_hashes_.begin(),
      api_key_hashes_.end(),
      [&key_hash](const auto& entry) { return entry.second == key_hash; });
  if (hash_it == api_key_hashes_.end()) {
    return std::nullopt;
  }
  const auto user_it = std::find_if(
      users_.begin(),
      users_.end(),
      [&hash_it](const UserDetails& details) { return details.summary.user_id == hash_it->first; });
  if (user_it == users_.end()) {
    return std::nullopt;
  }
  return *user_it;
}

bool InMemoryUserGroupRepository::UserExistsByLoginIdOrEmail(
    const std::string& login_id,
    const std::string& email) const {
  std::scoped_lock lock(mutex_);
  return std::any_of(
      users_.begin(),
      users_.end(),
      [&login_id, &email](const UserDetails& details) {
        return details.summary.login_id == login_id || details.summary.email == email;
      });
}

std::optional<auth::ApiKeyInfo> InMemoryUserGroupRepository::FindApiKeyInfoByUserId(
    const std::string& user_id) const {
  std::scoped_lock lock(mutex_);
  const auto it = std::find_if(
      api_keys_.begin(),
      api_keys_.end(),
      [&user_id](const auth::ApiKeyInfo& api_key) { return api_key.user_id == user_id; });
  if (it == api_keys_.end()) {
    return std::nullopt;
  }
  return *it;
}

UserDetails InMemoryUserGroupRepository::CreateUser(
    const CreateUserCommand& command,
    const std::vector<permission::ScopedPermission>& initial_scoped_permissions) {
  std::scoped_lock lock(mutex_);

  UserDetails user{
      .summary =
          {
              .user_id = BuildNextUserId(),
              .login_id = command.login_id,
              .user_name = command.user_name,
              .email = command.email,
              .role = command.role,
              .is_active = true,
          },
      .owned_groups = {},
      .scoped_permissions = initial_scoped_permissions,
  };

  users_.push_back(user);
  return user;
}

auth::ApiKeyInfo InMemoryUserGroupRepository::UpsertApiKey(
    const std::string& user_id,
    const std::string& key_prefix,
    const std::string& key_hash) {
  std::scoped_lock lock(mutex_);

  const auto user_it = std::find_if(
      users_.begin(),
      users_.end(),
      [&user_id](const UserDetails& details) { return details.summary.user_id == user_id; });
  if (user_it == users_.end()) {
    throw std::runtime_error("user_not_found");
  }

  const auth::ApiKeyInfo info{
      .user_id = user_id,
      .key_prefix = key_prefix,
      .enabled = true,
  };

  const auto info_it = std::find_if(
      api_keys_.begin(),
      api_keys_.end(),
      [&user_id](const auth::ApiKeyInfo& api_key) { return api_key.user_id == user_id; });
  if (info_it == api_keys_.end()) {
    api_keys_.push_back(info);
  } else {
    *info_it = info;
  }

  const auto hash_it = std::find_if(
      api_key_hashes_.begin(),
      api_key_hashes_.end(),
      [&user_id](const auto& entry) { return entry.first == user_id; });
  if (hash_it == api_key_hashes_.end()) {
    api_key_hashes_.push_back({user_id, key_hash});
  } else {
    hash_it->second = key_hash;
  }

  return info;
}

void InMemoryUserGroupRepository::DeleteApiKey(const std::string& user_id) {
  std::scoped_lock lock(mutex_);
  api_keys_.erase(
      std::remove_if(
          api_keys_.begin(),
          api_keys_.end(),
          [&user_id](const auth::ApiKeyInfo& api_key) { return api_key.user_id == user_id; }),
      api_keys_.end());
  api_key_hashes_.erase(
      std::remove_if(
          api_key_hashes_.begin(),
          api_key_hashes_.end(),
          [&user_id](const auto& entry) { return entry.first == user_id; }),
      api_key_hashes_.end());
}

std::optional<group::GroupSummary> InMemoryUserGroupRepository::FindGroupSummaryById(
    const std::string& group_id) const {
  std::scoped_lock lock(mutex_);
  const auto it = std::find_if(
      groups_.begin(),
      groups_.end(),
      [&group_id](const group::GroupSummary& group) { return group.group_id == group_id; });
  if (it == groups_.end()) {
    return std::nullopt;
  }
  return *it;
}

bool InMemoryUserGroupRepository::GroupExistsByName(const std::string& group_name) const {
  std::scoped_lock lock(mutex_);
  return std::any_of(
      groups_.begin(),
      groups_.end(),
      [&group_name](const group::GroupSummary& group) { return group.group_name == group_name; });
}

bool InMemoryUserGroupRepository::HasActiveMembership(
    const std::string& group_id,
    const std::string& user_id) const {
  std::scoped_lock lock(mutex_);
  const auto it = std::find_if(
      users_.begin(),
      users_.end(),
      [&user_id](const UserDetails& details) { return details.summary.user_id == user_id; });
  if (it == users_.end()) {
    return false;
  }

  return std::find(it->owned_groups.begin(), it->owned_groups.end(), group_id) != it->owned_groups.end() ||
      HasScopedPermissionForGroup(*it, group_id);
}

std::optional<group::GroupInvitation> InMemoryUserGroupRepository::FindInvitationById(
    const std::string& invitation_id) const {
  std::scoped_lock lock(mutex_);
  const auto it = std::find_if(
      invitations_.begin(),
      invitations_.end(),
      [&invitation_id](const group::GroupInvitation& invitation) {
        return invitation.invitation_id == invitation_id;
      });
  if (it == invitations_.end()) {
    return std::nullopt;
  }
  return *it;
}

bool InMemoryUserGroupRepository::HasPendingInvitation(
    const std::string& group_id,
    const std::string& invited_user_id) const {
  std::scoped_lock lock(mutex_);
  return std::any_of(
      invitations_.begin(),
      invitations_.end(),
      [&group_id, &invited_user_id](const group::GroupInvitation& invitation) {
        return invitation.group_id == group_id &&
            invitation.invited_user_id == invited_user_id &&
            invitation.status == group::InvitationStatus::Pending;
      });
}

group::GroupDetails InMemoryUserGroupRepository::CreateGroup(
    const group::CreateGroupCommand& command,
    const UserDetails& actor) {
  std::scoped_lock lock(mutex_);

  group::GroupSummary summary{
      .group_id = BuildNextGroupId(),
      .group_name = command.group_name,
      .description = command.description,
      .created_by_user_id = actor.summary.user_id,
  };
  groups_.push_back(summary);

  auto* actor_user = FindMutableUserById(actor.summary.user_id);
  if (actor_user == nullptr) {
    throw std::runtime_error("user_not_found");
  }

  if (actor.summary.role == permission::Role::Owner &&
      std::find(actor_user->owned_groups.begin(), actor_user->owned_groups.end(), summary.group_id) ==
          actor_user->owned_groups.end()) {
    actor_user->owned_groups.push_back(summary.group_id);
  }

  if (!HasScopedPermissionForGroup(*actor_user, summary.group_id) &&
      std::find(actor_user->owned_groups.begin(), actor_user->owned_groups.end(), summary.group_id) ==
          actor_user->owned_groups.end()) {
    actor_user->scoped_permissions.push_back({summary.group_id, 10, 10});
  }

  return BuildGroupDetails(summary.group_id);
}

group::GroupInvitation InMemoryUserGroupRepository::CreateInvitation(
    const group::InviteUserCommand& command) {
  std::scoped_lock lock(mutex_);

  group::GroupInvitation invitation{
      .invitation_id = BuildNextInvitationId(),
      .group_id = command.group_id,
      .invited_user_id = command.invited_user_id,
      .invited_by_user_id = command.actor_user_id,
      .status = group::InvitationStatus::Pending,
  };
  invitations_.push_back(invitation);
  return invitation;
}

group::GroupInvitation InMemoryUserGroupRepository::AcceptInvitation(
    const group::AcceptInvitationCommand& command) {
  std::scoped_lock lock(mutex_);

  auto* invitation = FindMutableInvitationById(command.invitation_id);
  if (invitation == nullptr) {
    throw std::runtime_error("invitation_not_found");
  }

  auto* actor = FindMutableUserById(command.actor_user_id);
  if (actor == nullptr) {
    throw std::runtime_error("user_not_found");
  }

  invitation->status = group::InvitationStatus::Accepted;
  if (!HasScopedPermissionForGroup(*actor, invitation->group_id) &&
      std::find(actor->owned_groups.begin(), actor->owned_groups.end(), invitation->group_id) ==
          actor->owned_groups.end()) {
    actor->scoped_permissions.push_back({invitation->group_id, 10, 10});
  }

  return *invitation;
}

permission::ScopedPermission InMemoryUserGroupRepository::UpsertScopedPermission(
    const std::string& user_id,
    const permission::ScopedPermission& scoped_permission) {
  std::scoped_lock lock(mutex_);
  auto* user = FindMutableUserById(user_id);
  if (user == nullptr) {
    throw std::runtime_error("user_not_found");
  }

  UpsertScopedPermissionInUser(*user, scoped_permission);
  return scoped_permission;
}

group::GroupDetails InMemoryUserGroupRepository::AssignOwnerGroup(
    const group::AssignOwnerGroupCommand& command,
    const UserDetails&) {
  std::scoped_lock lock(mutex_);
  auto* target = FindMutableUserById(command.target_user_id);
  if (target == nullptr) {
    throw std::runtime_error("user_not_found");
  }

  if (target->summary.role != permission::Role::Admin) {
    target->summary.role = permission::Role::Owner;
  }

  if (std::find(target->owned_groups.begin(), target->owned_groups.end(), command.group_id) ==
      target->owned_groups.end()) {
    target->owned_groups.push_back(command.group_id);
  }

  return BuildGroupDetails(command.group_id);
}

std::string InMemoryUserGroupRepository::BuildNextUserId() {
  std::ostringstream stream;
  stream << "user_" << next_user_id_++;
  return stream.str();
}

std::string InMemoryUserGroupRepository::BuildNextGroupId() {
  std::ostringstream stream;
  stream << "group_" << next_group_id_++;
  return stream.str();
}

std::string InMemoryUserGroupRepository::BuildNextInvitationId() {
  std::ostringstream stream;
  stream << "invitation_" << next_invitation_id_++;
  return stream.str();
}

group::GroupDetails InMemoryUserGroupRepository::BuildGroupDetails(const std::string& group_id) const {
  const auto group_it = std::find_if(
      groups_.begin(),
      groups_.end(),
      [&group_id](const group::GroupSummary& group) { return group.group_id == group_id; });
  if (group_it == groups_.end()) {
    throw std::runtime_error("group_not_found");
  }

  group::GroupDetails details{.summary = *group_it};
  for (const auto& user : users_) {
    if (std::find(user.owned_groups.begin(), user.owned_groups.end(), group_id) !=
        user.owned_groups.end()) {
      details.owner_user_ids.push_back(user.summary.user_id);
    }

    if (std::find(user.owned_groups.begin(), user.owned_groups.end(), group_id) !=
            user.owned_groups.end() ||
        HasScopedPermissionForGroup(user, group_id)) {
      details.member_user_ids.push_back(user.summary.user_id);
    }
  }

  return details;
}

UserDetails* InMemoryUserGroupRepository::FindMutableUserById(const std::string& user_id) {
  const auto it = std::find_if(
      users_.begin(),
      users_.end(),
      [&user_id](const UserDetails& details) { return details.summary.user_id == user_id; });
  return it == users_.end() ? nullptr : &(*it);
}

group::GroupSummary* InMemoryUserGroupRepository::FindMutableGroupById(const std::string& group_id) {
  const auto it = std::find_if(
      groups_.begin(),
      groups_.end(),
      [&group_id](const group::GroupSummary& group) { return group.group_id == group_id; });
  return it == groups_.end() ? nullptr : &(*it);
}

group::GroupInvitation* InMemoryUserGroupRepository::FindMutableInvitationById(
    const std::string& invitation_id) {
  const auto it = std::find_if(
      invitations_.begin(),
      invitations_.end(),
      [&invitation_id](const group::GroupInvitation& invitation) {
        return invitation.invitation_id == invitation_id;
      });
  return it == invitations_.end() ? nullptr : &(*it);
}

}  // namespace picaresque::user
