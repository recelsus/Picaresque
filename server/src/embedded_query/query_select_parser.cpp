#include "query_select_parser.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace picaresque::embedded_query {
namespace {

bool IsDeclaredTable(const QueryValidationResult& result, const std::string& table_id) {
  return std::find(result.table_ids.begin(), result.table_ids.end(), table_id) != result.table_ids.end();
}

class Parser {
 public:
  Parser(std::vector<Token> tokens, const QueryValidationConfig& config)
      : tokens_(std::move(tokens)), config_(config) {}

  std::optional<QueryValidationResult> Parse(std::string& error_message) {
    if (!ConsumeKeyword("select")) {
      error_message = "only SELECT is allowed";
      return std::nullopt;
    }

    QueryValidationResult result;
    if (!ParseSelectList(result, error_message)) {
      return std::nullopt;
    }
    if (!ConsumeKeyword("from")) {
      error_message = "table not found";
      return std::nullopt;
    }
    const auto base_table = ConsumeIdentifier();
    if (!base_table.has_value()) {
      error_message = "table not found";
      return std::nullopt;
    }
    result.table_id = *base_table;
    result.table_ids.push_back(*base_table);
    if (!result.select_all) {
      for (auto& column : result.selected_columns) {
        if (column.table_id.empty()) {
          column.table_id = result.table_id;
        }
      }
      for (auto& aggregate : result.aggregate_selections) {
        if (!aggregate.count_star && aggregate.column.table_id.empty()) {
          aggregate.column.table_id = result.table_id;
        }
      }
    }

    while (ConsumeKeyword("join")) {
      if (!ParseJoin(result, error_message)) {
        return std::nullopt;
      }
    }

    const bool require_qualified = result.table_ids.size() > 1;
    if (require_qualified && !EnsureSelectedColumnsQualified(result, error_message)) {
      return std::nullopt;
    }

    if (ConsumeKeyword("where")) {
      auto where = ParseOrExpression(result.table_id, require_qualified, error_message);
      if (!where.has_value()) {
        return std::nullopt;
      }
      result.where = *where;
    }

    if (ConsumeKeyword("group")) {
      if (!ConsumeKeyword("by") || !ParseGroupBy(result, require_qualified, error_message)) {
        error_message = error_message.empty() ? "unsupported GROUP BY" : error_message;
        return std::nullopt;
      }
    }

    if (!ValidateAggregateAndGroupBy(result, error_message)) {
      return std::nullopt;
    }

    if (ConsumeKeyword("order")) {
      if (!ConsumeKeyword("by")) {
        error_message = "unsupported ORDER BY";
        return std::nullopt;
      }
      auto order_by = ParseOrderBy(result.table_id, require_qualified, error_message);
      if (!order_by.has_value()) {
        return std::nullopt;
      }
      result.order_by = *order_by;
    }

    if (ConsumeKeyword("limit")) {
      const auto limit = ConsumeNumber();
      if (!limit.has_value()) {
        error_message = "row limit exceeded";
        return std::nullopt;
      }
      result.limit = *limit;
      if (result.limit == 0 || result.limit > config_.max_limit) {
        error_message = "row limit exceeded";
        return std::nullopt;
      }
    } else {
      result.limit = 1;
    }

    if (Match(TokenKind::Semicolon)) {
      Advance();
      if (!Match(TokenKind::End)) {
        error_message = "multiple statements are not allowed";
        return std::nullopt;
      }
    }
    if (!Match(TokenKind::End)) {
      error_message = "unsupported query tail";
      return std::nullopt;
    }
    if (!ValidateColumnRefTables(result, error_message)) {
      return std::nullopt;
    }
    return result;
  }

