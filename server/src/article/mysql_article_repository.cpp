#include "mysql_article_repository.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>

#include <json/json.h>
#include <mysql/mysql.h>

namespace picaresque::article {
namespace {

struct DbConfig {
  std::string host = "127.0.0.1";
  unsigned int port = 3306;
  std::string dbname = "picaresque";
  std::string user;
  std::string passwd;
};

class MySqlArticleRepository final : public ArticleRepository {
 public:
  explicit MySqlArticleRepository(DbConfig config) : config_(std::move(config)) {}

  std::vector<ArticleSummary> ListArticles() const override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT article_id FROM articles ORDER BY id DESC");

    std::vector<ArticleSummary> articles;
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result.get())) != nullptr) {
      if (row[0] != nullptr) {
        articles.push_back(LoadArticleDetails(connection.get(), row[0]).summary);
      }
    }
    return articles;
  }

  std::optional<ArticleDetails> FindArticleById(const std::string& article_id) const override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    if (!ArticleExists(connection.get(), article_id)) {
      return std::nullopt;
    }
    return LoadArticleDetails(connection.get(), article_id);
  }

  ArticleDetails CreateArticle(const CreateArticleCommand& command) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    BeginTransaction(connection.get());
    try {
      const auto article_id = GenerateUuidLikeString();
      ExecuteStatement(
          connection.get(),
          "INSERT INTO articles "
          "(article_id, title, body, created_by_user_id, updated_by_user_id, is_locked, locked_by_user_id, locked_at) "
          "VALUES ('" +
              EscapeSqlString(connection.get(), article_id) + "', '" +
              EscapeSqlString(connection.get(), command.title) + "', '" +
              EscapeSqlString(connection.get(), command.body) + "', '" +
              EscapeSqlString(connection.get(), command.actor_user_id) + "', '" +
              EscapeSqlString(connection.get(), command.actor_user_id) + "', " +
              std::string(command.is_locked ? "1" : "0") + ", " +
              NullableSqlString(connection.get(), command.locked_by_user_id) + ", " +
              std::string(command.is_locked ? "CURRENT_TIMESTAMP(3)" : "NULL") + ")");
      ReplaceRequiredPermissions(connection.get(), article_id, command.required_permissions);
      CommitTransaction(connection.get());
      return LoadArticleDetails(connection.get(), article_id);
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
  }

  ArticleDetails UpdateArticle(const UpdateArticleCommand& command) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    BeginTransaction(connection.get());
    try {
      ExecuteStatement(
          connection.get(),
          "UPDATE articles SET title = '" + EscapeSqlString(connection.get(), command.title) +
              "', body = '" + EscapeSqlString(connection.get(), command.body) +
              "', updated_by_user_id = '" + EscapeSqlString(connection.get(), command.actor_user_id) +
              "', is_locked = " + std::string(command.is_locked ? "1" : "0") +
              ", locked_by_user_id = " + NullableSqlString(connection.get(), command.locked_by_user_id) +
              ", locked_at = " + std::string(command.is_locked ? "COALESCE(locked_at, CURRENT_TIMESTAMP(3))" : "NULL") +
              " WHERE article_id = '" + EscapeSqlString(connection.get(), command.article_id) + "'");
      ReplaceRequiredPermissions(connection.get(), command.article_id, command.required_permissions);
      CommitTransaction(connection.get());
      return LoadArticleDetails(connection.get(), command.article_id);
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
  }

  void DeleteArticle(const std::string& article_id) override {
    auto connection = OpenConnection();
    ExecuteStatement(connection.get(), "USE `" + config_.dbname + "`");
    BeginTransaction(connection.get());
    try {
      ExecuteStatement(
          connection.get(),
          "DELETE FROM article_required_permissions WHERE article_id = '" +
              EscapeSqlString(connection.get(), article_id) + "'");
      ExecuteStatement(
          connection.get(),
          "DELETE FROM articles WHERE article_id = '" + EscapeSqlString(connection.get(), article_id) + "'");
      CommitTransaction(connection.get());
    } catch (...) {
      RollbackTransaction(connection.get());
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
    const auto escaped_size = mysql_real_escape_string(
        connection,
        escaped.data(),
        value.c_str(),
        value.size());
    escaped.resize(escaped_size);
    return escaped;
  }

  static std::string NullableSqlString(MYSQL* connection, const std::optional<std::string>& value) {
    if (!value.has_value()) {
      return "NULL";
    }
    return "'" + EscapeSqlString(connection, *value) + "'";
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

  bool ArticleExists(MYSQL* connection, const std::string& article_id) const {
    auto result = ExecuteQuery(
        connection,
        "SELECT COUNT(*) FROM articles WHERE article_id = '" + EscapeSqlString(connection, article_id) + "'");
    MYSQL_ROW row = mysql_fetch_row(result.get());
    return row != nullptr && row[0] != nullptr && std::stoull(row[0]) > 0;
  }

  ArticleDetails LoadArticleDetails(MYSQL* connection, const std::string& article_id) const {
    auto result = ExecuteQuery(
        connection,
        "SELECT article_id, title, body, created_by_user_id, updated_by_user_id, is_locked, "
        "locked_by_user_id, DATE_FORMAT(locked_at, '%Y-%m-%dT%H:%i:%s.%fZ') "
        "FROM articles WHERE article_id = '" +
            EscapeSqlString(connection, article_id) + "' LIMIT 1");
    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row == nullptr) {
      throw std::runtime_error("article_not_found");
    }

    ArticleDetails article{
        .summary =
            {
                .article_id = row[0] == nullptr ? "" : row[0],
                .title = row[1] == nullptr ? "" : row[1],
                .created_by_user_id = row[3] == nullptr ? "" : row[3],
                .updated_by_user_id = row[4] == nullptr ? "" : row[4],
                .is_locked = row[5] != nullptr && std::string(row[5]) == "1",
                .locked_by_user_id = row[6] == nullptr ? std::nullopt : std::optional<std::string>(row[6]),
                .locked_at = row[7] == nullptr ? std::nullopt : std::optional<std::string>(row[7]),
            },
        .body = row[2] == nullptr ? "" : row[2],
    };

    auto permission_result = ExecuteQuery(
        connection,
        "SELECT group_id, read_level, write_level FROM article_required_permissions "
        "WHERE article_id = '" +
            EscapeSqlString(connection, article_id) + "' ORDER BY id");
    while ((row = mysql_fetch_row(permission_result.get())) != nullptr) {
      article.summary.required_permissions.push_back({
          .group_id = row[0] == nullptr ? "" : row[0],
          .read = ParseUint8(row[1]),
          .write = ParseUint8(row[2]),
      });
    }
    return article;
  }

  void ReplaceRequiredPermissions(
      MYSQL* connection,
      const std::string& article_id,
      const std::vector<permission::AccessRequirement>& required_permissions) const {
    ExecuteStatement(
        connection,
        "DELETE FROM article_required_permissions WHERE article_id = '" +
            EscapeSqlString(connection, article_id) + "'");
    for (const auto& permission : required_permissions) {
      ExecuteStatement(
          connection,
          "INSERT INTO article_required_permissions "
          "(article_id, group_id, read_level, write_level) VALUES ('" +
              EscapeSqlString(connection, article_id) + "', '" +
              EscapeSqlString(connection, permission.group_id) + "', " +
              std::to_string(permission.read) + ", " + std::to_string(permission.write) + ")");
    }
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

ArticleRepository& GetMySqlArticleRepository() {
  static MySqlArticleRepository repository(LoadDbConfig(ResolveConfigPath()));
  return repository;
}

}  // namespace picaresque::article
