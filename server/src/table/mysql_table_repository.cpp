#include "mysql_table_repository.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>

#include <json/json.h>
#include <mysql/mysql.h>

#include "picaresque/table/table_service.hpp"

namespace picaresque::table {
namespace {

struct DbConfig {
  std::string host = "127.0.0.1";
  unsigned int port = 3306;
  std::string dbname = "picaresque";
  std::string user;
  std::string passwd;
};

class MySqlTableRepository final : public TableRepository {
 public:
  explicit MySqlTableRepository(DbConfig config) : config_(std::move(config)) {}

  std::vector<TableSummary> ListTables() const override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    auto result = ExecuteQuery(connection.get(), "SELECT table_id FROM custom_tables ORDER BY id DESC");

    std::vector<TableSummary> tables;
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result.get())) != nullptr) {
      if (row[0] != nullptr) {
        tables.push_back(LoadTableDetails(connection.get(), row[0]).summary);
      }
    }
    return tables;
  }

  std::optional<TableDetails> FindTableById(const std::string& table_id) const override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    if (!TableExists(connection.get(), table_id)) {
      return std::nullopt;
    }
    return LoadTableDetails(connection.get(), table_id);
  }

  TableDetails CreateTable(const CreateTableCommand& command) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    BeginTransaction(connection.get());
    try {
      const auto table_id = GenerateTableId();
      ExecuteStatement(
          connection.get(),
          "INSERT INTO custom_tables "
          "(table_id, table_name, created_by_user_id, updated_by_user_id) VALUES ('" +
              EscapeSqlString(connection.get(), table_id) + "', '" +
              EscapeSqlString(connection.get(), command.table_name) + "', '" +
              EscapeSqlString(connection.get(), command.actor_user_id) + "', '" +
              EscapeSqlString(connection.get(), command.actor_user_id) + "')");
      ReplaceRequiredPermissions(connection.get(), table_id, command.required_permissions);
      ReplaceColumns(connection.get(), table_id, command.columns);
      CommitTransaction(connection.get());
      return LoadTableDetails(connection.get(), table_id);
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
  }

  TableDetails UpdateTable(const UpdateTableCommand& command) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    BeginTransaction(connection.get());
    try {
      ExecuteStatement(
          connection.get(),
          "UPDATE custom_tables SET table_name = '" + EscapeSqlString(connection.get(), command.table_name) +
              "', updated_by_user_id = '" + EscapeSqlString(connection.get(), command.actor_user_id) +
              "' WHERE table_id = '" + EscapeSqlString(connection.get(), command.table_id) + "'");
      ReplaceRequiredPermissions(connection.get(), command.table_id, command.required_permissions);
      CommitTransaction(connection.get());
      return LoadTableDetails(connection.get(), command.table_id);
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
  }

  TableDetails AddColumn(const AddColumnCommand& command) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    BeginTransaction(connection.get());
    try {
      InsertColumn(connection.get(), command.table_id, command.column);
      ExecuteStatement(
          connection.get(),
          "UPDATE custom_tables SET updated_by_user_id = '" +
              EscapeSqlString(connection.get(), command.actor_user_id) + "' WHERE table_id = '" +
              EscapeSqlString(connection.get(), command.table_id) + "'");
      CommitTransaction(connection.get());
      return LoadTableDetails(connection.get(), command.table_id);
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
  }

  void DeleteTable(const std::string& table_id) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    BeginTransaction(connection.get());
    try {
      ExecuteStatement(
          connection.get(),
          "DELETE FROM custom_table_rows WHERE table_id = '" + EscapeSqlString(connection.get(), table_id) + "'");
      ExecuteStatement(
          connection.get(),
          "DELETE FROM custom_table_columns WHERE table_id = '" + EscapeSqlString(connection.get(), table_id) + "'");
      ExecuteStatement(
          connection.get(),
          "DELETE FROM custom_table_required_permissions WHERE table_id = '" +
              EscapeSqlString(connection.get(), table_id) + "'");
      ExecuteStatement(
          connection.get(),
          "DELETE FROM custom_tables WHERE table_id = '" + EscapeSqlString(connection.get(), table_id) + "'");
      CommitTransaction(connection.get());
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
  }

  std::vector<TableRow> ListRows(const std::string& table_id) const override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT row_id FROM custom_table_rows WHERE table_id = '" + EscapeSqlString(connection.get(), table_id) +
            "' ORDER BY id DESC");
    std::vector<TableRow> rows;
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result.get())) != nullptr) {
      if (row[0] != nullptr) {
        rows.push_back(LoadRow(connection.get(), table_id, row[0]));
      }
    }
    return rows;
  }

  std::optional<TableRow> FindRowById(const std::string& table_id, const std::string& row_id) const override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    if (!RowExists(connection.get(), table_id, row_id)) {
      return std::nullopt;
    }
    return LoadRow(connection.get(), table_id, row_id);
  }

  TableRow CreateRow(const CreateRowCommand& command) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    const auto row_id = GenerateUuidLikeString();
    ExecuteStatement(
        connection.get(),
        "INSERT INTO custom_table_rows "
        "(row_id, table_id, created_by_user_id, updated_by_user_id, values_json) VALUES ('" +
            EscapeSqlString(connection.get(), row_id) + "', '" +
            EscapeSqlString(connection.get(), command.table_id) + "', '" +
            EscapeSqlString(connection.get(), command.actor_user_id) + "', '" +
            EscapeSqlString(connection.get(), command.actor_user_id) + "', '" +
            EscapeSqlString(connection.get(), SerializeJson(command.values)) + "')");
    return LoadRow(connection.get(), command.table_id, row_id);
  }

  TableRow UpdateRow(const UpdateRowCommand& command) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    ExecuteStatement(
        connection.get(),
        "UPDATE custom_table_rows SET updated_by_user_id = '" +
            EscapeSqlString(connection.get(), command.actor_user_id) + "', values_json = '" +
            EscapeSqlString(connection.get(), SerializeJson(command.values)) + "' WHERE table_id = '" +
            EscapeSqlString(connection.get(), command.table_id) + "' AND row_id = '" +
            EscapeSqlString(connection.get(), command.row_id) + "'");
    return LoadRow(connection.get(), command.table_id, command.row_id);
  }

  void DeleteRow(const std::string& table_id, const std::string& row_id) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    ExecuteStatement(
        connection.get(),
        "DELETE FROM custom_table_rows WHERE table_id = '" + EscapeSqlString(connection.get(), table_id) +
            "' AND row_id = '" + EscapeSqlString(connection.get(), row_id) + "'");
  }

 private:
  struct MysqlDeleter {
    void operator()(MYSQL* connection) const {
      if (connection != nullptr) {
        mysql_close(connection);
      }
    }
  };

  struct MysqlResultDeleter {
    void operator()(MYSQL_RES* result) const {
      if (result != nullptr) {
        mysql_free_result(result);
      }
    }
  };

  using MysqlConnectionPtr = std::unique_ptr<MYSQL, MysqlDeleter>;
  using MysqlResultPtr = std::unique_ptr<MYSQL_RES, MysqlResultDeleter>;

  MysqlConnectionPtr OpenConnection() const {
    MYSQL* raw_connection = mysql_init(nullptr);
    if (raw_connection == nullptr) {
      throw std::runtime_error("mysql_init failed");
    }
    mysql_options(raw_connection, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    if (mysql_real_connect(
            raw_connection,
            config_.host.c_str(),
            config_.user.c_str(),
            config_.passwd.c_str(),
            config_.dbname.c_str(),
            config_.port,
            nullptr,
            0) == nullptr) {
      const std::string message = mysql_error(raw_connection);
      mysql_close(raw_connection);
      throw std::runtime_error("mysql_real_connect failed: " + message);
    }
    return MysqlConnectionPtr(raw_connection);
  }

  static MysqlResultPtr ExecuteQuery(MYSQL* connection, const std::string& statement) {
    if (mysql_real_query(connection, statement.c_str(), statement.size()) != 0) {
      throw std::runtime_error(
          "mysql query failed: " + std::string(mysql_error(connection)) + " | sql=" + statement);
    }
    return MysqlResultPtr(mysql_store_result(connection));
  }

  static void ExecuteStatement(MYSQL* connection, const std::string& statement) {
    auto result = ExecuteQuery(connection, statement);
    if (result != nullptr) {
      return;
    }
    if (mysql_field_count(connection) != 0) {
      throw std::runtime_error("mysql result fetch failed: " + std::string(mysql_error(connection)));
    }
  }

  static std::string EscapeSqlString(MYSQL* connection, const std::string& value) {
    std::string escaped;
    escaped.resize(value.size() * 2 + 1);
    const auto escaped_size = mysql_real_escape_string(connection, escaped.data(), value.c_str(), value.size());
    escaped.resize(escaped_size);
    return escaped;
  }

  static void BeginTransaction(MYSQL* connection) {
    ExecuteStatement(connection, "START TRANSACTION");
  }

  static void CommitTransaction(MYSQL* connection) {
    ExecuteStatement(connection, "COMMIT");
  }

  static void RollbackTransaction(MYSQL* connection) {
    ExecuteStatement(connection, "ROLLBACK");
  }

  static std::uint8_t ParseUint8(const char* value) {
    if (value == nullptr) {
      return 0;
    }
    return static_cast<std::uint8_t>(std::stoul(value));
  }

  static std::string GenerateUuidLikeString() {
    static std::mt19937_64 generator(std::random_device{}());
    static constexpr char kHex[] = "0123456789abcdef";
    std::string value(36, '0');
    for (std::size_t i = 0; i < value.size(); ++i) {
      if (i == 8 || i == 13 || i == 18 || i == 23) {
        value[i] = '-';
      } else {
        value[i] = kHex[generator() % 16];
      }
    }
    return value;
  }

  static std::string GenerateTableId() {
    static std::mt19937_64 generator(std::random_device{}());
    static constexpr char kHex[] = "0123456789abcdef";
    std::string value = "table_";
    for (int i = 0; i < 16; ++i) {
      value.push_back(kHex[generator() % 16]);
    }
    return value;
  }

  static std::string SerializeJson(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
  }

  static Json::Value ParseJson(const std::string& value) {
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    std::istringstream stream(value);
    if (!Json::parseFromStream(builder, stream, &root, &errors)) {
      throw std::runtime_error("invalid_row_json");
    }
    return root;
  }

  bool TableExists(MYSQL* connection, const std::string& table_id) const {
    auto result = ExecuteQuery(
        connection,
        "SELECT COUNT(*) FROM custom_tables WHERE table_id = '" + EscapeSqlString(connection, table_id) + "'");
    MYSQL_ROW row = mysql_fetch_row(result.get());
    return row != nullptr && row[0] != nullptr && std::stoull(row[0]) > 0;
  }

  bool RowExists(MYSQL* connection, const std::string& table_id, const std::string& row_id) const {
    auto result = ExecuteQuery(
        connection,
        "SELECT COUNT(*) FROM custom_table_rows WHERE table_id = '" + EscapeSqlString(connection, table_id) +
            "' AND row_id = '" + EscapeSqlString(connection, row_id) + "'");
    MYSQL_ROW row = mysql_fetch_row(result.get());
    return row != nullptr && row[0] != nullptr && std::stoull(row[0]) > 0;
  }

  TableDetails LoadTableDetails(MYSQL* connection, const std::string& table_id) const {
    auto result = ExecuteQuery(
        connection,
        "SELECT table_id, table_name, created_by_user_id, updated_by_user_id FROM custom_tables "
        "WHERE table_id = '" +
            EscapeSqlString(connection, table_id) + "' LIMIT 1");
    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row == nullptr) {
      throw std::runtime_error("table_not_found");
    }

    TableDetails table{
        .summary =
            {
                .table_id = row[0] == nullptr ? "" : row[0],
                .table_name = row[1] == nullptr ? "" : row[1],
                .created_by_user_id = row[2] == nullptr ? "" : row[2],
                .updated_by_user_id = row[3] == nullptr ? "" : row[3],
            },
    };

    auto permission_result = ExecuteQuery(
        connection,
        "SELECT group_id, read_level, write_level FROM custom_table_required_permissions "
        "WHERE table_id = '" +
            EscapeSqlString(connection, table_id) + "' ORDER BY id");
    while ((row = mysql_fetch_row(permission_result.get())) != nullptr) {
      table.summary.required_permissions.push_back({
          .group_id = row[0] == nullptr ? "" : row[0],
          .read = ParseUint8(row[1]),
          .write = ParseUint8(row[2]),
      });
    }

    auto column_result = ExecuteQuery(
        connection,
        "SELECT column_id, column_name, column_type, is_required FROM custom_table_columns "
        "WHERE table_id = '" +
            EscapeSqlString(connection, table_id) + "' ORDER BY position_index, id");
    while ((row = mysql_fetch_row(column_result.get())) != nullptr) {
      table.columns.push_back({
          .column_id = row[0] == nullptr ? "" : row[0],
          .column_name = row[1] == nullptr ? "" : row[1],
          .column_type = ColumnTypeFromString(row[2] == nullptr ? "" : row[2]),
          .is_required = row[3] != nullptr && std::string(row[3]) == "1",
      });
    }

    return table;
  }

  TableRow LoadRow(MYSQL* connection, const std::string& table_id, const std::string& row_id) const {
    auto result = ExecuteQuery(
        connection,
        "SELECT row_id, table_id, created_by_user_id, updated_by_user_id, values_json FROM custom_table_rows "
        "WHERE table_id = '" +
            EscapeSqlString(connection, table_id) + "' AND row_id = '" + EscapeSqlString(connection, row_id) +
            "' LIMIT 1");
    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row == nullptr) {
      throw std::runtime_error("row_not_found");
    }
    return {
        .row_id = row[0] == nullptr ? "" : row[0],
        .table_id = row[1] == nullptr ? "" : row[1],
        .created_by_user_id = row[2] == nullptr ? "" : row[2],
        .updated_by_user_id = row[3] == nullptr ? "" : row[3],
        .values = ParseJson(row[4] == nullptr ? "{}" : row[4]),
    };
  }

  void ReplaceRequiredPermissions(
      MYSQL* connection,
      const std::string& table_id,
      const std::vector<permission::AccessRequirement>& required_permissions) const {
    ExecuteStatement(
        connection,
        "DELETE FROM custom_table_required_permissions WHERE table_id = '" +
            EscapeSqlString(connection, table_id) + "'");
    for (const auto& permission : required_permissions) {
      ExecuteStatement(
          connection,
          "INSERT INTO custom_table_required_permissions "
          "(table_id, group_id, read_level, write_level) VALUES ('" +
              EscapeSqlString(connection, table_id) + "', '" +
              EscapeSqlString(connection, permission.group_id) + "', " +
              std::to_string(permission.read) + ", " + std::to_string(permission.write) + ")");
    }
  }

  void ReplaceColumns(
      MYSQL* connection,
      const std::string& table_id,
      const std::vector<ColumnDefinition>& columns) const {
    ExecuteStatement(
        connection,
        "DELETE FROM custom_table_columns WHERE table_id = '" + EscapeSqlString(connection, table_id) + "'");
    for (const auto& column : columns) {
      InsertColumn(connection, table_id, column);
    }
  }

  void InsertColumn(MYSQL* connection, const std::string& table_id, const ColumnDefinition& column) const {
    const auto column_id = column.column_id.empty() ? GenerateUuidLikeString() : column.column_id;
    auto position_result = ExecuteQuery(
        connection,
        "SELECT COALESCE(MAX(position_index), 0) + 1 FROM custom_table_columns WHERE table_id = '" +
            EscapeSqlString(connection, table_id) + "'");
    MYSQL_ROW row = mysql_fetch_row(position_result.get());
    const auto position = row == nullptr || row[0] == nullptr ? "1" : std::string(row[0]);
    ExecuteStatement(
        connection,
        "INSERT INTO custom_table_columns "
        "(column_id, table_id, column_name, column_type, is_required, position_index) VALUES ('" +
            EscapeSqlString(connection, column_id) + "', '" + EscapeSqlString(connection, table_id) + "', '" +
            EscapeSqlString(connection, column.column_name) + "', '" +
            EscapeSqlString(connection, ColumnTypeToString(column.column_type)) + "', " +
            std::string(column.is_required ? "1" : "0") + ", " + position + ")");
  }

  DbConfig config_;
};

