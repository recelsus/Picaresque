#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "picaresque/user/user_group_repository.hpp"

namespace picaresque::user {

class InMemoryUserGroupRepository final : public UserGroupRepository {
 public:
  std::size_t CountUsers() const override;
  std::vector<UserSummary> ListUsers(const UserListFilter& filter) const override;
  std::optional<UserDetails> FindUserDetailsById(const std::string& user_id) const override;
  std::optional<UserDetails> FindUserDetailsByApiKeyHash(const std::string& key_hash) const override;
  bool UserExistsByLoginIdOrEmail(const std::string& login_id, const std::string& email) const override;
  std::optional<auth::ApiKeyInfo> FindApiKeyInfoByUserId(const std::string& user_id) const override;
  UserDetails CreateUser(
      const CreateUserCommand& command,
      const std::vector<permission::ScopedPermission>& initial_scoped_permissions) override;
  auth::ApiKeyInfo UpsertApiKey(
      const std::string& user_id,
      const std::string& key_prefix,
      const std::string& key_hash) override;
  void DeleteApiKey(const std::string& user_id) override;
  std::optional<group::GroupSummary> FindGroupSummaryById(const std::string& group_id) const override;
  bool GroupExistsByName(const std::string& group_name) const override;
  bool HasActiveMembership(const std::string& group_id, const std::string& user_id) const override;
  std::optional<group::GroupInvitation> FindInvitationById(const std::string& invitation_id) const override;
  bool HasPendingInvitation(const std::string& group_id, const std::string& invited_user_id) const override;
  group::GroupDetails CreateGroup(
      const group::CreateGroupCommand& command,
      const UserDetails& actor) override;
  group::GroupInvitation CreateInvitation(const group::InviteUserCommand& command) override;
  group::GroupInvitation AcceptInvitation(const group::AcceptInvitationCommand& command) override;
  permission::ScopedPermission UpsertScopedPermission(
      const std::string& user_id,
      const permission::ScopedPermission& scoped_permission) override;
  group::GroupDetails AssignOwnerGroup(
      const group::AssignOwnerGroupCommand& command,
      const UserDetails& target) override;

 private:
  std::string BuildNextUserId();
  std::string BuildNextGroupId();
  std::string BuildNextInvitationId();
  group::GroupDetails BuildGroupDetails(const std::string& group_id) const;
  UserDetails* FindMutableUserById(const std::string& user_id);
  group::GroupSummary* FindMutableGroupById(const std::string& group_id);
  group::GroupInvitation* FindMutableInvitationById(const std::string& invitation_id);

  mutable std::mutex mutex_;
  std::vector<UserDetails> users_;
  std::vector<auth::ApiKeyInfo> api_keys_;
  std::vector<std::pair<std::string, std::string>> api_key_hashes_;
  std::vector<group::GroupSummary> groups_;
  std::vector<group::GroupInvitation> invitations_;
  std::size_t next_user_id_ = 1;
  std::size_t next_group_id_ = 1;
  std::size_t next_invitation_id_ = 1;
};

}  // namespace picaresque::user
