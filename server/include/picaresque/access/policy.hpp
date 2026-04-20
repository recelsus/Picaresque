#pragma once

#include <cstdint>

#include "picaresque/access/repository.hpp"

namespace picaresque::access {

class AccessPolicyService {
 public:
  AccessPolicyService(const AccessRepository& repository, AccessConfiguration configuration);

  AccessDecisionResult Evaluate(
      const AccessRequestContext& request_context,
      std::int64_t now_unix_seconds) const;

 private:
  DefaultPolicy ResolveDefaultPolicy(AccessSurface surface) const;

  const AccessRepository& repository_;
  AccessConfiguration configuration_;
};

}  // namespace picaresque::access
