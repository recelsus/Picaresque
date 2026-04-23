#pragma once

#include "picaresque/table/table_types.hpp"

#include <map>
#include <string>

#include <json/json.h>

namespace picaresque::embedded_query {

using RowContext = std::map<std::string, Json::Value>;
using TableDetailsMap = std::map<std::string, table::TableDetails>;

}  // namespace picaresque::embedded_query
