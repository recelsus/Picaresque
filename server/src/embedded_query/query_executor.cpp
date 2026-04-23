#include "picaresque/embedded_query/query_executor.hpp"

#include "query_aggregate.hpp"
#include "query_execution_types.hpp"
#include "query_value.hpp"
#include "query_where_evaluator.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace picaresque::embedded_query {
namespace {

std::vector<std::string> AllColumnNames(const table::TableDetails& table) {
  std::vector<std::string> names;
  names.reserve(table.columns.size());
  for (const auto& column : table.columns) {
    names.push_back(column.column_name);
  }
  return names;
}

std::vector<std::string> AllQualifiedColumnNames(const TableDetailsMap& tables) {
  std::vector<std::string> names;
  for (const auto& [table_id, table] : tables) {
    for (const auto& column : table.columns) {
      names.push_back(table_id + "." + column.column_name);
    }
  }
  return names;
}

void ValidateSelectedColumns(
    const std::vector<QueryValidationResult::ColumnRef>& selected_columns,
    const TableDetailsMap& tables) {
  std::map<std::string, std::unordered_set<std::string>> available_columns;
  for (const auto& [table_id, table] : tables) {
    for (const auto& column : table.columns) {
      available_columns[table_id].insert(column.column_name);
    }
  }
  for (const auto& column : selected_columns) {
    if (!available_columns.contains(column.table_id) ||
        !available_columns[column.table_id].contains(column.column_name)) {
      throw std::runtime_error("unknown_column");
    }
  }
}

std::vector<RowContext> LoadInitialContexts(table::TableRepository& repository, const std::string& table_id) {
  std::vector<RowContext> contexts;
  for (const auto& row : repository.ListRows(table_id)) {
    RowContext context;
    context[table_id] = row.values;
    contexts.push_back(context);
  }
  return contexts;
}

std::vector<RowContext> ApplyJoin(
    table::TableRepository& repository,
    const std::vector<RowContext>& contexts,
    const QueryValidationResult::JoinCondition& join) {
  std::vector<RowContext> joined_contexts;
  for (const auto& row : repository.ListRows(join.table_id)) {
    for (const auto& context : contexts) {
      RowContext candidate = context;
      candidate[join.table_id] = row.values;
      if (CompareValues(GetColumnValue(candidate, join.left), "=", GetColumnValue(candidate, join.right))) {
        joined_contexts.push_back(std::move(candidate));
      }
    }
  }
  return joined_contexts;
}

void SortResultRows(
    Json::Value& rows,
    const QueryValidationResult::OrderBy& order_by,
    bool include_table_id) {
  std::vector<Json::Value> sorted_rows;
  for (const auto& row : rows) {
    sorted_rows.push_back(row);
  }
  const auto key = ColumnOutputName(order_by.column, include_table_id);
  std::sort(
      sorted_rows.begin(),
      sorted_rows.end(),
      [&](const Json::Value& lhs, const Json::Value& rhs) {
        const auto left = lhs.isMember(key) ? JsonValueToComparableString(lhs[key]) : "";
        const auto right = rhs.isMember(key) ? JsonValueToComparableString(rhs[key]) : "";
        if (left == right) {
          return false;
        }
        return order_by.descending ? left > right : left < right;
      });
  rows = Json::Value(Json::arrayValue);
  for (const auto& row : sorted_rows) {
    rows.append(row);
  }
}

}  // namespace

QueryExecutor::QueryExecutor(table::TableRepository& table_repository) : table_repository_(table_repository) {}