 private:
  bool ParseSelectList(QueryValidationResult& result, std::string& error_message) {
    if (Match(TokenKind::Star)) {
      result.select_all = true;
      Advance();
      return true;
    }
    while (true) {
      if (IsAggregateFunction(Current())) {
        auto aggregate = ParseAggregateSelection(error_message);
        if (!aggregate.has_value()) {
          return false;
        }
        if (aggregate->function_name == "count" && aggregate->count_star) {
          result.count_all = true;
        }
        result.aggregate_selections.push_back(*aggregate);
      } else {
        auto column = ParseColumnRef("", false, error_message);
        if (!column.has_value()) {
          error_message = error_message.empty() ? "unsupported select list" : error_message;
          return false;
        }
        result.selected_columns.push_back(*column);
      }
      if (!Consume(TokenKind::Comma)) {
        break;
      }
    }
    return !result.selected_columns.empty() || !result.aggregate_selections.empty();
  }

  std::optional<QueryValidationResult::AggregateSelection> ParseAggregateSelection(std::string& error_message) {
    const auto function_name = Current().text;
    Advance();
    if (!Consume(TokenKind::LeftParen)) {
      error_message = "unsupported select list";
      return std::nullopt;
    }
    QueryValidationResult::AggregateSelection selection{
        .function_name = function_name,
    };
    if (function_name == "count" && Consume(TokenKind::Star)) {
      selection.count_star = true;
    } else {
      auto column = ParseColumnRef("", false, error_message);
      if (!column.has_value()) {
        error_message = error_message.empty() ? "unsupported select list" : error_message;
        return std::nullopt;
      }
      selection.column = *column;
    }
    if (!Consume(TokenKind::RightParen)) {
      error_message = "unsupported select list";
      return std::nullopt;
    }
    return selection;
  }

  bool ParseJoin(QueryValidationResult& result, std::string& error_message) {
    const auto joined_table = ConsumeIdentifier();
    if (!joined_table.has_value()) {
      error_message = "unsupported JOIN";
      return false;
    }
    if (!ConsumeKeyword("on")) {
      error_message = "unsupported JOIN";
      return false;
    }
    auto left = ParseColumnRef(result.table_id, true, error_message);
    if (!left.has_value()) {
      return false;
    }
    if (!ConsumeOperator("=")) {
      error_message = "unsupported JOIN";
      return false;
    }
    auto right = ParseColumnRef(*joined_table, true, error_message);
    if (!right.has_value()) {
      return false;
    }
    result.table_ids.push_back(*joined_table);
    result.joins.push_back({
        .table_id = *joined_table,
        .left = *left,
        .right = *right,
    });
    return true;
  }

  std::optional<QueryValidationResult::WhereExpression> ParseOrExpression(
      const std::string& default_table_id,
      bool require_qualified,
      std::string& error_message) {
    auto left = ParseAndExpression(default_table_id, require_qualified, error_message);
    if (!left.has_value()) {
      return std::nullopt;
    }
    while (ConsumeKeyword("or")) {
      auto right = ParseAndExpression(default_table_id, require_qualified, error_message);
      if (!right.has_value()) {
        return std::nullopt;
      }
      QueryValidationResult::WhereExpression expression;
      expression.kind = QueryValidationResult::WhereExpression::Kind::Or;
      expression.left = std::make_unique<QueryValidationResult::WhereExpression>(std::move(*left));
      expression.right = std::make_unique<QueryValidationResult::WhereExpression>(std::move(*right));
      left = std::move(expression);
    }
    return left;
  }

  std::optional<QueryValidationResult::WhereExpression> ParseAndExpression(
      const std::string& default_table_id,
      bool require_qualified,
      std::string& error_message) {
    auto left = ParseWherePrimary(default_table_id, require_qualified, error_message);
    if (!left.has_value()) {
      return std::nullopt;
    }
    while (ConsumeKeyword("and")) {
      auto right = ParseWherePrimary(default_table_id, require_qualified, error_message);
      if (!right.has_value()) {
        return std::nullopt;
      }
      QueryValidationResult::WhereExpression expression;
      expression.kind = QueryValidationResult::WhereExpression::Kind::And;
      expression.left = std::make_unique<QueryValidationResult::WhereExpression>(std::move(*left));
      expression.right = std::make_unique<QueryValidationResult::WhereExpression>(std::move(*right));
      left = std::move(expression);
    }
    return left;
  }