std::filesystem::path ResolveConfigPath() {
  if (std::filesystem::exists("config/config.local.json")) {
    return "config/config.local.json";
  }
  if (std::filesystem::exists("server/config/config.local.json")) {
    return "server/config/config.local.json";
  }
  if (std::filesystem::exists("config/config.example.json")) {
    return "config/config.example.json";
  }
  return "server/config/config.example.json";
}

DbConfig LoadDbConfig(const std::filesystem::path& config_path) {
  Json::CharReaderBuilder builder;
  Json::Value root;
  std::string errors;

  std::ifstream stream(config_path);
  if (!stream.is_open()) {
    throw std::runtime_error("failed to open config: " + config_path.string());
  }
  if (!Json::parseFromStream(builder, stream, &root, &errors)) {
    throw std::runtime_error("failed to parse config: " + errors);
  }

  const auto& client = root["db_clients"][0];
  return {
      .host = client.get("host", "127.0.0.1").asString(),
      .port = client.get("port", 3306).asUInt(),
      .dbname = client.get("dbname", "picaresque").asString(),
      .user = client.get("user", "").asString(),
      .passwd = client.get("passwd", "").asString(),
  };
}

}  // namespace

TableRepository& GetMySqlTableRepository() {
  static MySqlTableRepository repository(LoadDbConfig(ResolveConfigPath()));
  return repository;
}

}  // namespace picaresque::table
