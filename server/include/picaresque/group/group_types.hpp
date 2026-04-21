#pragma once

#include <optional>
#include <string>
#include <vector>

#include "picaresque/permission/types.hpp"

namespace picaresque::group {

enum class InvitationStatus {
  Pending,
  Accepted,
  Declined,
  Expired,
};

struct GroupSummary {
  std::string group_id;
  std::string group_name;
  std::optional<std::string> description;
  std::string created_by_user_id;
};

struct GroupDetails {
  GroupSummary summary;
  std::vector<std::string> owner_user_ids;
  std::vector<std::string> member_user_ids;
};

struct GroupInvitation {
  std::string invitation_id;
  std::string group_id;
  std::string invited_user_id;
  std::string invited_by_user_id;
  InvitationStatus status = InvitationStatus::Pending;
};

struct CreateGroupCommand {
  std::string actor_user_id;
  std::string group_name;
  std::optional<std::string> description;
};

struct InviteUserCommand {
  std::string actor_user_id;
  std::string group_id;
  std::string invited_user_id;
};

struct AcceptInvitationCommand {
  std::string actor_user_id;
  std::string invitation_id;
};

struct AssignScopedPermissionCommand {
  std::string actor_user_id;
  std::string target_user_id;
  std::string group_id;
  permission::ScopedPermission scoped_permission;
};

struct AssignOwnerGroupCommand {
  std::string actor_user_id;
  std::string target_user_id;
  std::string group_id;
};

}  // namespace picaresque::group
