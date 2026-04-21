#include "picaresque/auth/hash.hpp"

#include <iomanip>
#include <sstream>

#include <openssl/sha.h>

namespace picaresque::auth {
namespace {

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
