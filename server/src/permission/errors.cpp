#include "picaresque/permission/errors.hpp"

namespace picaresque::permission {

ValidationError::ValidationError(const std::string& message)
    : std::runtime_error(message) {}

}  // namespace picaresque::permission
