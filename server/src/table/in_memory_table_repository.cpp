#include "picaresque/table/in_memory_table_repository.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace picaresque::table {

std::vector<TableSummary> InMemoryTableRepository::ListTables() const {
  std::scoped_lock lock(mutex_);
  std::vector<TableSummary> tables;
  tables.reserve(tables_.size());
  for (const auto& table : tables_) {
    tables.push_back(table.summary);
  }
  return tables;
}

std::optional<TableDetails> InMemoryTableRepository::FindTableById(const std::string& table_id) const {
  std::scoped_lock lock(mutex_);
  const auto it = std::find_if(
      tables_.begin(),
      tables_.end(),
      [&table_id](const TableDetails& table) {
        return table.summary.table_id == table_id;
      });
  if (it == tables_.end()) {
    return std::nullopt;
  }
  return *it;
}

TableDetails InMemoryTableRepository::CreateTable(const CreateTableCommand& command) {
  std::scoped_lock lock(mutex_);
  std::vector<ColumnDefinition> columns;
  columns.reserve(command.columns.size());
  for (const auto& column : command.columns) {
    auto stored_column = column;
    if (stored_column.column_id.empty()) {
      stored_column.column_id = BuildNextColumnId();
    }
    columns.push_back(stored_column);
  }

  TableDetails table{
      .summary =
          {
              .table_id = BuildNextTableId(),
              .table_name = command.table_name,
              .created_by_user_id = command.actor_user_id,
              .updated_by_user_id = command.actor_user_id,
              .required_permissions = command.required_permissions,
          },
      .columns = columns,
  };
  tables_.push_back(table);
  return table;
}

TableDetails InMemoryTableRepository::UpdateTable(const UpdateTableCommand& command) {
  std::scoped_lock lock(mutex_);
  auto it = std::find_if(
      tables_.begin(),
      tables_.end(),
      [&command](const TableDetails& table) {
        return table.summary.table_id == command.table_id;
      });
  if (it == tables_.end()) {
    throw std::runtime_error("table_not_found");
  }

  it->summary.table_name = command.table_name;
  it->summary.updated_by_user_id = command.actor_user_id;
  it->summary.required_permissions = command.required_permissions;
  return *it;
}

TableDetails InMemoryTableRepository::AddColumn(const AddColumnCommand& command) {
  std::scoped_lock lock(mutex_);
  auto it = std::find_if(
      tables_.begin(),
      tables_.end(),
      [&command](const TableDetails& table) {
        return table.summary.table_id == command.table_id;
      });
  if (it == tables_.end()) {
    throw std::runtime_error("table_not_found");
  }

  auto column = command.column;
  if (column.column_id.empty()) {
    column.column_id = BuildNextColumnId();
  }
  it->columns.push_back(column);
  it->summary.updated_by_user_id = command.actor_user_id;
  return *it;
}

void InMemoryTableRepository::DeleteTable(const std::string& table_id) {
  std::scoped_lock lock(mutex_);
  const auto before = tables_.size();
  tables_.erase(
      std::remove_if(
          tables_.begin(),
          tables_.end(),
          [&table_id](const TableDetails& table) {
            return table.summary.table_id == table_id;
          }),
      tables_.end());
  rows_.erase(
      std::remove_if(
          rows_.begin(),
          rows_.end(),
          [&table_id](const TableRow& row) {
            return row.table_id == table_id;
          }),
      rows_.end());
  if (tables_.size() == before) {
    throw std::runtime_error("table_not_found");
  }
}

std::vector<TableRow> InMemoryTableRepository::ListRows(const std::string& table_id) const {
  std::scoped_lock lock(mutex_);
  std::vector<TableRow> rows;
  for (const auto& row : rows_) {
    if (row.table_id == table_id) {
      rows.push_back(row);
    }
  }
  return rows;
}

std::optional<TableRow> InMemoryTableRepository::FindRowById(
    const std::string& table_id,
    const std::string& row_id) const {
  std::scoped_lock lock(mutex_);
  const auto it = std::find_if(
      rows_.begin(),
      rows_.end(),
      [&table_id, &row_id](const TableRow& row) {
        return row.table_id == table_id && row.row_id == row_id;
      });
  if (it == rows_.end()) {
    return std::nullopt;
  }
  return *it;
}

TableRow InMemoryTableRepository::CreateRow(const CreateRowCommand& command) {
  std::scoped_lock lock(mutex_);
  TableRow row{
      .row_id = BuildNextRowId(),
      .table_id = command.table_id,
      .created_by_user_id = command.actor_user_id,
      .updated_by_user_id = command.actor_user_id,
      .values = command.values,
  };
  rows_.push_back(row);
  return row;
}

TableRow InMemoryTableRepository::UpdateRow(const UpdateRowCommand& command) {
  std::scoped_lock lock(mutex_);
  auto it = std::find_if(
      rows_.begin(),
      rows_.end(),
      [&command](const TableRow& row) {
        return row.table_id == command.table_id && row.row_id == command.row_id;
      });
  if (it == rows_.end()) {
    throw std::runtime_error("row_not_found");
  }

  it->updated_by_user_id = command.actor_user_id;
  it->values = command.values;
  return *it;
}

void InMemoryTableRepository::DeleteRow(const std::string& table_id, const std::string& row_id) {
  std::scoped_lock lock(mutex_);
  const auto before = rows_.size();
  rows_.erase(
      std::remove_if(
          rows_.begin(),
          rows_.end(),
          [&table_id, &row_id](const TableRow& row) {
            return row.table_id == table_id && row.row_id == row_id;
          }),
      rows_.end());
  if (rows_.size() == before) {
    throw std::runtime_error("row_not_found");
  }
}

std::string InMemoryTableRepository::BuildNextTableId() {
  std::ostringstream stream;
  stream << "table_" << next_table_id_++;
  return stream.str();
}

std::string InMemoryTableRepository::BuildNextColumnId() {
  std::ostringstream stream;
  stream << "column_" << next_column_id_++;
  return stream.str();
}

std::string InMemoryTableRepository::BuildNextRowId() {
  std::ostringstream stream;
  stream << "row_" << next_row_id_++;
  return stream.str();
}

}  // namespace picaresque::table
