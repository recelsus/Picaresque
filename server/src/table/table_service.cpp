#include "picaresque/table/table_service.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "picaresque/permission/api.hpp"

namespace picaresque::table {
namespace {

std::vector<permission::AccessRequirement> EffectiveRequiredPermissions(
    const std::vector<permission::AccessRequirement>& required_permissions) {
  if (!required_permissions.empty()) {
    return required_permissions;
  }
  return {
      {
          .group_id = "*",
          .read = permission::kDefaultPermission,
          .write = permission::kDefaultPermission,
      },
  };
}

permission::TableResource BuildTableResource(const TableSummary& table) {
  return {
      .table_id = table.table_id,
      .required_permissions = EffectiveRequiredPermissions(table.required_permissions),
  };
}

bool IsDigits(const std::string& value, std::size_t start, std::size_t length) {
  if (start + length > value.size()) {
    return false;
  }
  for (std::size_t index = start; index < start + length; ++index) {
    if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
      return false;
    }
  }
  return true;
}

int ParseNumber(const std::string& value, std::size_t start, std::size_t length) {
  return std::stoi(value.substr(start, length));
}

bool IsLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int DaysInMonth(int year, int month) {
  switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      return 31;
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
    case 2:
      return IsLeapYear(year) ? 29 : 28;
    default:
      return 0;
  }
}

bool IsValidDate(const std::string& value) {
  if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
    return false;
  }
  if (!IsDigits(value, 0, 4) || !IsDigits(value, 5, 2) || !IsDigits(value, 8, 2)) {
    return false;
  }
  const int year = ParseNumber(value, 0, 4);
  const int month = ParseNumber(value, 5, 2);
  const int day = ParseNumber(value, 8, 2);
  return month >= 1 && month <= 12 && day >= 1 && day <= DaysInMonth(year, month);
}

bool IsValidTime(const std::string& value) {
  if ((value.size() != 5 && value.size() != 8) || value[2] != ':') {
    return false;
  }
  if (!IsDigits(value, 0, 2) || !IsDigits(value, 3, 2)) {
    return false;
  }
  if (value.size() == 8 && (value[5] != ':' || !IsDigits(value, 6, 2))) {
    return false;
  }
  const int hour = ParseNumber(value, 0, 2);
  const int minute = ParseNumber(value, 3, 2);
  const int second = value.size() == 8 ? ParseNumber(value, 6, 2) : 0;
  return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59;
}

bool IsValidDateTime(const std::string& value) {
  if (value.size() != 19 || (value[10] != 'T' && value[10] != ' ')) {
    return false;
  }
  return IsValidDate(value.substr(0, 10)) && IsValidTime(value.substr(11, 8));
}

bool IsValidDecimalString(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  std::size_t index = value[0] == '-' ? 1 : 0;
  if (index == value.size()) {
    return false;
  }
  bool saw_digit = false;
  bool saw_dot = false;
  bool saw_fraction_digit = false;
  for (; index < value.size(); ++index) {
    const char current = value[index];
    if (std::isdigit(static_cast<unsigned char>(current))) {
      saw_digit = true;
      if (saw_dot) {
        saw_fraction_digit = true;
      }
      continue;
    }
    if (current == '.' && !saw_dot) {
      saw_dot = true;
      continue;
    }
    return false;
  }
  return saw_digit && (!saw_dot || saw_fraction_digit);
}

std::string DecimalToString(const Json::Value& value) {
  if (value.isString()) {
    const auto text = value.asString();
    if (!IsValidDecimalString(text)) {
      throw std::runtime_error("invalid_column_value");
    }
    return text;
  }
  if (value.isInt64() || value.isInt()) {
    return std::to_string(value.asLargestInt());
  }
  if (value.isUInt64() || value.isUInt()) {
    return std::to_string(value.asLargestUInt());
  }
  if (value.isDouble()) {
    std::ostringstream stream;
    stream << std::setprecision(15) << value.asDouble();
    auto text = stream.str();
    if (text.find('.') != std::string::npos) {
      while (!text.empty() && text.back() == '0') {
        text.pop_back();
      }
      if (!text.empty() && text.back() == '.') {
        text.pop_back();
      }
    }
    if (!IsValidDecimalString(text)) {
      throw std::runtime_error("invalid_column_value");
    }
    return text;
  }
  throw std::runtime_error("invalid_column_value");
}

}  // namespace

TableService::TableService(TableRepository& table_repository, user::UserGroupRepository& user_repository)
    : table_repository_(table_repository), user_repository_(user_repository) {}

std::vector<TableSummary> TableService::ListTables(const std::string& actor_user_id) const {
  const auto actor = BuildPermissionUser(actor_user_id);
  std::vector<TableSummary> readable_tables;
  for (const auto& table : table_repository_.ListTables()) {
    if (CanRead(actor, table)) {
      readable_tables.push_back(table);
    }
  }
  return readable_tables;
}

