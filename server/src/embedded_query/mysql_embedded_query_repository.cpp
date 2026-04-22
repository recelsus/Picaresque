#include "mysql_embedded_query_repository.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

#include <json/json.h>
#include <mysql/mysql.h>

namespace picaresque::embedded_query {
namespace {

struct DbConfig {
  std::string host = "127.0.0.1";
  unsigned int port = 3306;
  std::string dbname = "picaresque";
  std::string user;
  std::string passwd;
};

class MySqlEmbeddedQueryRepository final : public EmbeddedQueryRepository {
 public:
  explicit MySqlEmbeddedQueryRepository(DbConfig config) : config_(std::move(config)) {}

  std::vector<StoredEmbeddedQuery> ListByArticleId(const std::string& article_id) const override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT query_id, article_id, fragment_kind, location_index, sql_body, table_id "
        "FROM embedded_queries WHERE article_id = '" +
            EscapeSqlString(connection.get(), article_id) + "' ORDER BY location_index");
    return ReadQueries(result.get());
  }

  std::optional<StoredEmbeddedQuery> FindById(
      const std::string& article_id,
      const std::string& query_id) const override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT query_id, article_id, fragment_kind, location_index, sql_body, table_id "
        "FROM embedded_queries WHERE article_id = '" +
            EscapeSqlString(connection.get(), article_id) + "' AND query_id = '" +
            EscapeSqlString(connection.get(), query_id) + "' LIMIT 1");
    auto queries = ReadQueries(result.get());
    if (queries.empty()) {
      return std::nullopt;
    }
    return queries.front();
  }

  void ReplaceArticleQueries(
      const std::string& article_id,
      const std::vector<StoredEmbeddedQuery>& queries) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    ExecuteStatement(connection.get(), "START TRANSACTION");
    try {
      ExecuteStatement(
          connection.get(),
          "DELETE FROM embedded_queries WHERE article_id = '" + EscapeSqlString(connection.get(), article_id) + "'");
      for (const auto& query : queries) {
        ExecuteStatement(
            connection.get(),
            "INSERT INTO embedded_queries "
            "(query_id, article_id, fragment_kind, location_index, sql_body, table_id) VALUES ('" +
                EscapeSqlString(connection.get(), query.query_id) + "', '" +
                EscapeSqlString(connection.get(), article_id) + "', '" +
                EscapeSqlString(connection.get(), FragmentKindToString(query.fragment_kind)) + "', " +
                std::to_string(query.location_index) + ", '" +
                EscapeSqlString(connection.get(), query.sql) + "', '" +
                EscapeSqlString(connection.get(), query.table_id) + "')");
      }
      ExecuteStatement(connection.get(), "COMMIT");
    } catch (...) {
      ExecuteStatement(connection.get(), "ROLLBACK");
      throw;
    }
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

  static const char* FragmentKindToString(FragmentKind kind) {
    return kind == FragmentKind::Inline ? "inline" : "block";
  }

  static FragmentKind FragmentKindFromString(const std::string& value) {
    return value == "block" ? FragmentKind::Block : FragmentKind::Inline;
  }

  static std::vector<StoredEmbeddedQuery> ReadQueries(MYSQL_RES* result) {
    std::vector<StoredEmbeddedQuery> queries;
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result)) != nullptr) {
      queries.push_back({
          .query_id = row[0] == nullptr ? "" : row[0],
          .article_id = row[1] == nullptr ? "" : row[1],
          .fragment_kind = FragmentKindFromString(row[2] == nullptr ? "" : row[2]),
          .location_index = row[3] == nullptr ? 0 : std::stoull(row[3]),
          .sql = row[4] == nullptr ? "" : row[4],
          .table_id = row[5] == nullptr ? "" : row[5],
      });
    }
    return queries;
  }

  DbConfig config_;
};

DbConfig LoadDbConfig() {
  std::filesystem::path path = "config/config.local.json";
  if (!std::filesystem::exists(path)) {
    path = "server/config/config.local.json";
  }
  if (!std::filesystem::exists(path)) {
    path = "config/config.example.json";
  }
  if (!std::filesystem::exists(path)) {
    path = "server/config/config.example.json";
  }

  std::ifstream file(path);
  Json::Value root;
  file >> root;

  DbConfig config;
  const auto& client = root["db_clients"][0];
  config.host = client.get("host", config.host).asString();
  config.port = client.get("port", config.port).asUInt();
  config.dbname = client.get("dbname", config.dbname).asString();
  config.user = client.get("user", "").asString();
  config.passwd = client.get("passwd", "").asString();
  return config;
}

}  // namespace

EmbeddedQueryRepository& GetMySqlEmbeddedQueryRepository() {
  static MySqlEmbeddedQueryRepository repository(LoadDbConfig());
  return repository;
}

}  // namespace picaresque::embedded_query