  std::optional<QueryValidationResult::WhereExpression> ParseWherePrimary(
      const std::string& default_table_id,
      bool require_qualified,
      std::string& error_message) {
    if (Consume(TokenKind::LeftParen)) {
      auto nested = ParseOrExpression(default_table_id, require_qualified, error_message);
      if (!nested.has_value()) {
        return std::nullopt;
      }
      if (!Consume(TokenKind::RightParen)) {
        error_message = "unsupported WHERE";
        return std::nullopt;
      }
      return nested;
    }
    auto condition = ParseWhereCondition(default_table_id, require_qualified, error_message);
    if (!condition.has_value()) {
      return std::nullopt;
    }
    QueryValidationResult::WhereExpression expression;
    expression.kind = QueryValidationResult::WhereExpression::Kind::Condition;
    expression.condition = *condition;
    return expression;
  }

  std::optional<QueryValidationResult::WhereCondition> ParseWhereCondition(
      const std::string& default_table_id,
      bool require_qualified,
      std::string& error_message) {
    auto left = ParseColumnRef(default_table_id, require_qualified, error_message);
    if (!left.has_value()) {
      error_message = error_message.empty() ? "unsupported WHERE" : error_message;
      return std::nullopt;
    }
    if (ConsumeKeyword("is")) {
      bool negate = ConsumeKeyword("not");
      if (!ConsumeKeyword("null")) {
        error_message = "unsupported WHERE";
        return std::nullopt;
      }
      return QueryValidationResult::WhereCondition{
          .left = *left,
          .op = "is_null",
          .negate = negate,
      };
    }
    if (ConsumeKeyword("like")) {
      auto literal = ConsumeLiteral();
      if (!literal.has_value()) {
        error_message = "unsupported WHERE";
        return std::nullopt;
      }
      return QueryValidationResult::WhereCondition{
          .left = *left,
          .op = "like",
          .literal = *literal,
      };
    }
    if (ConsumeKeyword("in")) {
      if (!Consume(TokenKind::LeftParen)) {
        error_message = "unsupported WHERE";
        return std::nullopt;
      }
      std::vector<std::string> literals;
      do {
        auto literal = ConsumeLiteral();
        if (!literal.has_value()) {
          error_message = "unsupported WHERE";
          return std::nullopt;
        }
        literals.push_back(*literal);
      } while (Consume(TokenKind::Comma));
      if (!Consume(TokenKind::RightParen) || literals.empty()) {
        error_message = "unsupported WHERE";
        return std::nullopt;
      }
      return QueryValidationResult::WhereCondition{
          .left = *left,
          .op = "in",
          .literals = literals,
      };
    }
    if (ConsumeKeyword("between")) {
      auto lower = ConsumeLiteral();
      if (!lower.has_value() || !ConsumeKeyword("and")) {
        error_message = "unsupported WHERE";
        return std::nullopt;
      }
      auto upper = ConsumeLiteral();
      if (!upper.has_value()) {
        error_message = "unsupported WHERE";
        return std::nullopt;
      }
      return QueryValidationResult::WhereCondition{
          .left = *left,
          .op = "between",
          .literals = {*lower, *upper},
      };
    }
    if (!Match(TokenKind::Operator)) {
      error_message = "unsupported WHERE";
      return std::nullopt;
    }
    const auto op = Current().text;
    Advance();
    auto literal = ConsumeLiteral();
    if (!literal.has_value()) {
      error_message = "unsupported WHERE";
      return std::nullopt;
    }
    return QueryValidationResult::WhereCondition{
        .left = *left,
        .op = op,
        .literal = *literal,
    };
  }

  std::optional<QueryValidationResult::OrderBy> ParseOrderBy(
      const std::string& default_table_id,
      bool require_qualified,
      std::string& error_message) {
    auto column = ParseColumnRef(default_table_id, require_qualified, error_message);
    if (!column.has_value()) {
      error_message = error_message.empty() ? "unsupported ORDER BY" : error_message;
      return std::nullopt;
    }
    bool descending = false;
    if (ConsumeKeyword("asc")) {
      descending = false;
    } else if (ConsumeKeyword("desc")) {
      descending = true;
    }
    return QueryValidationResult::OrderBy{
        .column = *column,
        .descending = descending,
    };
  }

