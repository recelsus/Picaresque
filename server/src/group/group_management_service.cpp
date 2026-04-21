#include "picaresque/group/group_management_service.hpp"

#include <algorithm>
#include <stdexcept>

#include "picaresque/permission/api.hpp"
#include "picaresque/user/user_management_service.hpp"
#include "../user/in_memory_state.hpp"

namespace picaresque::group {
namespace {

using picaresque::user::GetAppState;

permission::User BuildPermissionUser(const user::UserDetails& details) {
  return {
      .user_id = details.summary.user_id,
      .user_name = details.summary.user_name,
      .role = details.summary.role,
      .owned_groups = details.owned_groups,
      .scoped_permissions = details.scoped_permissions,
  };
}

user::UserDetails* FindUser(std::vector<user::UserDetails>& users, const std::string& user_id) {
  const auto it = std::find_if(
      users.begin(),
      users.end(),
      [&user_id](const user::UserDetails& details) { return details.summary.user_id == user_id; });
  return it == users.end() ? nullptr : &(*it);
}

const GroupSummary* FindGroup(const std::vector<GroupSummary>& groups, const std::string& group_id) {
  const auto it = std::find_if(
      groups.begin(),
      groups.end(),
      [&group_id](const GroupSummary& group) { return group.group_id == group_id; });
  return it == groups.end() ? nullptr : &(*it);
}

group::GroupInvitation* FindInvitation(
    std::vector<group::GroupInvitation>& invitations,
    const std::string& invitation_id) {
  const auto it = std::find_if(
      invitations.begin(),
      invitations.end(),
      [&invitation_id](const group::GroupInvitation& invitation) {
        return invitation.invitation_id == invitation_id;
      });
  return it == invitations.end() ? nullptr : &(*it);
}

bool HasGroupScopedPermission(const user::UserDetails& details, const std::string& group_id) {
  return std::any_of(
      details.scoped_permissions.begin(),
      details.scoped_permissions.end(),
      [&group_id](const permission::ScopedPermission& scoped_permission) {
        return scoped_permission.name == group_id;
      });
}

void UpsertScopedPermission(
    user::UserDetails& details,
    const permission::ScopedPermission& scoped_permission) {
  const auto it = std::find_if(
      details.scoped_permissions.begin(),
      details.scoped_permissions.end(),
      [&scoped_permission](const permission::ScopedPermission& current) {
        return current.name == scoped_permission.name;
      });

  if (it == details.scoped_permissions.end()) {
    details.scoped_permissions.push_back(scoped_permission);
    return;
  }

  *it = scoped_permission;
}

GroupDetails BuildGroupDetails(
    const GroupSummary& summary,
    const std::vector<user::UserDetails>& users) {
  GroupDetails details{.summary = summary};

  for (const auto& user : users) {
    if (std::find(user.owned_groups.begin(), user.owned_groups.end(), summary.group_id) !=
        user.owned_groups.end()) {
      details.owner_user_ids.push_back(user.summary.user_id);
    }

    if (HasGroupScopedPermission(user, summary.group_id) ||
        std::find(user.owned_groups.begin(), user.owned_groups.end(), summary.group_id) !=
            user.owned_groups.end()) {
      details.member_user_ids.push_back(user.summary.user_id);
    }
  }

  return details;
}

void EnsureUserExists(const user::UserDetails* user) {
  if (user == nullptr) {
    throw std::runtime_error("user_not_found");
  }
}

void EnsureGroupExists(const GroupSummary* group) {
  if (group == nullptr) {
    throw std::runtime_error("group_not_found");
  }
}

}  // namespace

GroupDetails GroupManagementService::CreateGroup(const CreateGroupCommand& command) const {
  auto& state = GetAppState();
  std::scoped_lock lock(state.mutex);

  auto* actor = FindUser(state.users, command.actor_user_id);
  EnsureUserExists(actor);

  if (command.group_name.empty()) {
    throw std::runtime_error("group_name_required");
  }

  const auto duplicate = std::find_if(
      state.groups.begin(),
      state.groups.end(),
      [&command](const GroupSummary& group) { return group.group_name == command.group_name; });
  if (duplicate != state.groups.end()) {
    throw std::runtime_error("group_name_already_exists");
  }

  if (!permission::CanCreateGroup(BuildPermissionUser(*actor))) {
    throw std::runtime_error("forbidden");
  }

  GroupSummary summary{
      .group_id = user::BuildNextGroupId(state),
      .group_name = command.group_name,
      .description = command.description,
      .created_by_user_id = actor->summary.user_id,
  };
  state.groups.push_back(summary);

  if (actor->summary.role == permission::Role::Owner &&
      std::find(actor->owned_groups.begin(), actor->owned_groups.end(), summary.group_id) ==
          actor->owned_groups.end()) {
    actor->owned_groups.push_back(summary.group_id);
  }

  if (!HasGroupScopedPermission(*actor, summary.group_id) &&
      std::find(actor->owned_groups.begin(), actor->owned_groups.end(), summary.group_id) ==
          actor->owned_groups.end()) {
    actor->scoped_permissions.push_back({summary.group_id, 10, 10});
  }

  return BuildGroupDetails(summary, state.users);
}

GroupInvitation GroupManagementService::InviteUser(const InviteUserCommand& command) const {
  auto& state = GetAppState();
  std::scoped_lock lock(state.mutex);

  auto* actor = FindUser(state.users, command.actor_user_id);
  auto* target = FindUser(state.users, command.invited_user_id);
  const auto* group = FindGroup(state.groups, command.group_id);
  EnsureUserExists(actor);
  EnsureUserExists(target);
  EnsureGroupExists(group);

  if (!permission::CanInviteToGroup(BuildPermissionUser(*actor), command.group_id)) {
    throw std::runtime_error("forbidden");
  }

  if (permission::IsMemberOfGroup(BuildPermissionUser(*target), command.group_id)) {
    throw std::runtime_error("user_already_member");
  }

  const auto pending = std::find_if(
      state.invitations.begin(),
      state.invitations.end(),
      [&command](const GroupInvitation& invitation) {
        return invitation.group_id == command.group_id &&
            invitation.invited_user_id == command.invited_user_id &&
            invitation.status == InvitationStatus::Pending;
      });
  if (pending != state.invitations.end()) {
    throw std::runtime_error("invitation_already_exists");
  }

  GroupInvitation invitation{
      .invitation_id = user::BuildNextInvitationId(state),
      .group_id = command.group_id,
      .invited_user_id = command.invited_user_id,
      .invited_by_user_id = command.actor_user_id,
      .status = InvitationStatus::Pending,
  };
  state.invitations.push_back(invitation);
  return invitation;
}

GroupInvitation GroupManagementService::AcceptInvitation(const AcceptInvitationCommand& command) const {
  auto& state = GetAppState();
  std::scoped_lock lock(state.mutex);

  auto* actor = FindUser(state.users, command.actor_user_id);
  EnsureUserExists(actor);

  auto* invitation = FindInvitation(state.invitations, command.invitation_id);
  if (invitation == nullptr) {
    throw std::runtime_error("invitation_not_found");
  }

  if (invitation->invited_user_id != command.actor_user_id) {
    throw std::runtime_error("forbidden");
  }

  if (invitation->status != InvitationStatus::Pending) {
    throw std::runtime_error("invitation_not_pending");
  }

  invitation->status = InvitationStatus::Accepted;
  if (!HasGroupScopedPermission(*actor, invitation->group_id) &&
      std::find(actor->owned_groups.begin(), actor->owned_groups.end(), invitation->group_id) ==
          actor->owned_groups.end()) {
    actor->scoped_permissions.push_back({invitation->group_id, 10, 10});
  }

  return *invitation;
}

permission::ScopedPermission GroupManagementService::AssignScopedPermission(
    const AssignScopedPermissionCommand& command) const {
  auto& state = GetAppState();
  std::scoped_lock lock(state.mutex);

  auto* actor = FindUser(state.users, command.actor_user_id);
  auto* target = FindUser(state.users, command.target_user_id);
  const auto* group = FindGroup(state.groups, command.group_id);
  EnsureUserExists(actor);
  EnsureUserExists(target);
  EnsureGroupExists(group);

  permission::ValidateScopedPermission(command.scoped_permission);

  if (!permission::CanAssignScopedPermission(
          BuildPermissionUser(*actor),
          BuildPermissionUser(*target),
          command.group_id,
          command.scoped_permission)) {
    throw std::runtime_error("forbidden");
  }

  UpsertScopedPermission(*target, command.scoped_permission);
  return command.scoped_permission;
}

GroupDetails GroupManagementService::AssignOwnerGroup(const AssignOwnerGroupCommand& command) const {
  auto& state = GetAppState();
  std::scoped_lock lock(state.mutex);

  auto* actor = FindUser(state.users, command.actor_user_id);
  auto* target = FindUser(state.users, command.target_user_id);
  const auto* group = FindGroup(state.groups, command.group_id);
  EnsureUserExists(actor);
  EnsureUserExists(target);
  EnsureGroupExists(group);

  if (!permission::CanAssignOwnerRole(BuildPermissionUser(*actor))) {
    throw std::runtime_error("forbidden");
  }

  if (!permission::IsMemberOfGroup(BuildPermissionUser(*target), command.group_id)) {
    throw std::runtime_error("target_not_group_member");
  }

  if (target->summary.role != permission::Role::Admin) {
    target->summary.role = permission::Role::Owner;
  }

  if (std::find(target->owned_groups.begin(), target->owned_groups.end(), command.group_id) ==
      target->owned_groups.end()) {
    target->owned_groups.push_back(command.group_id);
  }

  return BuildGroupDetails(*group, state.users);
}

}  // namespace picaresque::group
