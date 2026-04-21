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

struct UserPasswordRecord {
  std::string user_id;
  std::string password_hash;
  bool is_active = true;
};

struct WebSessionInfo {
  std::string session_id;
  std::string user_id;
  std::string token_prefix;
  bool enabled = true;
};

struct IssuedWebSession {
  WebSessionInfo info;
  std::string plain_session_token;
};

}  // namespace picaresque::auth
