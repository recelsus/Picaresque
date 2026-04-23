#pragma once

#include "picaresque/embedded_query/query_validator.hpp"

#include "query_execution_types.hpp"

#include <string>

#include <json/json.h>

namespace picaresque::embedded_query {

std::string ColumnOutputName(const QueryValidationResult::ColumnRef& column, bool include_table_id);
Json::Value GetColumnValue(const RowContext& context, const QueryValidationResult::ColumnRef& column);
std::string JsonValueToComparableString(const Json::Value& value);
bool CompareValues(const Json::Value& left, const std::string& op, const Json::Value& right);
bool LikeMatches(const std::string& value, const std::string& pattern);
Json::Value LiteralToJson(const std::string& literal);

}  // namespace picaresque::embedded_query
