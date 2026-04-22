#pragma once

#include <optional>
#include <string>
#include <vector>

#include <json/json.h>

#include "picaresque/permission/types.hpp"

namespace picaresque::table {

enum class ColumnType {
  Varchar,
  LongText,
  Integer,
  Decimal,
  Boolean,
  Date,
  Time,
  DateTime,
  Json,
};

struct ColumnDefinition {
  std::string column_id;
  std::string column_name;
  ColumnType column_type = ColumnType::Varchar;
  bool is_required = false;
};

struct TableSummary {
  std::string table_id;
  std::string table_name;
  std::string created_by_user_id;
  std::string updated_by_user_id;
  std::vector<permission::AccessRequirement> required_permissions;
};

struct TableDetails {
  TableSummary summary;
  std::vector<ColumnDefinition> columns;
};

struct TableRow {
  std::string row_id;
  std::string table_id;
  std::string created_by_user_id;
  std::string updated_by_user_id;
  Json::Value values = Json::objectValue;
};

struct CreateTableCommand {
  std::string actor_user_id;
  std::string table_name;
  std::vector<ColumnDefinition> columns;
  std::vector<permission::AccessRequirement> required_permissions;
};

struct UpdateTableCommand {
  std::string actor_user_id;
  std::string table_id;
  std::string table_name;
  std::vector<permission::AccessRequirement> required_permissions;
};

struct AddColumnCommand {
  std::string actor_user_id;
  std::string table_id;
  ColumnDefinition column;
};

struct DeleteTableCommand {
  std::string actor_user_id;
  std::string table_id;
};

struct CreateRowCommand {
  std::string actor_user_id;
  std::string table_id;
  Json::Value values = Json::objectValue;
};

struct UpdateRowCommand {
  std::string actor_user_id;
  std::string table_id;
  std::string row_id;
  Json::Value values = Json::objectValue;
};

struct DeleteRowCommand {
  std::string actor_user_id;
  std::string table_id;
  std::string row_id;
};

}  // namespace picaresque::table
