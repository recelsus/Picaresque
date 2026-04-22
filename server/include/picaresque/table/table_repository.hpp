#pragma once

#include <optional>
#include <vector>

#include "picaresque/table/table_types.hpp"

namespace picaresque::table {

class TableRepository {
 public:
  virtual ~TableRepository() = default;

  virtual std::vector<TableSummary> ListTables() const = 0;
  virtual std::optional<TableDetails> FindTableById(const std::string& table_id) const = 0;
  virtual TableDetails CreateTable(const CreateTableCommand& command) = 0;
  virtual TableDetails UpdateTable(const UpdateTableCommand& command) = 0;
  virtual TableDetails AddColumn(const AddColumnCommand& command) = 0;
  virtual void DeleteTable(const std::string& table_id) = 0;

  virtual std::vector<TableRow> ListRows(const std::string& table_id) const = 0;
  virtual std::optional<TableRow> FindRowById(const std::string& table_id, const std::string& row_id) const = 0;
  virtual TableRow CreateRow(const CreateRowCommand& command) = 0;
  virtual TableRow UpdateRow(const UpdateRowCommand& command) = 0;
  virtual void DeleteRow(const std::string& table_id, const std::string& row_id) = 0;
};

}  // namespace picaresque::table
