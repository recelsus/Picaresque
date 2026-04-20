#include "picaresque/permission/access.hpp"

#include "picaresque/permission/resolver.hpp"

namespace picaresque::permission {
namespace {

bool MatchesAnyRequirement(
    const User& user,
    const std::vector<AccessRequirement>& requirements,
    bool require_read,
    bool require_write) {
  if (requirements.empty()) {
    return true;
  }

  for (const auto& requirement : requirements) {
    const auto resolved = ResolvePermissionForGroup(user, requirement.name);
    const bool read_ok = !require_read || resolved.read >= requirement.read;
    const bool write_ok = !require_write || resolved.write >= requirement.write;

    if (read_ok && write_ok) {
      return true;
    }
  }

  return false;
}

}  // namespace

bool CanRead(const User& user, const std::vector<AccessRequirement>& requirements) {
  return MatchesAnyRequirement(user, requirements, true, false);
}

bool CanWrite(const User& user, const std::vector<AccessRequirement>& requirements) {
  return MatchesAnyRequirement(user, requirements, false, true);
}

bool CanReadAndWrite(const User& user, const std::vector<AccessRequirement>& requirements) {
  return MatchesAnyRequirement(user, requirements, true, true);
}

}  // namespace picaresque::permission
