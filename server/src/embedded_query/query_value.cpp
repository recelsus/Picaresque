#include "query_value.hpp"

#include <vector>

namespace picaresque::embedded_query {

std::string ColumnOutputName(const QueryValidationResult::ColumnRef& column, bool include_table_id) {
  if (include_table_id) {
    return column.table_id + "." + column.column_name;
  }
  return column.column_name;
}

Json::Value GetColumnValue(const RowContext& context, const QueryValidationResult::ColumnRef& column) {
  const auto table_it = context.find(column.table_id);
  if (table_it == context.end() || !table_it->second.isMember(column.column_name)) {
    return Json::Value();
  }
  return table_it->second[column.column_name];
}

std::string JsonValueToComparableString(const Json::Value& value) {
  if (value.isString()) {
    return value.asString();
  }
  if (value.isBool()) {
    return value.asBool() ? "true" : "false";
  }
  if (value.isNumeric()) {
    return value.asString();
  }
  return "";
}

bool CompareValues(const Json::Value& left, const std::string& op, const Json::Value& right) {
  if (left.isNumeric() && right.isNumeric()) {
    const auto lhs = left.asDouble();
    const auto rhs = right.asDouble();
    if (op == "=") return lhs == rhs;
    if (op == "!=") return lhs != rhs;
    if (op == "<") return lhs < rhs;
    if (op == "<=") return lhs <= rhs;
    if (op == ">") return lhs > rhs;
    if (op == ">=") return lhs >= rhs;
  }
  const auto lhs = JsonValueToComparableString(left);
  const auto rhs = JsonValueToComparableString(right);
  if (op == "=") return lhs == rhs;
  if (op == "!=") return lhs != rhs;
  if (op == "<") return lhs < rhs;
  if (op == "<=") return lhs <= rhs;
  if (op == ">") return lhs > rhs;
  if (op == ">=") return lhs >= rhs;
  return false;
}

bool LikeMatches(const std::string& value, const std::string& pattern) {
  std::vector<std::vector<bool>> dp(value.size() + 1, std::vector<bool>(pattern.size() + 1, false));
  dp[0][0] = true;
  for (std::size_t j = 1; j <= pattern.size(); ++j) {
    if (pattern[j - 1] == '%') {
      dp[0][j] = dp[0][j - 1];
    }
  }
  for (std::size_t i = 1; i <= value.size(); ++i) {
    for (std::size_t j = 1; j <= pattern.size(); ++j) {
      if (pattern[j - 1] == '%') {
        dp[i][j] = dp[i][j - 1] || dp[i - 1][j];
      } else if (pattern[j - 1] == '_' || pattern[j - 1] == value[i - 1]) {
        dp[i][j] = dp[i - 1][j - 1];
      }
    }
  }
  return dp[value.size()][pattern.size()];
}

Json::Value LiteralToJson(const std::string& literal) {
  if (literal == "true") return Json::Value(true);
  if (literal == "false") return Json::Value(false);
  try {
    std::size_t parsed = 0;
    const auto number = std::stod(literal, &parsed);
    if (parsed == literal.size()) {
      return Json::Value(number);
    }
  } catch (...) {
  }
  return Json::Value(literal);
}

}  // namespace picaresque::embedded_query
