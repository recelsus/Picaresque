#include "migration_runner.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

#include <json/json.h>
#include <mysql/mysql.h>

namespace picaresque::migration {
namespace {

constexpr const char* kMigrationTableName = "schema_migrations";

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream.is_open()) {
    throw std::runtime_error("failed to open file: " + path.string());
  }

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::string EscapeSqlString(MYSQL* connection, const std::string& value) {
  std::string escaped;
  escaped.resize(value.size() * 2 + 1);
  const auto escaped_size = mysql_real_escape_string(
      connection,
      escaped.data(),
      value.c_str(),
      value.size());
  escaped.resize(escaped_size);
  return escaped;
}

MYSQL_RES* ExecuteQuery(MYSQL* connection, const std::string& statement) {
  if (mysql_real_query(connection, statement.c_str(), statement.size()) != 0) {
    throw std::runtime_error(
        "mysql query failed: " + std::string(mysql_error(connection)) + " | sql=" + statement);
  }

  return mysql_store_result(connection);
}

bool DatabaseExists(MYSQL* connection, const std::string& dbname) {
  const auto escaped_dbname = EscapeSqlString(connection, dbname);
  MYSQL_RES* result = ExecuteQuery(
      connection,
      "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = '" + escaped_dbname +
          "' LIMIT 1");
  if (result == nullptr) {
    return false;
  }

  const bool exists = mysql_num_rows(result) > 0;
  mysql_free_result(result);
  return exists;
}

void ExecuteStatement(MYSQL* connection, const std::string& statement) {
  MYSQL_RES* result = ExecuteQuery(connection, statement);
  if (result != nullptr) {
    mysql_free_result(result);
    return;
  }

  if (mysql_field_count(connection) != 0) {
    throw std::runtime_error("mysql result fetch failed: " + std::string(mysql_error(connection)));
  }
}

void EnsureMigrationTable(MYSQL* connection, const std::string& dbname) {
  ExecuteStatement(connection, "USE `" + dbname + "`");
  ExecuteStatement(
      connection,
      "CREATE TABLE IF NOT EXISTS " + std::string(kMigrationTableName) + " ("
      "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, "
      "filename VARCHAR(255) NOT NULL, "
      "applied_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3), "
      "PRIMARY KEY (id), "
      "UNIQUE KEY uk_schema_migrations_filename (filename))");
}

std::set<std::string> LoadAppliedMigrations(MYSQL* connection, const std::string& dbname) {
  std::set<std::string> filenames;

  if (!DatabaseExists(connection, dbname)) {
    return filenames;
  }

  EnsureMigrationTable(connection, dbname);
  ExecuteStatement(connection, "USE `" + dbname + "`");
  MYSQL_RES* result = ExecuteQuery(
      connection,
      "SELECT filename FROM " + std::string(kMigrationTableName) + " ORDER BY filename");
  if (result == nullptr) {
    return filenames;
  }

  MYSQL_ROW row = nullptr;
  while ((row = mysql_fetch_row(result)) != nullptr) {
    if (row[0] != nullptr) {
      filenames.insert(row[0]);
    }
  }

  mysql_free_result(result);
  return filenames;
}

void RecordAppliedMigration(MYSQL* connection, const std::string& dbname, const std::string& filename) {
  EnsureMigrationTable(connection, dbname);
  ExecuteStatement(connection, "USE `" + dbname + "`");
  ExecuteStatement(
      connection,
      "INSERT INTO " + std::string(kMigrationTableName) + " (filename) VALUES ('" +
          EscapeSqlString(connection, filename) + "')");
}

bool IsSqlMigrationFile(const std::filesystem::path& path) {
  return path.extension() == ".sql";
}

}  // namespace

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

  const auto& clients = root["db_clients"];
  if (!clients.isArray() || clients.empty()) {
    throw std::runtime_error("db_clients is empty");
  }

  const auto& client = clients[0];
  return {
      .host = client.get("host", "127.0.0.1").asString(),
      .port = client.get("port", 3306).asUInt(),
      .dbname = client.get("dbname", "picaresque").asString(),
      .user = client.get("user", "").asString(),
      .passwd = client.get("passwd", "").asString(),
  };
}