  bool ParseGroupBy(QueryValidationResult& result, bool require_qualified, std::string& error_message) {
    while (true) {
      auto column = ParseColumnRef(result.table_id, require_qualified, error_message);
      if (!column.has_value()) {
        error_message = error_message.empty() ? "unsupported GROUP BY" : error_message;
        return false;
      }
      result.group_by_columns.push_back(*column);
      if (!Consume(TokenKind::Comma)) {
        break;
      }
    }
    return !result.group_by_columns.empty();
  }

  std::optional<QueryValidationResult::ColumnRef> ParseColumnRef(
      const std::string& default_table_id,
      bool require_qualified,
      std::string& error_message) {
    const auto first = ConsumeIdentifier();
    if (!first.has_value()) {
      error_message = "unsupported column reference";
      return std::nullopt;
    }
    if (!Consume(TokenKind::Dot)) {
      if (require_qualified) {
        error_message = "qualified column reference required";
        return std::nullopt;
      }
      return QueryValidationResult::ColumnRef{.table_id = default_table_id, .column_name = *first, .qualified = false};
    }
    const auto second = ConsumeIdentifier();
    if (!second.has_value()) {
      error_message = "unsupported column reference";
      return std::nullopt;
    }
    return QueryValidationResult::ColumnRef{.table_id = *first, .column_name = *second, .qualified = true};
  }

  bool EnsureSelectedColumnsQualified(const QueryValidationResult& result, std::string& error_message) const {
    if (result.select_all || result.count_all) {
      return true;
    }
    for (const auto& column : result.selected_columns) {
      if (!column.qualified) {
        error_message = "qualified column reference required";
        return false;
      }
    }
    for (const auto& aggregate : result.aggregate_selections) {
      if (!aggregate.count_star && !aggregate.column.qualified) {
        error_message = "qualified column reference required";
        return false;
      }
    }
    return true;
  }

  bool SameColumn(const QueryValidationResult::ColumnRef& lhs, const QueryValidationResult::ColumnRef& rhs) const {
    return lhs.table_id == rhs.table_id && lhs.column_name == rhs.column_name;
  }

  bool ValidateAggregateAndGroupBy(const QueryValidationResult& result, std::string& error_message) const {
    if (result.aggregate_selections.empty()) {
      if (!result.group_by_columns.empty()) {
        error_message = "unsupported GROUP BY";
        return false;
      }
      return true;
    }
    if (result.select_all) {
      error_message = "unsupported select list";
      return false;
    }
    if (result.selected_columns.empty()) {
      return true;
    }
    if (result.group_by_columns.empty()) {
      error_message = "GROUP BY required";
      return false;
    }
    for (const auto& selected : result.selected_columns) {
      const auto grouped = std::any_of(
          result.group_by_columns.begin(),
          result.group_by_columns.end(),
          [&](const QueryValidationResult::ColumnRef& group_column) {
            return SameColumn(selected, group_column);
          });
      if (!grouped) {
        error_message = "selected column must appear in GROUP BY";
        return false;
      }
    }
    return true;
  }

  bool ValidateColumnRefTables(const QueryValidationResult& result, std::string& error_message) const {
    for (const auto& column : result.selected_columns) {
      if (!column.table_id.empty() && !IsDeclaredTable(result, column.table_id)) {
        error_message = "undeclared table reference";
        return false;
      }
    }
    for (const auto& aggregate : result.aggregate_selections) {
      if (!aggregate.count_star && !aggregate.column.table_id.empty() &&
          !IsDeclaredTable(result, aggregate.column.table_id)) {
        error_message = "undeclared table reference";
        return false;
      }
    }
    for (const auto& column : result.group_by_columns) {
      if (!column.table_id.empty() && !IsDeclaredTable(result, column.table_id)) {
        error_message = "undeclared table reference";
        return false;
      }
    }
    for (const auto& join : result.joins) {
      if (!IsDeclaredTable(result, join.left.table_id) || !IsDeclaredTable(result, join.right.table_id)) {
        error_message = "undeclared table reference";
        return false;
      }
    }
    if (result.where.has_value()) {
      bool valid = true;
      ValidateWhereExpressionTables(*result.where, result, error_message, valid);
      if (!valid) {
        return false;
      }
    }
    if (result.order_by.has_value() && !IsDeclaredTable(result, result.order_by->column.table_id)) {
      error_message = "undeclared table reference";
      return false;
    }
    return true;
  }

