#pragma once

#include "picaresque/embedded_query/query_validator.hpp"

#include "query_execution_types.hpp"

#include <string>
#include <vector>

#include <json/json.h>

namespace picaresque::embedded_query {

std::string AggregateOutputName(const QueryValidationResult::AggregateSelection& aggregate, bool include_table_id);
Json::Value ComputeAggregate(
    const std::vector<RowContext>& contexts,
    const QueryValidationResult::AggregateSelection& aggregate);
std::string BuildGroupKey(
    const RowContext& context,
    const std::vector<QueryValidationResult::ColumnRef>& group_by_columns);

}  // namespace picaresque::embedded_query
