#include "picaresque/embedded_query/query_validator.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace picaresque::embedded_query {
namespace {

std::string Trim(const std::string& value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string ToLower(std::string value) {
  std::transform(
      value.begin(),
      value.end(),
      value.begin(),
      [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
  return value;
}

bool ContainsWord(const std::string& sql, const std::string& word) {
  const std::regex pattern("\\b" + word + "\\b", std::regex_constants::icase);
  return std::regex_search(sql, pattern);
}

std::vector<std::string> SplitColumns(const std::string& select_list) {
  std::vector<std::string> columns;
  std::stringstream stream(select_list);
  std::string column;
  while (std::getline(stream, column, ',')) {
    columns.push_back(Trim(column));
  }
  return columns;
}

bool IsIdentifier(const std::string& value) {
  if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_')) {
    return false;
  }
  return std::all_of(
      value.begin() + 1,
      value.end(),
      [](unsigned char c) {
        return std::isalnum(c) || c == '_';
      });
}

QueryValidationResult::ColumnRef ParseColumnRef(
    const std::string& value,
    const std::string& default_table_id,
    std::string& error_message) {
  const auto dot = value.find('.');
  if (dot == std::string::npos) {
    if (!IsIdentifier(value)) {
      error_message = "unsupported column reference";
      return {};
    }
    return {.table_id = default_table_id, .column_name = value};
  }
  const auto table_id = value.substr(0, dot);
  const auto column_name = value.substr(dot + 1);
  if (!IsIdentifier(table_id) || !IsIdentifier(column_name)) {
    error_message = "unsupported column reference";
    return {};
  }
  return {.table_id = table_id, .column_name = column_name};
}

std::optional<QueryValidationResult::WhereCondition> ParseWhere(
    const std::string& where_clause,
    const std::string& default_table_id,
    std::string& error_message) {
  const std::regex pattern("^([A-Za-z_][A-Za-z0-9_]*(?:\\.[A-Za-z_][A-Za-z0-9_]*)?)\\s*(=|!=|<=|>=|<|>)\\s*('([^']*)'|[A-Za-z0-9_.-]+)$");
  std::smatch match;
  if (!std::regex_match(where_clause, match, pattern)) {
    error_message = "unsupported WHERE";
    return std::nullopt;
  }
  auto literal = match[4].matched ? match[4].str() : match[3].str();
  return QueryValidationResult::WhereCondition{
      .left = ParseColumnRef(match[1].str(), default_table_id, error_message),
      .op = match[2].str(),
      .literal = literal,
  };
}

std::optional<QueryValidationResult::OrderBy> ParseOrderBy(
    const std::string& order_by_clause,
    const std::string& default_table_id,
    std::string& error_message) {
  const std::regex pattern("^([A-Za-z_][A-Za-z0-9_]*(?:\\.[A-Za-z_][A-Za-z0-9_]*)?)(?:\\s+(asc|desc))?$");
  std::smatch match;
  if (!std::regex_match(order_by_clause, match, pattern)) {
    error_message = "unsupported ORDER BY";
    return std::nullopt;
  }
  return QueryValidationResult::OrderBy{
      .column = ParseColumnRef(match[1].str(), default_table_id, error_message),
      .descending = match[2].matched && match[2].str() == "desc",
  };
}

std::optional<std::uint64_t> ExtractLimit(std::string& query_body, const QueryValidationConfig& config, std::string& error_message) {
  const std::regex limit_pattern("\\s+limit\\s+([0-9]+)\\s*$", std::regex_constants::icase);
  std::smatch match;
  if (!std::regex_search(query_body, match, limit_pattern)) {
    return 1;
  }
  const auto limit = std::stoull(match[1].str());
  if (limit == 0 || limit > config.max_limit) {
    error_message = "row limit exceeded";
    return std::nullopt;
  }
  query_body = Trim(query_body.substr(0, match.position()));
  return limit;
}

}  // namespace

std::optional<QueryValidationResult> ValidateSelectQuery(
    const std::string& sql,
    const QueryValidationConfig& config,
    std::string& error_message) {
  const auto trimmed = Trim(sql);
  if (trimmed.empty()) {
    error_message = "query is empty";
    return std::nullopt;
  }
  if (trimmed.size() > config.max_sql_length) {
    error_message = "query is too long";
    return std::nullopt;
  }
  if (trimmed.find("--") != std::string::npos || trimmed.find("/*") != std::string::npos ||
      trimmed.find("*/") != std::string::npos || trimmed.find('#') != std::string::npos) {
    error_message = "comments are not allowed";
    return std::nullopt;
  }
  const auto semicolon = trimmed.find(';');
  if (semicolon != std::string::npos && semicolon != trimmed.size() - 1) {
    error_message = "multiple statements are not allowed";
    return std::nullopt;
  }

  const auto normalized = ToLower(semicolon == trimmed.size() - 1 ? trimmed.substr(0, trimmed.size() - 1) : trimmed);
  if (normalized.rfind("select ", 0) != 0 && normalized != "select") {
    error_message = "only SELECT is allowed";
    return std::nullopt;
  }

  static const std::unordered_set<std::string> forbidden_words = {
      "insert", "update", "delete", "drop", "alter", "create", "replace", "truncate", "grant", "revoke"};
  for (const auto& word : forbidden_words) {
    if (ContainsWord(normalized, word)) {
      error_message = "only SELECT is allowed";
      return std::nullopt;
    }
  }

  std::string query_body = normalized;
  const auto limit = ExtractLimit(query_body, config, error_message);
  if (!limit.has_value()) {
    return std::nullopt;
  }

  const std::regex select_pattern("^select\\s+(.+)\\s+from\\s+([A-Za-z_][A-Za-z0-9_]*)(.*)$");
  std::smatch select_match;
  if (!std::regex_match(query_body, select_match, select_pattern)) {
    error_message = "table not found";
    return std::nullopt;
  }

  const auto select_list = Trim(select_match[1].str());
  const auto table_id = select_match[2].str();
  auto tail = Trim(select_match[3].str());

  QueryValidationResult result{
      .table_id = table_id,
      .table_ids = {table_id},
      .limit = *limit,
  };
  if (select_list == "*") {
    result.select_all = true;
  } else if (select_list == "count(*)") {
    result.count_all = true;
    result.selected_columns = {{.table_id = "", .column_name = "count"}};
  } else {
    const auto columns = SplitColumns(select_list);
    if (columns.empty()) {
      error_message = "unsupported select list";
      return std::nullopt;
    }
    for (const auto& column : columns) {
      const auto parsed_column = ParseColumnRef(column, table_id, error_message);
      if (!error_message.empty()) {
        return std::nullopt;
      }
      result.selected_columns.push_back(parsed_column);
    }
  }

  while (tail.rfind("join ", 0) == 0) {
    const std::regex join_pattern("^join\\s+([A-Za-z_][A-Za-z0-9_]*)\\s+on\\s+([A-Za-z_][A-Za-z0-9_]*(?:\\.[A-Za-z_][A-Za-z0-9_]*)?)\\s*=\\s*([A-Za-z_][A-Za-z0-9_]*(?:\\.[A-Za-z_][A-Za-z0-9_]*)?)(.*)$");
    std::smatch join_match;
    if (!std::regex_match(tail, join_match, join_pattern)) {
      error_message = "unsupported JOIN";
      return std::nullopt;
    }
    const auto joined_table_id = join_match[1].str();
    auto left = ParseColumnRef(join_match[2].str(), table_id, error_message);
    if (!error_message.empty()) {
      return std::nullopt;
    }
    auto right = ParseColumnRef(join_match[3].str(), joined_table_id, error_message);
    if (!error_message.empty()) {
      return std::nullopt;
    }
    result.table_ids.push_back(joined_table_id);
    result.joins.push_back({
        .table_id = joined_table_id,
        .left = left,
        .right = right,
    });
    tail = Trim(join_match[4].str());
  }

  const auto where_pos = tail.find("where ");
  const auto order_pos = tail.find("order by ");
  if (where_pos != std::string::npos) {
    if (where_pos != 0) {
      error_message = "unsupported query tail";
      return std::nullopt;
    }
    const auto where_end = order_pos == std::string::npos ? tail.size() : order_pos;
    result.where = ParseWhere(Trim(tail.substr(6, where_end - 6)), table_id, error_message);
    if (!result.where.has_value()) {
      return std::nullopt;
    }
  }
  if (order_pos != std::string::npos) {
    if (where_pos == std::string::npos && order_pos != 0) {
      error_message = "unsupported query tail";
      return std::nullopt;
    }
    result.order_by = ParseOrderBy(Trim(tail.substr(order_pos + 9)), table_id, error_message);
    if (!result.order_by.has_value()) {
      return std::nullopt;
    }
  }
  if (where_pos == std::string::npos && order_pos == std::string::npos && !tail.empty()) {
    error_message = "unsupported query tail";
    return std::nullopt;
  }
  return result;
}

}  // namespace picaresque::embedded_query
