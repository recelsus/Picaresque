#pragma once

#include <string>

namespace picaresque::auth {

struct ApiKeyInfo {
  std::string user_id;
  std::string key_prefix;
  bool enabled = true;
};

struct IssuedApiKey {
  ApiKeyInfo info;
  std::string plain_api_key;
};

}  // namespace picaresque::auth
