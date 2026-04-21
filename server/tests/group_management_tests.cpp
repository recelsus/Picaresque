#include <cassert>
#include <algorithm>
#include <stdexcept>
#include <string>

#include "picaresque/group/group_management_service.hpp"
#include "picaresque/permission/api.hpp"
#include "picaresque/user/in_memory_user_group_repository.hpp"
#include "picaresque/user/user_management_service.hpp"

namespace group = picaresque::group;
namespace user = picaresque::user;
namespace permission = picaresque::permission;

int main() {
  user::InMemoryUserGroupRepository repository;
  const user::UserManagementService user_service(repository);
  const group::GroupManagementService group_service(repository);

  const auto admin = user_service.CreateInitialAdmin({
      .login_id = "admin",
      .user_name = "Initial Admin",
      .email = "admin@example.local",
      .password = "change-me",
      .role = permission::Role::Admin,
  });
  assert(admin.scoped_permissions.empty());

  const auto member = user_service.CreateUser({
      .login_id = "member1",
      .user_name = "Member One",
      .email = "member1@example.local",
      .password = "change-me",
      .role = permission::Role::Member,
  });

  const auto owner = user_service.CreateUser({
      .login_id = "owner1",
      .user_name = "Owner One",
      .email = "owner1@example.local",
      .password = "change-me",
      .role = permission::Role::Owner,
  });

  const auto outsider = user_service.CreateUser({
      .login_id = "outsider1",
      .user_name = "Outsider One",
      .email = "outsider1@example.local",
      .password = "change-me",
      .role = permission::Role::Member,
  });

  const auto created_group = group_service.CreateGroup({
      .actor_user_id = admin.summary.user_id,
      .group_name = "group-alpha",
      .description = std::string("Alpha group"),
  });
  assert(created_group.summary.group_name == "group-alpha");
  assert(created_group.summary.created_by_user_id == admin.summary.user_id);

  const auto invitation = group_service.InviteUser({
      .actor_user_id = admin.summary.user_id,
      .group_id = created_group.summary.group_id,
      .invited_user_id = member.summary.user_id,
  });
  assert(invitation.status == group::InvitationStatus::Pending);

  const auto accepted = group_service.AcceptInvitation({
      .actor_user_id = member.summary.user_id,
      .invitation_id = invitation.invitation_id,
  });
  assert(accepted.status == group::InvitationStatus::Accepted);

  const auto assigned_permission = group_service.AssignScopedPermission({
      .actor_user_id = admin.summary.user_id,
      .target_user_id = member.summary.user_id,
      .group_id = created_group.summary.group_id,
      .scoped_permission =
          {
              .group_id = created_group.summary.group_id,
              .read = 60,
              .write = 60,
          },
  });
  assert(assigned_permission.read == 60);
  assert(assigned_permission.write == 60);

  {
    bool failed = false;
    try {
      group_service.AssignScopedPermission({
          .actor_user_id = admin.summary.user_id,
          .target_user_id = outsider.summary.user_id,
          .group_id = created_group.summary.group_id,
          .scoped_permission =
              {
                  .group_id = created_group.summary.group_id,
                  .read = 30,
                  .write = 30,
              },
      });
    } catch (const std::runtime_error& error) {
      failed = std::string(error.what()) == "target_not_group_member";
    }
    assert(failed);
  }

  const auto updated_group = group_service.AssignOwnerGroup({
      .actor_user_id = admin.summary.user_id,
      .target_user_id = member.summary.user_id,
      .group_id = created_group.summary.group_id,
  });
  assert(!updated_group.owner_user_ids.empty());

  const auto member_details = user_service.GetUserDetails(member.summary.user_id);
  assert(member_details.summary.role == permission::Role::Owner);
  assert(!member_details.owned_groups.empty());
  assert(member_details.owned_groups.front() == created_group.summary.group_id);

  const auto owner_created_group = group_service.CreateGroup({
      .actor_user_id = owner.summary.user_id,
      .group_name = "group-owner-created",
      .description = std::string("Owner created group"),
  });

  const auto owner_details = user_service.GetUserDetails(owner.summary.user_id);
  assert(std::find(
             owner_details.owned_groups.begin(),
             owner_details.owned_groups.end(),
             owner_created_group.summary.group_id) != owner_details.owned_groups.end());

  const permission::User permission_owner{
      .user_id = owner_details.summary.user_id,
      .user_name = owner_details.summary.user_name,
      .role = owner_details.summary.role,
      .owned_groups = owner_details.owned_groups,
      .scoped_permissions = owner_details.scoped_permissions,
  };
  const auto resolved = permission::ResolvePermissionForGroup(
      permission_owner,
      owner_created_group.summary.group_id);
  assert(resolved.read == 99);
  assert(resolved.write == 99);
  assert(resolved.source == permission::PermissionSource::OwnedGroupOverride);

  const auto owner_invitation = group_service.InviteUser({
      .actor_user_id = owner.summary.user_id,
      .group_id = owner_created_group.summary.group_id,
      .invited_user_id = outsider.summary.user_id,
  });
  assert(owner_invitation.status == group::InvitationStatus::Pending);

  const auto owner_accepted = group_service.AcceptInvitation({
      .actor_user_id = outsider.summary.user_id,
      .invitation_id = owner_invitation.invitation_id,
  });
  assert(owner_accepted.status == group::InvitationStatus::Accepted);

  const auto owner_assigned_permission = group_service.AssignScopedPermission({
      .actor_user_id = owner.summary.user_id,
      .target_user_id = outsider.summary.user_id,
      .group_id = owner_created_group.summary.group_id,
      .scoped_permission =
          {
              .group_id = owner_created_group.summary.group_id,
              .read = 90,
              .write = 90,
          },
  });
  assert(owner_assigned_permission.read == 90);
  assert(owner_assigned_permission.write == 90);

  {
    bool failed = false;
    try {
      group_service.AssignOwnerGroup({
          .actor_user_id = owner.summary.user_id,
          .target_user_id = outsider.summary.user_id,
          .group_id = owner_created_group.summary.group_id,
      });
    } catch (const std::runtime_error& error) {
      failed = std::string(error.what()) == "forbidden";
    }
    assert(failed);
  }

  return 0;
}
