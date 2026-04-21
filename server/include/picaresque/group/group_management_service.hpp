#pragma once

#include <vector>

#include "picaresque/group/group_types.hpp"
#include "picaresque/user/user_group_repository.hpp"

namespace picaresque::group {

class GroupManagementService {
 public:
  explicit GroupManagementService(user::UserGroupRepository& repository);

  std::vector<GroupDetails> ListGroups() const;
  GroupDetails GetGroupDetails(const std::string& group_id) const;
  GroupDetails CreateGroup(const CreateGroupCommand& command) const;
  GroupInvitation InviteUser(const InviteUserCommand& command) const;
  GroupInvitation AcceptInvitation(const AcceptInvitationCommand& command) const;
  permission::ScopedPermission AssignScopedPermission(const AssignScopedPermissionCommand& command) const;
  GroupDetails AssignOwnerGroup(const AssignOwnerGroupCommand& command) const;

 private:
  user::UserGroupRepository& repository_;
};

}  // namespace picaresque::group
