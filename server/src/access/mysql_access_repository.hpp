#pragma once

#include "picaresque/access/repository.hpp"

namespace picaresque::access {

AccessRepository& GetMySqlAccessRepository();

}  // namespace picaresque::access