EmbeddedQueryExecutionResult QueryExecutor::Execute(const StoredEmbeddedQuery& query) const {
  std::string error_message;
  const auto validation = ValidateSelectQuery(query.sql, config_, error_message);
  if (!validation.has_value()) {
    throw std::runtime_error(error_message);
  }
  if (validation->table_id != query.table_id) {
    throw std::runtime_error("query_table_mismatch");
  }

  TableDetailsMap tables;
  for (const auto& table_id : validation->table_ids) {
    const auto table = table_repository_.FindTableById(table_id);
    if (!table.has_value()) {
      throw std::runtime_error("table_not_found");
    }
    tables[table_id] = *table;
  }

  auto contexts = LoadInitialContexts(table_repository_, validation->table_id);
  for (const auto& join : validation->joins) {
    contexts = ApplyJoin(table_repository_, contexts, join);
  }
  if (validation->where.has_value()) {
    contexts = ApplyWhere(std::move(contexts), *validation->where);
  }
  if (validation->order_by.has_value()) {
    const auto order_by = *validation->order_by;
    std::sort(
        contexts.begin(),
        contexts.end(),
        [&](const RowContext& lhs, const RowContext& rhs) {
          const auto left = JsonValueToComparableString(GetColumnValue(lhs, order_by.column));
          const auto right = JsonValueToComparableString(GetColumnValue(rhs, order_by.column));
          if (left == right) {
            return false;
          }
          return order_by.descending ? left > right : left < right;
        });
  }

  EmbeddedQueryExecutionResult result{
      .query_id = query.query_id,
      .fragment_kind = query.fragment_kind,
      .table_id = query.table_id,
  };

  const bool include_table_id = validation->table_ids.size() > 1;
  if (!validation->aggregate_selections.empty()) {
    for (const auto& aggregate : validation->aggregate_selections) {
      if (!aggregate.count_star) {
        ValidateSelectedColumns({aggregate.column}, tables);
      }
    }
    ValidateSelectedColumns(validation->selected_columns, tables);
    for (const auto& column : validation->selected_columns) {
      result.columns.push_back(ColumnOutputName(column, include_table_id));
    }
    for (const auto& aggregate : validation->aggregate_selections) {
      result.columns.push_back(AggregateOutputName(aggregate, include_table_id));
    }

    if (validation->group_by_columns.empty()) {
      Json::Value row(Json::objectValue);
      for (const auto& aggregate : validation->aggregate_selections) {
        row[AggregateOutputName(aggregate, include_table_id)] = ComputeAggregate(contexts, aggregate);
      }
      result.rows.append(row);
    } else {
      std::map<std::string, std::vector<RowContext>> groups;
      for (const auto& context : contexts) {
        groups[BuildGroupKey(context, validation->group_by_columns)].push_back(context);
      }
      for (const auto& [_, group_contexts] : groups) {
        if (group_contexts.empty()) {
          continue;
        }
        Json::Value row(Json::objectValue);
        for (const auto& column : validation->selected_columns) {
          row[ColumnOutputName(column, include_table_id)] = GetColumnValue(group_contexts.front(), column);
        }
        for (const auto& aggregate : validation->aggregate_selections) {
          row[AggregateOutputName(aggregate, include_table_id)] = ComputeAggregate(group_contexts, aggregate);
        }
        result.rows.append(row);
      }
    }
    if (validation->order_by.has_value()) {
      SortResultRows(result.rows, *validation->order_by, include_table_id);
    }
    while (result.rows.size() > validation->limit) {
      result.rows.removeIndex(static_cast<Json::ArrayIndex>(result.rows.size() - 1), nullptr);
    }
    result.row_count = static_cast<std::uint64_t>(result.rows.size());
    return result;
  }

  if (validation->select_all) {
    result.columns = include_table_id ? AllQualifiedColumnNames(tables) : AllColumnNames(tables.at(validation->table_id));
  } else {
    ValidateSelectedColumns(validation->selected_columns, tables);
    for (const auto& column : validation->selected_columns) {
      result.columns.push_back(ColumnOutputName(column, include_table_id));
    }
  }

  const auto row_limit = std::min<std::size_t>(contexts.size(), static_cast<std::size_t>(validation->limit));
  for (std::size_t index = 0; index < row_limit; ++index) {
    Json::Value output_row(Json::objectValue);
    if (validation->select_all) {
      for (const auto& [table_id, table] : tables) {
        for (const auto& column : table.columns) {
          const auto key = include_table_id ? table_id + "." + column.column_name : column.column_name;
          output_row[key] = contexts[index].at(table_id).isMember(column.column_name)
              ? contexts[index].at(table_id)[column.column_name]
              : Json::Value();
        }
      }
    } else {
      for (const auto& column : validation->selected_columns) {
        output_row[ColumnOutputName(column, include_table_id)] = GetColumnValue(contexts[index], column);
      }
    }
    result.rows.append(output_row);
  }
  result.row_count = static_cast<std::uint64_t>(row_limit);
  return result;
}

}  // namespace picaresque::embedded_query