TableDetails TableService::GetTable(const std::string& actor_user_id, const std::string& table_id) const {
  const auto table = table_repository_.FindTableById(table_id);
  if (!table.has_value()) {
    throw std::runtime_error("table_not_found");
  }
  if (!CanRead(BuildPermissionUser(actor_user_id), table->summary)) {
    throw std::runtime_error("forbidden");
  }
  return *table;
}

TableDetails TableService::CreateTable(const CreateTableCommand& command) const {
  ValidateTableInput(command.table_name, command.columns, command.required_permissions);
  const TableSummary proposed_table{
      .table_id = "",
      .table_name = command.table_name,
      .created_by_user_id = command.actor_user_id,
      .updated_by_user_id = command.actor_user_id,
      .required_permissions = command.required_permissions,
  };
  if (!CanWrite(BuildPermissionUser(command.actor_user_id), proposed_table)) {
    throw std::runtime_error("forbidden");
  }
  return table_repository_.CreateTable(command);
}

TableDetails TableService::UpdateTable(const UpdateTableCommand& command) const {
  ValidateTableInput(command.table_name, {}, command.required_permissions);
  const auto current = table_repository_.FindTableById(command.table_id);
  if (!current.has_value()) {
    throw std::runtime_error("table_not_found");
  }
  const auto actor = BuildPermissionUser(command.actor_user_id);
  if (!CanWrite(actor, current->summary)) {
    throw std::runtime_error("forbidden");
  }
  const TableSummary proposed_table{
      .table_id = command.table_id,
      .table_name = command.table_name,
      .created_by_user_id = current->summary.created_by_user_id,
      .updated_by_user_id = command.actor_user_id,
      .required_permissions = command.required_permissions,
  };
  if (!CanWrite(actor, proposed_table)) {
    throw std::runtime_error("forbidden");
  }
  return table_repository_.UpdateTable(command);
}

TableDetails TableService::AddColumn(const AddColumnCommand& command) const {
  ValidateColumn(command.column);
  const auto current = table_repository_.FindTableById(command.table_id);
  if (!current.has_value()) {
    throw std::runtime_error("table_not_found");
  }
  if (!CanWrite(BuildPermissionUser(command.actor_user_id), current->summary)) {
    throw std::runtime_error("forbidden");
  }
  return table_repository_.AddColumn(command);
}

void TableService::DeleteTable(const DeleteTableCommand& command) const {
  const auto table = table_repository_.FindTableById(command.table_id);
  if (!table.has_value()) {
    throw std::runtime_error("table_not_found");
  }
  if (!CanWrite(BuildPermissionUser(command.actor_user_id), table->summary)) {
    throw std::runtime_error("forbidden");
  }
  table_repository_.DeleteTable(command.table_id);
}

std::vector<TableRow> TableService::ListRows(const std::string& actor_user_id, const std::string& table_id) const {
  const auto table = GetTable(actor_user_id, table_id);
  static_cast<void>(table);
  return table_repository_.ListRows(table_id);
}

TableRow TableService::GetRow(
    const std::string& actor_user_id,
    const std::string& table_id,
    const std::string& row_id) const {
  const auto table = GetTable(actor_user_id, table_id);
  static_cast<void>(table);
  const auto row = table_repository_.FindRowById(table_id, row_id);
  if (!row.has_value()) {
    throw std::runtime_error("row_not_found");
  }
  return *row;
}

TableRow TableService::CreateRow(const CreateRowCommand& command) const {
  const auto table = table_repository_.FindTableById(command.table_id);
  if (!table.has_value()) {
    throw std::runtime_error("table_not_found");
  }
  if (!CanWrite(BuildPermissionUser(command.actor_user_id), table->summary)) {
    throw std::runtime_error("forbidden");
  }
  auto normalized_command = command;
  normalized_command.values = NormalizeRowValues(*table, command.values);
  return table_repository_.CreateRow(normalized_command);
}

TableRow TableService::UpdateRow(const UpdateRowCommand& command) const {
  const auto table = table_repository_.FindTableById(command.table_id);
  if (!table.has_value()) {
    throw std::runtime_error("table_not_found");
  }
  if (!CanWrite(BuildPermissionUser(command.actor_user_id), table->summary)) {
    throw std::runtime_error("forbidden");
  }
  if (!table_repository_.FindRowById(command.table_id, command.row_id).has_value()) {
    throw std::runtime_error("row_not_found");
  }
  auto normalized_command = command;
  normalized_command.values = NormalizeRowValues(*table, command.values);
  return table_repository_.UpdateRow(normalized_command);
}

void TableService::DeleteRow(const DeleteRowCommand& command) const {
  const auto table = table_repository_.FindTableById(command.table_id);
  if (!table.has_value()) {
    throw std::runtime_error("table_not_found");
  }
  if (!CanWrite(BuildPermissionUser(command.actor_user_id), table->summary)) {
    throw std::runtime_error("forbidden");
  }
  if (!table_repository_.FindRowById(command.table_id, command.row_id).has_value()) {
    throw std::runtime_error("row_not_found");
  }
  table_repository_.DeleteRow(command.table_id, command.row_id);
}

