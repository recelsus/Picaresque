#pragma once

#include <string>
#include <string_view>

namespace picaresque::auth {

std::string HashApiKey(std::string_view api_key);
std::string HashPassword(std::string_view password);
std::string HashSessionToken(std::string_view session_token);

}  // namespace picaresque::auth
