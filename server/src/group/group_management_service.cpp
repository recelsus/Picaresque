#include "picaresque/group/group_management_service.hpp"

#include <stdexcept>

#include "picaresque/permission/api.hpp"

namespace picaresque::group {
namespace {

permission::User BuildPermissionUser(const user::UserDetails& details) {
  return {
      .user_id = details.summary.user_id,
      .user_name = details.summary.user_name,
      .role = details.summary.role,
      .owned_groups = details.owned_groups,
      .scoped_permissions = details.scoped_permissions,
  };
}

}  // namespace

GroupManagementService::GroupManagementService(user::UserGroupRepository& repository)
    : repository_(repository) {}

GroupDetails GroupManagementService::CreateGroup(const CreateGroupCommand& command) const {
  const auto actor = repository_.FindUserDetailsById(command.actor_user_id);
  if (!actor.has_value()) {
    throw std::runtime_error("user_not_found");
  }

  if (command.group_name.empty()) {
    throw std::runtime_error("group_name_required");
  }

  if (repository_.GroupExistsByName(command.group_name)) {
    throw std::runtime_error("group_name_already_exists");
  }

  if (!permission::CanCreateGroup(BuildPermissionUser(*actor))) {
    throw std::runtime_error("forbidden");
  }

  return repository_.CreateGroup(command, *actor);
}

GroupInvitation GroupManagementService::InviteUser(const InviteUserCommand& command) const {
  const auto actor = repository_.FindUserDetailsById(command.actor_user_id);
  const auto target = repository_.FindUserDetailsById(command.invited_user_id);
  const auto group = repository_.FindGroupSummaryById(command.group_id);
  if (!actor.has_value() || !target.has_value()) {
    throw std::runtime_error("user_not_found");
  }
  if (!group.has_value()) {
    throw std::runtime_error("group_not_found");
  }

  if (!permission::CanInviteToGroup(BuildPermissionUser(*actor), command.group_id)) {
    throw std::runtime_error("forbidden");
  }

  if (repository_.HasActiveMembership(command.group_id, command.invited_user_id)) {
    throw std::runtime_error("user_already_member");
  }

  if (repository_.HasPendingInvitation(command.group_id, command.invited_user_id)) {
    throw std::runtime_error("invitation_already_exists");
  }

  return repository_.CreateInvitation(command);
}

GroupInvitation GroupManagementService::AcceptInvitation(const AcceptInvitationCommand& command) const {
  const auto actor = repository_.FindUserDetailsById(command.actor_user_id);
  if (!actor.has_value()) {
    throw std::runtime_error("user_not_found");
  }

  const auto invitation = repository_.FindInvitationById(command.invitation_id);
  if (!invitation.has_value()) {
    throw std::runtime_error("invitation_not_found");
  }

  if (invitation->invited_user_id != command.actor_user_id) {
    throw std::runtime_error("forbidden");
  }

  if (invitation->status != InvitationStatus::Pending) {
    throw std::runtime_error("invitation_not_pending");
  }

  return repository_.AcceptInvitation(command);
}

permission::ScopedPermission GroupManagementService::AssignScopedPermission(
    const AssignScopedPermissionCommand& command) const {
  const auto actor = repository_.FindUserDetailsById(command.actor_user_id);
  const auto target = repository_.FindUserDetailsById(command.target_user_id);
  if (!actor.has_value() || !target.has_value()) {
    throw std::runtime_error("user_not_found");
  }
  if (command.group_id != "*" && !repository_.FindGroupSummaryById(command.group_id).has_value()) {
    throw std::runtime_error("group_not_found");
  }

  permission::ValidateScopedPermission(command.scoped_permission);

  if (!permission::CanAssignScopedPermission(
          BuildPermissionUser(*actor),
          command.group_id,
          command.scoped_permission)) {
    throw std::runtime_error("forbidden");
  }

  if (command.group_id != "*" &&
      !repository_.HasActiveMembership(command.group_id, command.target_user_id)) {
    throw std::runtime_error("target_not_group_member");
  }

  return repository_.UpsertScopedPermission(command.target_user_id, command.scoped_permission);
}

GroupDetails GroupManagementService::AssignOwnerGroup(const AssignOwnerGroupCommand& command) const {
  const auto actor = repository_.FindUserDetailsById(command.actor_user_id);
  const auto target = repository_.FindUserDetailsById(command.target_user_id);
  const auto group = repository_.FindGroupSummaryById(command.group_id);
  if (!actor.has_value() || !target.has_value()) {
    throw std::runtime_error("user_not_found");
  }
  if (!group.has_value()) {
    throw std::runtime_error("group_not_found");
  }

  if (!permission::CanAssignOwnerRole(BuildPermissionUser(*actor))) {
    throw std::runtime_error("forbidden");
  }

  if (!repository_.HasActiveMembership(command.group_id, command.target_user_id)) {
    throw std::runtime_error("target_not_group_member");
  }

  return repository_.AssignOwnerGroup(command, *target);
}

}  // namespace picaresque::group
