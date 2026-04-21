#pragma once

#include "picaresque/group/group_types.hpp"

namespace picaresque::group {

class GroupManagementService {
 public:
  GroupDetails CreateGroup(const CreateGroupCommand& command) const;
  GroupInvitation InviteUser(const InviteUserCommand& command) const;
  GroupInvitation AcceptInvitation(const AcceptInvitationCommand& command) const;
  permission::ScopedPermission AssignScopedPermission(const AssignScopedPermissionCommand& command) const;
  GroupDetails AssignOwnerGroup(const AssignOwnerGroupCommand& command) const;
};

}  // namespace picaresque::group
