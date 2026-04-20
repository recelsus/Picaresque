#pragma once

#include <cstdint>

namespace picaresque::permission {

inline constexpr std::uint8_t kDefaultPermission = 10;
inline constexpr std::uint8_t kManagerPermission = 90;
inline constexpr std::uint8_t kPrivilegedPermission = 99;

}  // namespace picaresque::permission
