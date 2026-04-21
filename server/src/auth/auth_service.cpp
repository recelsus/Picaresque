#include "picaresque/auth/auth_service.hpp"

#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

#include <openssl/sha.h>

namespace picaresque::auth {
namespace {

std::string GenerateApiKey() {
  static std::mt19937_64 generator(std::random_device{}());
  static constexpr char kHex[] = "0123456789abcdef";

  std::string value = "pk_";
  for (int i = 0; i < 48; ++i) {
    value.push_back(kHex[generator() % 16]);
  }
  return value;
}

std::string GenerateSessionToken() {
  static std::mt19937_64 generator(std::random_device{}());
  static constexpr char kHex[] = "0123456789abcdef";

  std::string value = "ps_";
  for (int i = 0; i < 64; ++i) {
    value.push_back(kHex[generator() % 16]);
  }
  return value;
}

std::string BuildPrefix(std::string_view value) {
  const std::size_t prefix_length = std::min<std::size_t>(12, value.size());
  return std::string(value.substr(0, prefix_length));
}

std::string HashValue(std::string_view namespace_prefix, std::string_view value) {
  const std::string namespaced = std::string(namespace_prefix) + ":" + std::string(value);
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(
      reinterpret_cast<const unsigned char*>(namespaced.data()),
      namespaced.size(),
      hash);

  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (const auto byte : hash) {
    stream << std::setw(2) << static_cast<int>(byte);
  }
  return stream.str();
}

}  // namespace

AuthService::AuthService(user::UserGroupRepository& repository) : repository_(repository) {}

std::optional<ApiKeyInfo> AuthService::GetUserApiKey(const std::string& user_id) const {
  if (!repository_.FindUserDetailsById(user_id).has_value()) {
    throw std::runtime_error("user_not_found");
  }
  return repository_.FindApiKeyInfoByUserId(user_id);
}

IssuedApiKey AuthService::IssueUserApiKey(const std::string& user_id) const {
  if (!repository_.FindUserDetailsById(user_id).has_value()) {
    throw std::runtime_error("user_not_found");
  }

  const auto plain_api_key = GenerateApiKey();
  const auto api_key_info =
      repository_.UpsertApiKey(user_id, BuildPrefix(plain_api_key), HashApiKey(plain_api_key));
  return {
      .info = api_key_info,
      .plain_api_key = plain_api_key,
  };
}

void AuthService::RevokeUserApiKey(const std::string& user_id) const {
  if (!repository_.FindUserDetailsById(user_id).has_value()) {
    throw std::runtime_error("user_not_found");
  }
  repository_.DeleteApiKey(user_id);
}

user::UserDetails AuthService::AuthenticateApiKey(const std::string& api_key) const {
  if (api_key.empty()) {
    throw std::runtime_error("api_key_required");
  }

  const auto user = repository_.FindUserDetailsByApiKeyHash(HashApiKey(api_key));
  if (!user.has_value()) {
    throw std::runtime_error("invalid_api_key");
  }
  return *user;
}

IssuedWebSession AuthService::LoginWithPassword(
    const std::string& login_id,
    const std::string& password) const {
  if (login_id.empty() || password.empty()) {
    throw std::runtime_error("invalid_credentials");
  }

  const auto password_record = repository_.FindUserPasswordByLoginId(login_id);
  if (!password_record.has_value() || !password_record->is_active) {
    throw std::runtime_error("invalid_credentials");
  }

  const auto hashed_password = HashPassword(password);
  if (password_record->password_hash != hashed_password) {
    throw std::runtime_error("invalid_credentials");
  }

  const auto plain_session_token = GenerateSessionToken();
  const auto session_info = repository_.CreateWebSession(
      password_record->user_id,
      BuildPrefix(plain_session_token),
      HashSessionToken(plain_session_token));
  return {
      .info = session_info,
      .plain_session_token = plain_session_token,
  };
}

user::UserDetails AuthService::AuthenticateWebSession(const std::string& session_token) const {
  if (session_token.empty()) {
    throw std::runtime_error("session_required");
  }

  const auto user = repository_.FindUserDetailsByWebSessionTokenHash(HashSessionToken(session_token));
  if (!user.has_value()) {
    throw std::runtime_error("invalid_session");
  }
  return *user;
}

void AuthService::LogoutWebSession(const std::string& session_token) const {
  if (session_token.empty()) {
    throw std::runtime_error("session_required");
  }
  repository_.DeleteWebSessionByTokenHash(HashSessionToken(session_token));
}

std::string HashApiKey(std::string_view api_key) {
  return HashValue("api_key", api_key);
}

std::string HashPassword(std::string_view password) {
  return HashValue("password", password);
}

std::string HashSessionToken(std::string_view session_token) {
  return HashValue("web_session", session_token);
}

}  // namespace picaresque::auth
