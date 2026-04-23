#include "query_where_evaluator.hpp"

#include "query_value.hpp"

#include <algorithm>

namespace picaresque::embedded_query {
namespace {

bool EvaluateWhereCondition(const RowContext& context, const QueryValidationResult::WhereCondition& condition) {
  const auto left = GetColumnValue(context, condition.left);
  if (condition.op == "is_null") {
    const bool is_null = left.isNull();
    return condition.negate ? !is_null : is_null;
  }
  if (condition.op == "like") {
    return LikeMatches(JsonValueToComparableString(left), condition.literal);
  }
  if (condition.op == "in") {
    return std::any_of(
        condition.literals.begin(),
        condition.literals.end(),
        [&](const std::string& literal) {
          return CompareValues(left, "=", LiteralToJson(literal));
        });
  }
  if (condition.op == "between" && condition.literals.size() == 2) {
    return CompareValues(left, ">=", LiteralToJson(condition.literals[0])) &&
        CompareValues(left, "<=", LiteralToJson(condition.literals[1]));
  }
  const auto literal = LiteralToJson(condition.literal);
  return CompareValues(left, condition.op, literal);
}

}  // namespace

bool EvaluateWhereExpression(const RowContext& context, const QueryValidationResult::WhereExpression& expression) {
  switch (expression.kind) {
    case QueryValidationResult::WhereExpression::Kind::Condition:
      return EvaluateWhereCondition(context, expression.condition);
    case QueryValidationResult::WhereExpression::Kind::And:
      return expression.left != nullptr && expression.right != nullptr &&
          EvaluateWhereExpression(context, *expression.left) &&
          EvaluateWhereExpression(context, *expression.right);
    case QueryValidationResult::WhereExpression::Kind::Or:
      return expression.left != nullptr && expression.right != nullptr &&
          (EvaluateWhereExpression(context, *expression.left) ||
           EvaluateWhereExpression(context, *expression.right));
  }
  return false;
}

std::vector<RowContext> ApplyWhere(std::vector<RowContext> contexts, const QueryValidationResult::WhereExpression& where) {
  contexts.erase(
      std::remove_if(
          contexts.begin(),
          contexts.end(),
          [&](const RowContext& context) {
            return !EvaluateWhereExpression(context, where);
          }),
      contexts.end());
  return contexts;
}

}  // namespace picaresque::embedded_query
