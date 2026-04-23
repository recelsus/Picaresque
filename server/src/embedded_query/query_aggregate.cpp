#include "query_aggregate.hpp"

#include "query_value.hpp"

namespace picaresque::embedded_query {

std::string AggregateOutputName(const QueryValidationResult::AggregateSelection& aggregate, bool include_table_id) {
  if (aggregate.count_star) {
    return "count";
  }
  return aggregate.function_name + "(" + ColumnOutputName(aggregate.column, include_table_id) + ")";
}

Json::Value ComputeAggregate(
    const std::vector<RowContext>& contexts,
    const QueryValidationResult::AggregateSelection& aggregate) {
  if (aggregate.function_name == "count") {
    if (aggregate.count_star) {
      return Json::Value(static_cast<Json::UInt64>(contexts.size()));
    }
    Json::UInt64 count = 0;
    for (const auto& context : contexts) {
      if (!GetColumnValue(context, aggregate.column).isNull()) {
        ++count;
      }
    }
    return Json::Value(count);
  }

  double sum = 0.0;
  Json::UInt64 numeric_count = 0;
  bool has_value = false;
  Json::Value min_value;
  Json::Value max_value;
  for (const auto& context : contexts) {
    const auto value = GetColumnValue(context, aggregate.column);
    if (value.isNull()) {
      continue;
    }
    if (aggregate.function_name == "sum" || aggregate.function_name == "avg") {
      if (!value.isNumeric()) {
        continue;
      }
      sum += value.asDouble();
      ++numeric_count;
      continue;
    }
    if (!has_value) {
      min_value = value;
      max_value = value;
      has_value = true;
      continue;
    }
    if (CompareValues(value, "<", min_value)) {
      min_value = value;
    }
    if (CompareValues(value, ">", max_value)) {
      max_value = value;
    }
  }

  if (aggregate.function_name == "sum") {
    return Json::Value(sum);
  }
  if (aggregate.function_name == "avg") {
    return numeric_count == 0 ? Json::Value() : Json::Value(sum / static_cast<double>(numeric_count));
  }
  if (aggregate.function_name == "min") {
    return has_value ? min_value : Json::Value();
  }
  if (aggregate.function_name == "max") {
    return has_value ? max_value : Json::Value();
  }
  return Json::Value();
}

std::string BuildGroupKey(
    const RowContext& context,
    const std::vector<QueryValidationResult::ColumnRef>& group_by_columns) {
  std::string key;
  for (const auto& column : group_by_columns) {
    key.append(JsonValueToComparableString(GetColumnValue(context, column)));
    key.push_back('\x1f');
  }
  return key;
}

}  // namespace picaresque::embedded_query
