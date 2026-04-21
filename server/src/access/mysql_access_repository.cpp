#include "mysql_access_repository.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

#include <json/json.h>
#include <mysql/mysql.h>

namespace picaresque::access {
namespace {

struct DbConfig {
  std::string host = "127.0.0.1";
  unsigned int port = 3306;
  std::string dbname = "picaresque";
  std::string user;
  std::string passwd;
};

class MySqlAccessRepository final : public AccessRepository {
 public:
  explicit MySqlAccessRepository(DbConfig config) : config_(std::move(config)) {}

  std::vector<IpRuleRecord> ListIpRules() const override {
    auto connection = OpenConnection();
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT id, value_text, address_family, rule_type, prefix_length, effect, surface, enabled, note "
        "FROM access_ip_rules WHERE enabled = 1 ORDER BY id");

    std::vector<IpRuleRecord> rules;
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result.get())) != nullptr) {
      rules.push_back({
          .id = ParseInt64(row[0]),
          .value_text = row[1] == nullptr ? "" : row[1],
          .address_family = ParseAddressFamily(row[2]),
          .rule_type = ParseRuleType(row[3]),
          .prefix_length = row[4] == nullptr ? std::nullopt : std::optional<int>(std::stoi(row[4])),
          .effect = ParseEffect(row[5]),
          .enabled = row[7] != nullptr && std::string(row[7]) == "1",
          .surface = ParseSurface(row[6]),
          .note = row[8] == nullptr ? std::nullopt : std::optional<std::string>(row[8]),
      });
    }
    return rules;
  }

 private:
  struct MysqlDeleter {
    void operator()(MYSQL* mysql) const {
      if (mysql != nullptr) {
        mysql_close(mysql);
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

  using ConnectionPtr = std::unique_ptr<MYSQL, MysqlDeleter>;
  using ResultPtr = std::unique_ptr<MYSQL_RES, MysqlResultDeleter>;

  ConnectionPtr OpenConnection() const {
    MYSQL* raw = mysql_init(nullptr);
    if (raw == nullptr) {
      throw std::runtime_error("mysql_init failed");
    }
    mysql_options(raw, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    if (mysql_real_connect(
            raw,
            config_.host.c_str(),
            config_.user.c_str(),
            config_.passwd.c_str(),
            config_.dbname.c_str(),
            config_.port,
            nullptr,
            0) == nullptr) {
      const std::string message = mysql_error(raw);
      mysql_close(raw);
      throw std::runtime_error("mysql_real_connect failed: " + message);
    }
    return ConnectionPtr(raw);
  }

  static ResultPtr ExecuteQuery(MYSQL* connection, const std::string& statement) {
    if (mysql_real_query(connection, statement.c_str(), statement.size()) != 0) {
      throw std::runtime_error(
          "mysql query failed: " + std::string(mysql_error(connection)) + " | sql=" + statement);
    }
    return ResultPtr(mysql_store_result(connection));
  }

  static std::int64_t ParseInt64(const char* value) {
    return value == nullptr ? 0 : std::stoll(value);
  }

  static AddressFamily ParseAddressFamily(const char* value) {
    return std::string(value == nullptr ? "" : value) == "ipv6" ? AddressFamily::IPv6 : AddressFamily::IPv4;
  }

  static IpRuleType ParseRuleType(const char* value) {
    return std::string(value == nullptr ? "" : value) == "cidr" ? IpRuleType::Cidr : IpRuleType::Single;
  }

  static RuleEffect ParseEffect(const char* value) {
    return std::string(value == nullptr ? "" : value) == "allow" ? RuleEffect::Allow : RuleEffect::Deny;
  }

  static std::optional<AccessSurface> ParseSurface(const char* value) {
    const std::string text = value == nullptr ? "" : value;
    if (text == "web") {
      return AccessSurface::Web;
    }
    if (text == "rest_api") {
      return AccessSurface::RestApi;
    }
    return std::nullopt;
  }

  DbConfig config_;
};

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

}  // namespace

AccessRepository& GetMySqlAccessRepository() {
  static MySqlAccessRepository repository(LoadDbConfig(ResolveConfigPath()));
  return repository;
}

}  // namespace picaresque::access
