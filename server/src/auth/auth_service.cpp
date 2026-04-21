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

std::string BuildKeyPrefix(std::string_view api_key) {
  const std::size_t prefix_length = std::min<std::size_t>(12, api_key.size());
  return std::string(api_key.substr(0, prefix_length));
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
      repository_.UpsertApiKey(user_id, BuildKeyPrefix(plain_api_key), HashApiKey(plain_api_key));
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

std::string HashApiKey(std::string_view api_key) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(
      reinterpret_cast<const unsigned char*>(api_key.data()),
      api_key.size(),
      hash);

  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (const auto byte : hash) {
    stream << std::setw(2) << static_cast<int>(byte);
  }
  return stream.str();
}

}  // namespace picaresque::auth