  void ValidateWhereExpressionTables(
      const QueryValidationResult::WhereExpression& expression,
      const QueryValidationResult& result,
      std::string& error_message,
      bool& valid) const {
    if (!valid) {
      return;
    }
    if (expression.kind == QueryValidationResult::WhereExpression::Kind::Condition) {
      if (!IsDeclaredTable(result, expression.condition.left.table_id)) {
        error_message = "undeclared table reference";
        valid = false;
      }
      return;
    }
    if (expression.left != nullptr) {
      ValidateWhereExpressionTables(*expression.left, result, error_message, valid);
    }
    if (expression.right != nullptr) {
      ValidateWhereExpressionTables(*expression.right, result, error_message, valid);
    }
  }

  bool Match(TokenKind kind) const {
    return Current().kind == kind;
  }

  bool Consume(TokenKind kind) {
    if (!Match(kind)) {
      return false;
    }
    Advance();
    return true;
  }

  bool ConsumeKeyword(const std::string& keyword) {
    if (!IsKeyword(Current(), keyword)) {
      return false;
    }
    Advance();
    return true;
  }

  bool ConsumeOperator(const std::string& op) {
    if (!Match(TokenKind::Operator) || Current().text != op) {
      return false;
    }
    Advance();
    return true;
  }

  bool IsAggregateFunction(const Token& token) const {
    return IsKeyword(token, "count") || IsKeyword(token, "sum") || IsKeyword(token, "avg") ||
        IsKeyword(token, "min") || IsKeyword(token, "max");
  }

  std::optional<std::string> ConsumeIdentifier() {
    if (!Match(TokenKind::Identifier)) {
      return std::nullopt;
    }
    const auto value = Current().text;
    Advance();
    return value;
  }

  std::optional<std::uint64_t> ConsumeNumber() {
    if (!Match(TokenKind::Number)) {
      return std::nullopt;
    }
    const auto value = Current().text;
    Advance();
    if (value.find('.') != std::string::npos) {
      return std::nullopt;
    }
    try {
      return std::stoull(value);
    } catch (...) {
      return std::nullopt;
    }
  }

  std::optional<std::string> ConsumeLiteral() {
    if (Match(TokenKind::String) || Match(TokenKind::Number) || Match(TokenKind::Identifier)) {
      const auto value = Current().text;
      Advance();
      return value;
    }
    return std::nullopt;
  }

  const Token& Current() const {
    return tokens_[position_];
  }

  void Advance() {
    if (position_ + 1 < tokens_.size()) {
      ++position_;
    }
  }

  std::vector<Token> tokens_;
  const QueryValidationConfig& config_;
  std::size_t position_ = 0;
};

}  // namespace

QueryValidationResult::WhereExpression::WhereExpression(const WhereExpression& other)
    : kind(other.kind),
      condition(other.condition),
      left(other.left == nullptr ? nullptr : std::make_unique<WhereExpression>(*other.left)),
      right(other.right == nullptr ? nullptr : std::make_unique<WhereExpression>(*other.right)) {}

QueryValidationResult::WhereExpression& QueryValidationResult::WhereExpression::operator=(
    const WhereExpression& other) {
  if (this == &other) {
    return *this;
  }
  kind = other.kind;
  condition = other.condition;
  left = other.left == nullptr ? nullptr : std::make_unique<WhereExpression>(*other.left);
  right = other.right == nullptr ? nullptr : std::make_unique<WhereExpression>(*other.right);
  return *this;
}

std::optional<QueryValidationResult> ParseSelectQueryTokens(
    std::vector<Token> tokens,
    const QueryValidationConfig& config,
    std::string& error_message) {
  Parser parser(std::move(tokens), config);
  return parser.Parse(error_message);
}

}  // namespace picaresque::embedded_query
