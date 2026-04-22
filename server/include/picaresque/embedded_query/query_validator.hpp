#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace picaresque::embedded_query {

struct QueryValidationConfig {
  std::size_t max_sql_length = 4000;
  std::uint64_t max_limit = 500;
};

struct QueryValidationResult {
  struct ColumnRef {
    std::string table_id;
    std::string column_name;
  };

  struct JoinCondition {
    std::string table_id;
    ColumnRef left;
    ColumnRef right;
  };

  struct WhereCondition {
    ColumnRef left;
    std::string op;
    std::string literal;
  };

  struct OrderBy {
    ColumnRef column;
    bool descending = false;
  };

  std::string table_id;
  std::vector<std::string> table_ids;
  std::uint64_t limit = 0;
  bool select_all = false;
  bool count_all = false;
  std::vector<ColumnRef> selected_columns;
  std::vector<JoinCondition> joins;
  std::optional<WhereCondition> where;
  std::optional<OrderBy> order_by;
};

std::optional<QueryValidationResult> ValidateSelectQuery(
    const std::string& sql,
    const QueryValidationConfig& config,
    std::string& error_message);

}  // namespace picaresque::embedded_query
