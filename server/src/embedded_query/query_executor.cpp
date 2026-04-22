#include "picaresque/embedded_query/query_executor.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace picaresque::embedded_query {
namespace {

using RowContext = std::map<std::string, Json::Value>;
using TableDetailsMap = std::map<std::string, table::TableDetails>;

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

std::vector<RowContext> ApplyWhere(
    std::vector<RowContext> contexts,
    const QueryValidationResult::WhereCondition& where) {
  const auto literal = LiteralToJson(where.literal);
  contexts.erase(
      std::remove_if(
          contexts.begin(),
          contexts.end(),
          [&](const RowContext& context) {
            return !CompareValues(GetColumnValue(context, where.left), where.op, literal);
          }),
      contexts.end());
  return contexts;
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

  if (validation->count_all) {
    result.columns = {"count"};
    Json::Value row(Json::objectValue);
    row["count"] = static_cast<Json::UInt64>(contexts.size());
    result.rows.append(row);
    result.row_count = 1;
    return result;
  }

  const bool include_table_id = validation->table_ids.size() > 1;
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
