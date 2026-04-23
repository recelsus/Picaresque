#pragma once

#include <cstdint>
#include <memory>
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
    bool qualified = false;
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
    std::vector<std::string> literals;
    bool negate = false;
  };

  struct WhereExpression {
    enum class Kind {
      Condition,
      And,
      Or,
    };

    Kind kind = Kind::Condition;
    WhereCondition condition;
    std::unique_ptr<WhereExpression> left;
    std::unique_ptr<WhereExpression> right;

    WhereExpression() = default;
    WhereExpression(const WhereExpression& other);
    WhereExpression& operator=(const WhereExpression& other);
    WhereExpression(WhereExpression&&) noexcept = default;
    WhereExpression& operator=(WhereExpression&&) noexcept = default;
  };

  struct OrderBy {
    ColumnRef column;
    bool descending = false;
  };

  struct AggregateSelection {
    std::string function_name;
    ColumnRef column;
    bool count_star = false;
  };

  std::string table_id;
  std::vector<std::string> table_ids;
  std::uint64_t limit = 0;
  bool select_all = false;
  bool count_all = false;
  std::vector<ColumnRef> selected_columns;
  std::vector<AggregateSelection> aggregate_selections;
  std::vector<ColumnRef> group_by_columns;
  std::vector<JoinCondition> joins;
  std::optional<WhereExpression> where;
  std::optional<OrderBy> order_by;
};

std::optional<QueryValidationResult> ValidateSelectQuery(
    const std::string& sql,
    const QueryValidationConfig& config,
    std::string& error_message);

}  // namespace picaresque::embedded_query
