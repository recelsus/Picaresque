#include <cassert>

#include "picaresque/group/group_management_service.hpp"
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

  const auto member = user_service.CreateUser({
      .login_id = "member1",
      .user_name = "Member One",
      .email = "member1@example.local",
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
              .name = created_group.summary.group_id,
              .read = 60,
              .write = 60,
          },
  });
  assert(assigned_permission.read == 60);
  assert(assigned_permission.write == 60);

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

  return 0;
}
