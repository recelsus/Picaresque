#pragma once

#include "picaresque/permission/types.hpp"
#include "picaresque/table/table_repository.hpp"
#include "picaresque/user/user_group_repository.hpp"

namespace picaresque::table {

class TableService {
 public:
  TableService(TableRepository& table_repository, user::UserGroupRepository& user_repository);

  std::vector<TableSummary> ListTables(const std::string& actor_user_id) const;
  TableDetails GetTable(const std::string& actor_user_id, const std::string& table_id) const;
  TableDetails CreateTable(const CreateTableCommand& command) const;
  TableDetails UpdateTable(const UpdateTableCommand& command) const;
  TableDetails AddColumn(const AddColumnCommand& command) const;
  void DeleteTable(const DeleteTableCommand& command) const;

  std::vector<TableRow> ListRows(const std::string& actor_user_id, const std::string& table_id) const;
  TableRow GetRow(const std::string& actor_user_id, const std::string& table_id, const std::string& row_id) const;
  TableRow CreateRow(const CreateRowCommand& command) const;
  TableRow UpdateRow(const UpdateRowCommand& command) const;
  void DeleteRow(const DeleteRowCommand& command) const;

 private:
  permission::User BuildPermissionUser(const std::string& actor_user_id) const;
  bool CanRead(const permission::User& actor, const TableSummary& table) const;
  bool CanWrite(const permission::User& actor, const TableSummary& table) const;
  void ValidateTableInput(
      const std::string& table_name,
      const std::vector<ColumnDefinition>& columns,
      const std::vector<permission::AccessRequirement>& required_permissions) const;
  void ValidateColumn(const ColumnDefinition& column) const;
  Json::Value NormalizeRowValues(const TableDetails& table, const Json::Value& values) const;

  TableRepository& table_repository_;
  user::UserGroupRepository& user_repository_;
};

const char* ColumnTypeToString(ColumnType type);
ColumnType ColumnTypeFromString(const std::string& value);

}  // namespace picaresque::table
