#pragma once

#include <optional>
#include <string>
#include <vector>

#include "picaresque/auth/auth_types.hpp"
#include "picaresque/group/group_types.hpp"
#include "picaresque/user/user_types.hpp"

namespace picaresque::user {

class UserGroupRepository {
 public:
  virtual ~UserGroupRepository() = default;

  // User aggregate reads.
  virtual std::size_t CountUsers() const = 0;
  virtual std::vector<UserSummary> ListUsers(const UserListFilter& filter) const = 0;
  virtual std::optional<UserDetails> FindUserDetailsById(const std::string& user_id) const = 0;
  virtual std::optional<UserDetails> FindUserDetailsByApiKeyHash(const std::string& key_hash) const = 0;
  virtual bool UserExistsByLoginIdOrEmail(
      const std::string& login_id,
      const std::string& email) const = 0;

  // API key persistence. Plain keys are never stored by repository implementations.
  virtual std::optional<auth::ApiKeyInfo> FindApiKeyInfoByUserId(const std::string& user_id) const = 0;

  // User aggregate writes.
  virtual UserDetails CreateUser(
      const CreateUserCommand& command,
      const std::vector<permission::ScopedPermission>& initial_scoped_permissions) = 0;
  virtual auth::ApiKeyInfo UpsertApiKey(
      const std::string& user_id,
      const std::string& key_prefix,
      const std::string& key_hash) = 0;
  virtual void DeleteApiKey(const std::string& user_id) = 0;

  // Group facts used by service-level authorization and membership checks.
  virtual std::optional<group::GroupSummary> FindGroupSummaryById(const std::string& group_id) const = 0;
  virtual bool GroupExistsByName(const std::string& group_name) const = 0;
  virtual bool HasActiveMembership(
      const std::string& group_id,
      const std::string& user_id) const = 0;

  // Invitation facts.
  virtual std::optional<group::GroupInvitation> FindInvitationById(
      const std::string& invitation_id) const = 0;
  virtual bool HasPendingInvitation(
      const std::string& group_id,
      const std::string& invited_user_id) const = 0;

  // Group aggregate writes. Implementations own persistence invariants such as
  // owner-created groups adding the actor to ownership and membership.
  virtual group::GroupDetails CreateGroup(
      const group::CreateGroupCommand& command,
      const UserDetails& actor) = 0;
  virtual group::GroupInvitation CreateInvitation(const group::InviteUserCommand& command) = 0;
  virtual group::GroupInvitation AcceptInvitation(const group::AcceptInvitationCommand& command) = 0;
  virtual permission::ScopedPermission UpsertScopedPermission(
      const std::string& user_id,
      const permission::ScopedPermission& scoped_permission) = 0;
  virtual group::GroupDetails AssignOwnerGroup(
      const group::AssignOwnerGroupCommand& command,
      const UserDetails& target) = 0;
};

}  // namespace picaresque::user
