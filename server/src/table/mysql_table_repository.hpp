#pragma once

#include "picaresque/table/table_repository.hpp"

namespace picaresque::table {

TableRepository& GetMySqlTableRepository();

}  // namespace picaresque::table
