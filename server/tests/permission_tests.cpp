#include <cassert>
#include <vector>

#include "picaresque/permission/api.hpp"

namespace permission = picaresque::permission;

int main() {
  const permission::User member_user{
      .user_id = "user_member",
      .user_name = "Member User",
      .role = permission::Role::Member,
      .owned_groups = {},
      .scoped_permissions = {
          {"group_alpha", 30, 30},
          {"group_beta", 10, 10},
      },
  };

  const permission::User owner_user{
      .user_id = "user_owner",
      .user_name = "Owner User",
      .role = permission::Role::Owner,
      .owned_groups = {"group_alpha"},
      .scoped_permissions = {
          {"group_beta", 90, 90},
      },
  };

  const permission::User admin_user{
      .user_id = "user_admin",
      .user_name = "Admin User",
      .role = permission::Role::Admin,
      .owned_groups = {},
      .scoped_permissions = {
          {"*", 30, 30},
      },
  };

  permission::ValidateUser(member_user);
  permission::ValidateUser(owner_user);
  permission::ValidateUser(admin_user);

  {
    const auto resolved = permission::ResolvePermissionForGroup(owner_user, "group_alpha");
    assert(resolved.read == 99);
    assert(resolved.write == 99);
    assert(resolved.source == permission::PermissionSource::OwnedGroupOverride);
  }

  {
    const auto resolved = permission::ResolvePermissionForGroup(member_user, "group_unknown");
    assert(resolved.read == 10);
    assert(resolved.write == 10);
    assert(resolved.source == permission::PermissionSource::DefaultScope);
  }

  {
    const std::vector<permission::AccessRequirement> requirements = {
        {"group_missing", 60, 60},
        {"group_alpha", 30, 30},
    };
    assert(permission::CanReadAndWrite(member_user, requirements));
  }

  {
    const std::vector<permission::AccessRequirement> requirements = {
        {"group_alpha", 60, 60},
        {"group_beta", 30, 30},
    };
    assert(!permission::CanReadAndWrite(member_user, requirements));
  }

  {
    assert(permission::CanCreateGroup(owner_user));
    assert(permission::CanDeleteGroup(owner_user, "group_alpha"));
    assert(!permission::CanDeleteGroup(owner_user, "group_beta"));
    assert(permission::CanInviteToGroup(owner_user, "group_alpha"));
    assert(permission::CanInviteToGroup(owner_user, "group_beta"));
  }

  {
    const permission::User target_member{
        .user_id = "user_target",
        .user_name = "Target User",
        .role = permission::Role::Member,
        .owned_groups = {},
        .scoped_permissions = {
            {"group_alpha", 10, 10},
        },
    };

    assert(permission::CanAssignScopedPermission(
        owner_user,
        target_member,
        "group_alpha",
        {"group_alpha", 90, 90}));

    assert(!permission::CanAssignScopedPermission(
        owner_user,
        target_member,
        "group_alpha",
        {"group_alpha", 99, 99}));

    assert(permission::CanAssignScopedPermission(
        admin_user,
        target_member,
        "group_alpha",
        {"group_alpha", 99, 99}));
  }

  {
    const permission::User outsider{
        .user_id = "user_outsider",
        .user_name = "Outsider User",
        .role = permission::Role::Member,
        .owned_groups = {},
        .scoped_permissions = {},
    };

    assert(!permission::CanAssignScopedPermission(
        owner_user,
        outsider,
        "group_alpha",
        {"group_alpha", 30, 30}));
  }

  {
    permission::ValidateScopedPermission({"group_alpha", 9, 1});
    permission::ValidateAccessRequirement({"group_alpha", 9, 1});
  }

  {
    bool failed = false;
    try {
      permission::ValidateScopedPermission({"group_alpha", 0, 0});
    } catch (const permission::ValidationError&) {
      failed = true;
    }
    assert(failed);
  }

  {
    bool failed = false;
    try {
      permission::ValidateUser(permission::User{
          .user_id = "user_invalid",
          .user_name = "Invalid User",
          .role = permission::Role::Member,
          .owned_groups = {},
          .scoped_permissions = {
              {"*", 30, 30},
          },
      });
    } catch (const permission::ValidationError&) {
      failed = true;
    }
    assert(failed);
  }

  assert(permission::CanAssignOwnerRole(admin_user));
  assert(!permission::CanAssignOwnerRole(owner_user));
  assert(!permission::CanAssignOwnerRole(member_user));

  return 0;
}
