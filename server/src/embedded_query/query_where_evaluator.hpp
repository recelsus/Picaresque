#pragma once

#include "picaresque/embedded_query/query_validator.hpp"

#include "query_execution_types.hpp"

#include <vector>

namespace picaresque::embedded_query {

bool EvaluateWhereExpression(const RowContext& context, const QueryValidationResult::WhereExpression& expression);
std::vector<RowContext> ApplyWhere(std::vector<RowContext> contexts, const QueryValidationResult::WhereExpression& where);

}  // namespace picaresque::embedded_query
