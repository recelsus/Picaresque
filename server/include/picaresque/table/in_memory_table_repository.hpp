#pragma once

#include <mutex>

#include "picaresque/table/table_repository.hpp"

namespace picaresque::table {

class InMemoryTableRepository final : public TableRepository {
 public:
  std::vector<TableSummary> ListTables() const override;
  std::optional<TableDetails> FindTableById(const std::string& table_id) const override;
  TableDetails CreateTable(const CreateTableCommand& command) override;
  TableDetails UpdateTable(const UpdateTableCommand& command) override;
  TableDetails AddColumn(const AddColumnCommand& command) override;
  void DeleteTable(const std::string& table_id) override;

  std::vector<TableRow> ListRows(const std::string& table_id) const override;
  std::optional<TableRow> FindRowById(const std::string& table_id, const std::string& row_id) const override;
  TableRow CreateRow(const CreateRowCommand& command) override;
  TableRow UpdateRow(const UpdateRowCommand& command) override;
  void DeleteRow(const std::string& table_id, const std::string& row_id) override;

 private:
  std::string BuildNextTableId();
  std::string BuildNextColumnId();
  std::string BuildNextRowId();

  mutable std::mutex mutex_;
  std::vector<TableDetails> tables_;
  std::vector<TableRow> rows_;
  std::size_t next_table_id_ = 1;
  std::size_t next_column_id_ = 1;
  std::size_t next_row_id_ = 1;
};

}  // namespace picaresque::table