permission::User TableService::BuildPermissionUser(const std::string& actor_user_id) const {
  const auto actor = user_repository_.FindUserDetailsById(actor_user_id);
  if (!actor.has_value()) {
    throw std::runtime_error("user_not_found");
  }
  return {
      .user_id = actor->summary.user_id,
      .user_name = actor->summary.user_name,
      .role = actor->summary.role,
      .owned_groups = actor->owned_groups,
      .scoped_permissions = actor->scoped_permissions,
  };
}

bool TableService::CanRead(const permission::User& actor, const TableSummary& table) const {
  return permission::CanReadTable(actor, BuildTableResource(table));
}

bool TableService::CanWrite(const permission::User& actor, const TableSummary& table) const {
  return permission::CanWriteTable(actor, BuildTableResource(table));
}

void TableService::ValidateTableInput(
    const std::string& table_name,
    const std::vector<ColumnDefinition>& columns,
    const std::vector<permission::AccessRequirement>& required_permissions) const {
  if (table_name.empty()) {
    throw std::runtime_error("table_name_required");
  }
  std::unordered_set<std::string> column_names;
  for (const auto& column : columns) {
    ValidateColumn(column);
    if (!column_names.insert(column.column_name).second) {
      throw std::runtime_error("duplicate_column_name");
    }
  }
  for (const auto& requirement : required_permissions) {
    permission::ValidateAccessRequirement(requirement);
  }
}

void TableService::ValidateColumn(const ColumnDefinition& column) const {
  if (column.column_name.empty()) {
    throw std::runtime_error("column_name_required");
  }
}

Json::Value TableService::NormalizeRowValues(const TableDetails& table, const Json::Value& values) const {
  if (!values.isObject()) {
    throw std::runtime_error("row_values_must_be_object");
  }

  Json::Value normalized_values = values;
  std::unordered_set<std::string> allowed_columns;
  for (const auto& column : table.columns) {
    allowed_columns.insert(column.column_name);
    if (column.is_required && (!values.isMember(column.column_name) || values[column.column_name].isNull())) {
      throw std::runtime_error("required_column_missing");
    }
    if (!values.isMember(column.column_name) || values[column.column_name].isNull()) {
      continue;
    }
    const auto& value = values[column.column_name];
    switch (column.column_type) {
      case ColumnType::Varchar:
      case ColumnType::LongText:
        if (!value.isString()) {
          throw std::runtime_error("invalid_column_value");
        }
        break;
      case ColumnType::Integer:
        if (!value.isInt64() && !value.isUInt64() && !value.isInt() && !value.isUInt()) {
          throw std::runtime_error("invalid_column_value");
        }
        break;
      case ColumnType::Decimal:
        normalized_values[column.column_name] = DecimalToString(value);
        break;
      case ColumnType::Boolean:
        if (!value.isBool()) {
          throw std::runtime_error("invalid_column_value");
        }
        break;
      case ColumnType::Date:
        if (!value.isString() || !IsValidDate(value.asString())) {
          throw std::runtime_error("invalid_column_value");
        }
        break;
      case ColumnType::Time:
        if (!value.isString() || !IsValidTime(value.asString())) {
          throw std::runtime_error("invalid_column_value");
        }
        break;
      case ColumnType::DateTime:
        if (!value.isString() || !IsValidDateTime(value.asString())) {
          throw std::runtime_error("invalid_column_value");
        }
        break;
      case ColumnType::Json:
        break;
    }
  }

  const auto names = values.getMemberNames();
  for (const auto& name : names) {
    if (!allowed_columns.contains(name)) {
      throw std::runtime_error("unknown_column");
    }
  }
  return normalized_values;
}

const char* ColumnTypeToString(ColumnType type) {
  switch (type) {
    case ColumnType::Varchar:
      return "varchar";
    case ColumnType::LongText:
      return "long_text";
    case ColumnType::Integer:
      return "integer";
    case ColumnType::Decimal:
      return "decimal";
    case ColumnType::Boolean:
      return "boolean";
    case ColumnType::Date:
      return "date";
    case ColumnType::Time:
      return "time";
    case ColumnType::DateTime:
      return "datetime";
    case ColumnType::Json:
      return "json";
  }
  return "varchar";
}

ColumnType ColumnTypeFromString(const std::string& value) {
  if (value == "varchar") {
    return ColumnType::Varchar;
  }
  if (value == "long_text") {
    return ColumnType::LongText;
  }
  if (value == "integer") {
    return ColumnType::Integer;
  }
  if (value == "decimal") {
    return ColumnType::Decimal;
  }
  if (value == "boolean") {
    return ColumnType::Boolean;
  }
  if (value == "date") {
    return ColumnType::Date;
  }
  if (value == "time") {
    return ColumnType::Time;
  }
  if (value == "datetime") {
    return ColumnType::DateTime;
  }
  if (value == "json") {
    return ColumnType::Json;
  }
  throw std::runtime_error("invalid_column_type");
}

}  // namespace picaresque::table
