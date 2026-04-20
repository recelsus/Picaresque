#pragma once

#include <stdexcept>
#include <string>

namespace picaresque::permission {

class ValidationError : public std::runtime_error {
 public:
  explicit ValidationError(const std::string& message);
};

}  // namespace picaresque::permission