std::vector<std::filesystem::path> DiscoverMigrationFiles(const std::filesystem::path& sql_dir) {
  std::vector<std::filesystem::path> files;

  for (const auto& entry : std::filesystem::directory_iterator(sql_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (IsSqlMigrationFile(entry.path())) {
      files.push_back(entry.path());
    }
  }

  std::sort(files.begin(), files.end());
  return files;
}

std::vector<std::string> SplitSqlStatements(const std::string& sql) {
  std::vector<std::string> statements;
  std::string current;
  bool in_single_quote = false;
  bool in_double_quote = false;
  bool in_line_comment = false;
  bool in_block_comment = false;

  for (std::size_t i = 0; i < sql.size(); ++i) {
    const char ch = sql[i];
    const char next = i + 1 < sql.size() ? sql[i + 1] : '\0';

    if (in_line_comment) {
      if (ch == '\n') {
        in_line_comment = false;
      }
      continue;
    }

    if (in_block_comment) {
      if (ch == '*' && next == '/') {
        in_block_comment = false;
        ++i;
      }
      continue;
    }

    if (!in_single_quote && !in_double_quote && ch == '-' && next == '-') {
      in_line_comment = true;
      ++i;
      continue;
    }

    if (!in_single_quote && !in_double_quote && ch == '/' && next == '*') {
      in_block_comment = true;
      ++i;
      continue;
    }

    if (ch == '\'' && !in_double_quote) {
      in_single_quote = !in_single_quote;
      current.push_back(ch);
      continue;
    }

    if (ch == '"' && !in_single_quote) {
      in_double_quote = !in_double_quote;
      current.push_back(ch);
      continue;
    }

    if (ch == ';' && !in_single_quote && !in_double_quote) {
      const auto first = current.find_first_not_of(" \t\r\n");
      if (first != std::string::npos) {
        const auto last = current.find_last_not_of(" \t\r\n");
        statements.push_back(current.substr(first, last - first + 1));
      }
      current.clear();
      continue;
    }

    current.push_back(ch);
  }

  const auto first = current.find_first_not_of(" \t\r\n");
  if (first != std::string::npos) {
    const auto last = current.find_last_not_of(" \t\r\n");
    statements.push_back(current.substr(first, last - first + 1));
  }

  return statements;
}

int RunMigrations(const std::filesystem::path& config_path, const std::filesystem::path& sql_dir) {
  const auto config = LoadDbConfig(config_path);
  MYSQL* connection = mysql_init(nullptr);
  if (connection == nullptr) {
    throw std::runtime_error("mysql_init failed");
  }

  mysql_options(connection, MYSQL_SET_CHARSET_NAME, "utf8mb4");

  if (mysql_real_connect(
          connection,
          config.host.c_str(),
          config.user.c_str(),
          config.passwd.c_str(),
          nullptr,
          config.port,
          nullptr,
          0) == nullptr) {
    const std::string message = mysql_error(connection);
    mysql_close(connection);
    throw std::runtime_error("mysql_real_connect failed: " + message);
  }

  try {
    auto applied = LoadAppliedMigrations(connection, config.dbname);
    const auto migration_files = DiscoverMigrationFiles(sql_dir);

    for (const auto& migration_file : migration_files) {
      const auto filename = migration_file.filename().string();
      if (applied.contains(filename)) {
        continue;
      }

      const auto sql = ReadTextFile(migration_file);
      for (const auto& statement : SplitSqlStatements(sql)) {
        ExecuteStatement(connection, statement);
      }

      if (DatabaseExists(connection, config.dbname)) {
        RecordAppliedMigration(connection, config.dbname, filename);
        applied.insert(filename);
      }
    }

    mysql_close(connection);
    return 0;
  } catch (...) {
    mysql_close(connection);
    throw;
  }
}

}  // namespace picaresque::migration
