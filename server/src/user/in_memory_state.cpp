#include "in_memory_state.hpp"

#include <sstream>

namespace picaresque::user {

InMemoryAppState& GetAppState() {
  static InMemoryAppState state;
  return state;
}

std::string BuildNextUserId(InMemoryAppState& state) {
  std::ostringstream stream;
  stream << "user_" << state.next_user_id++;
  return stream.str();
}

std::string BuildNextGroupId(InMemoryAppState& state) {
  std::ostringstream stream;
  stream << "group_" << state.next_group_id++;
  return stream.str();
}

std::string BuildNextInvitationId(InMemoryAppState& state) {
  std::ostringstream stream;
  stream << "invitation_" << state.next_invitation_id++;
  return stream.str();
}

}  // namespace picaresque::user
