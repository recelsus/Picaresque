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

  const permission::User member_with_wildcard{
      .user_id = "user_member_with_wildcard",
      .user_name = "Member With Wildcard",
      .role = permission::Role::Member,
      .owned_groups = {},
      .scoped_permissions = {
          {"*", 30, 30},
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

  const permission::User owner_with_wildcard{
      .user_id = "user_owner_with_wildcard",
      .user_name = "Owner With Wildcard",
      .role = permission::Role::Owner,
      .owned_groups = {"group_owned"},
      .scoped_permissions = {
          {"*", 30, 30},
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
  permission::ValidateUser(member_with_wildcard);
  permission::ValidateUser(owner_user);
  permission::ValidateUser(owner_with_wildcard);
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
    const auto resolved = permission::ResolvePermissionForGroup(member_with_wildcard, "group_unknown");
    assert(resolved.read == 30);
    assert(resolved.write == 30);
    assert(resolved.source == permission::PermissionSource::WildcardScope);
  }

  {
    const permission::User direct_and_wildcard{
        .user_id = "user_direct_and_wildcard",
        .user_name = "Direct And Wildcard",
        .role = permission::Role::Member,
        .owned_groups = {},
        .scoped_permissions = {
            {"group_alpha", 60, 60},
            {"*", 30, 30},
        },
    };

    const auto resolved = permission::ResolvePermissionForGroup(direct_and_wildcard, "group_alpha");
    assert(resolved.read == 60);
    assert(resolved.write == 60);
    assert(resolved.source == permission::PermissionSource::DirectScope);
  }

  {
    const permission::User owner_with_direct_scope{
        .user_id = "user_owner_with_direct",
        .user_name = "Owner With Direct Scope",
        .role = permission::Role::Owner,
        .owned_groups = {"group_alpha"},
        .scoped_permissions = {
            {"group_alpha", 30, 30},
        },
    };

    const auto resolved = permission::ResolvePermissionForGroup(owner_with_direct_scope, "group_alpha");
    assert(resolved.read == 99);
    assert(resolved.write == 99);
    assert(resolved.source == permission::PermissionSource::OwnedGroupOverride);
  }

  {
    const std::vector<permission::AccessRequirement> requirements = {
        {"group_missing", 60, 60},
        {"group_alpha", 30, 30},
    };
    assert(permission::CanReadAndWrite(member_user, requirements));

    const auto result = permission::EvaluateReadAndWrite(member_user, requirements);
    assert(result.IsAllowed());
    assert(result.reason == permission::PermissionCheckReason::RequirementSatisfied);
    assert(result.matched_requirement.has_value());
    assert(result.matched_requirement->group_id == "group_alpha");
    assert(result.resolved_permission.has_value());
    assert(result.resolved_permission->source == permission::PermissionSource::DirectScope);
  }

  {
    const std::vector<permission::AccessRequirement> requirements = {
        {"group_alpha", 30, 30},
        {"group_beta", 10, 10},
    };
    assert(permission::CanRead(member_user, requirements));
    assert(permission::CanWrite(member_user, requirements));
    assert(permission::CanReadAndWrite(member_user, requirements));
  }

  {
    const std::vector<permission::AccessRequirement> requirements = {
        {"group_alpha", 60, 60},
        {"group_beta", 30, 30},
    };
    assert(!permission::CanReadAndWrite(member_user, requirements));

    const auto result = permission::EvaluateReadAndWrite(member_user, requirements);
    assert(!result.IsAllowed());
    assert(result.reason == permission::PermissionCheckReason::RequirementNotSatisfied);
    assert(!result.matched_requirement.has_value());
    assert(result.resolved_permission.has_value());
  }

  {
    const std::vector<permission::AccessRequirement> requirements = {
        {"group_alpha", 60, 30},
    };
    assert(!permission::CanReadAndWrite(member_user, requirements));
    assert(!permission::CanRead(member_user, requirements));
    assert(permission::CanWrite(member_user, requirements));
  }

  {
    const std::vector<permission::AccessRequirement> requirements = {};
    assert(permission::CanRead(member_user, requirements));
    assert(permission::CanWrite(member_user, requirements));
    assert(permission::CanReadAndWrite(member_user, requirements));

    const auto result = permission::EvaluateRead(member_user, requirements);
    assert(result.IsAllowed());
    assert(result.reason == permission::PermissionCheckReason::NoRequirement);
  }

  {
    assert(permission::CanCreateGroup(owner_user));
    assert(permission::CanDeleteGroup(owner_user, "group_alpha"));
    assert(!permission::CanDeleteGroup(owner_user, "group_beta"));
    assert(permission::CanInviteToGroup(owner_user, "group_alpha"));
    assert(permission::CanInviteToGroup(owner_user, "group_beta"));
  }

  {
    const permission::User rw90_member{
        .user_id = "user_rw90",
        .user_name = "RW90 Member",
        .role = permission::Role::Member,
        .owned_groups = {},
        .scoped_permissions = {
            {"group_beta", 90, 90},
        },
    };

    assert(!permission::CanCreateGroup(rw90_member));
    assert(!permission::CanDeleteGroup(rw90_member, "group_beta"));
    assert(permission::CanInviteToGroup(rw90_member, "group_beta"));
  }

  {
    assert(permission::CanAssignScopedPermission(
        owner_user,
        "group_alpha",
        {"group_alpha", 90, 90}));

    assert(!permission::CanAssignScopedPermission(
        owner_user,
        "group_alpha",
        {"group_alpha", 99, 99}));

    assert(permission::CanAssignScopedPermission(
        admin_user,
        "group_alpha",
        {"group_alpha", 99, 99}));
  }

  {
    const permission::User rw90_member{
        .user_id = "user_rw90_assign",
        .user_name = "RW90 Assign Member",
        .role = permission::Role::Member,
        .owned_groups = {},
        .scoped_permissions = {
            {"group_beta", 90, 90},
        },
    };

    assert(permission::CanAssignScopedPermission(
        rw90_member,
        "group_beta",
        {"group_beta", 90, 90}));

    assert(!permission::CanAssignScopedPermission(
        rw90_member,
        "group_beta",
        {"group_beta", 99, 99}));
  }

  {
    assert(permission::CanAssignScopedPermission(
        owner_user,
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
    permission::ValidateUser(permission::User{
        .user_id = "user_with_wildcard",
        .user_name = "Wildcard User",
        .role = permission::Role::Member,
        .owned_groups = {},
        .scoped_permissions = {
            {"*", 30, 30},
        },
    });
  }

  {
    assert(permission::CanAssignScopedPermission(
        admin_user,
        "group_alpha",
        {"*", 30, 30}));

    assert(!permission::CanAssignScopedPermission(
        owner_user,
        "group_alpha",
        {"*", 30, 30}));
  }

  {
    const permission::User owner_only_user{
        .user_id = "user_owner_only",
        .user_name = "Owner Only User",
        .role = permission::Role::Owner,
        .owned_groups = {"group_gamma"},
        .scoped_permissions = {},
    };

    assert(permission::IsMemberOfGroup(owner_only_user, "group_gamma"));
    assert(!permission::IsMemberOfGroup(owner_only_user, "group_missing"));
  }

  assert(permission::CanAssignOwnerRole(admin_user));
  assert(!permission::CanAssignOwnerRole(owner_user));
  assert(!permission::CanAssignOwnerRole(member_user));

  return 0;
}
