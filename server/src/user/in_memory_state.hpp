#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "picaresque/group/group_types.hpp"
#include "picaresque/user/user_types.hpp"

namespace picaresque::user {

struct InMemoryAppState {
  mutable std::mutex mutex;
  std::vector<UserDetails> users;
  std::vector<group::GroupSummary> groups;
  std::vector<group::GroupInvitation> invitations;
  std::size_t next_user_id = 1;
  std::size_t next_group_id = 1;
  std::size_t next_invitation_id = 1;
};

InMemoryAppState& GetAppState();
std::string BuildNextUserId(InMemoryAppState& state);
std::string BuildNextGroupId(InMemoryAppState& state);
std::string BuildNextInvitationId(InMemoryAppState& state);

}  // namespace picaresque::user
