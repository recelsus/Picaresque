#include "mysql_user_group_repository.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

#include <json/json.h>
#include <mysql/mysql.h>

namespace picaresque::user {
namespace {

struct DbConfig {
  std::string host = "127.0.0.1";
  unsigned int port = 3306;
  std::string dbname = "picaresque";
  std::string user;
  std::string passwd;
};

class MySqlUserGroupRepository final : public UserGroupRepository {
 public:
  explicit MySqlUserGroupRepository(DbConfig config) : config_(std::move(config)) {}

  std::size_t CountUsers() const override {
    auto connection = OpenConnection();
    auto result = ExecuteQuery(connection.get(), "SELECT COUNT(*) FROM users");
    return ReadCountResult(result.get());
  }

  std::vector<UserSummary> ListUsers(const UserListFilter& filter) const override {
    auto connection = OpenConnection();
    std::string query =
        "SELECT user_id, login_id, user_name, email, role, is_active FROM users WHERE 1=1";

    if (filter.role.has_value()) {
      query += " AND role = '" + EscapeSqlString(connection.get(), RoleToString(*filter.role)) + "'";
    }

    if (filter.is_active.has_value()) {
      query += std::string(" AND is_active = ") + (*filter.is_active ? "1" : "0");
    }

    query += " ORDER BY id";

    auto result = ExecuteQuery(connection.get(), query);
    std::vector<UserSummary> users;
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result.get())) != nullptr) {
      users.push_back({
          .user_id = row[0] == nullptr ? "" : row[0],
          .login_id = row[1] == nullptr ? "" : row[1],
          .user_name = row[2] == nullptr ? "" : row[2],
          .email = row[3] == nullptr ? "" : row[3],
          .role = ParseRole(row[4]),
          .is_active = row[5] != nullptr && std::string(row[5]) == "1",
      });
    }
    return users;
  }

  std::optional<UserDetails> FindUserDetailsById(const std::string& user_id) const override {
    auto connection = OpenConnection();
    auto summary_result = ExecuteQuery(
        connection.get(),
        "SELECT user_id, login_id, user_name, email, role, is_active "
        "FROM users WHERE user_id = '" +
            EscapeSqlString(connection.get(), user_id) + "' LIMIT 1");

    MYSQL_ROW row = mysql_fetch_row(summary_result.get());
    if (row == nullptr) {
      return std::nullopt;
    }

    UserDetails details{
        .summary =
            {
                .user_id = row[0] == nullptr ? "" : row[0],
                .login_id = row[1] == nullptr ? "" : row[1],
                .user_name = row[2] == nullptr ? "" : row[2],
                .email = row[3] == nullptr ? "" : row[3],
                .role = ParseRole(row[4]),
                .is_active = row[5] != nullptr && std::string(row[5]) == "1",
            },
    };

    auto owner_result = ExecuteQuery(
        connection.get(),
        "SELECT group_id FROM group_owners WHERE user_id = '" +
            EscapeSqlString(connection.get(), user_id) + "' ORDER BY group_id");
    while ((row = mysql_fetch_row(owner_result.get())) != nullptr) {
      if (row[0] != nullptr) {
        details.owned_groups.emplace_back(row[0]);
      }
    }

    auto scoped_result = ExecuteQuery(
        connection.get(),
        "SELECT group_id, read_level, write_level FROM user_scoped_permissions WHERE user_id = '" +
            EscapeSqlString(connection.get(), user_id) + "' ORDER BY group_id");
    while ((row = mysql_fetch_row(scoped_result.get())) != nullptr) {
      details.scoped_permissions.push_back({
          .group_id = row[0] == nullptr ? "" : row[0],
          .read = ParseUint8(row[1]),
          .write = ParseUint8(row[2]),
      });
    }

    return details;
  }

  std::optional<UserDetails> FindUserDetailsByApiKeyHash(const std::string& key_hash) const override {
    auto connection = OpenConnection();
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT user_id FROM user_api_keys WHERE key_hash = '" +
            EscapeSqlString(connection.get(), key_hash) + "' AND enabled = 1 LIMIT 1");
    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row == nullptr || row[0] == nullptr) {
      return std::nullopt;
    }
    return FindUserDetailsById(row[0]);
  }

  bool UserExistsByLoginIdOrEmail(const std::string& login_id, const std::string& email) const override {
    auto connection = OpenConnection();
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT COUNT(*) FROM users WHERE login_id = '" + EscapeSqlString(connection.get(), login_id) +
            "' OR email = '" + EscapeSqlString(connection.get(), email) + "'");
    return ReadCountResult(result.get()) > 0;
  }

  std::optional<auth::ApiKeyInfo> FindApiKeyInfoByUserId(const std::string& user_id) const override {
    auto connection = OpenConnection();
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT user_id, key_prefix, enabled FROM user_api_keys WHERE user_id = '" +
            EscapeSqlString(connection.get(), user_id) + "' LIMIT 1");
    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row == nullptr) {
      return std::nullopt;
    }

    return auth::ApiKeyInfo{
        .user_id = row[0] == nullptr ? "" : row[0],
        .key_prefix = row[1] == nullptr ? "" : row[1],
        .enabled = row[2] != nullptr && std::string(row[2]) == "1",
    };
  }

  UserDetails CreateUser(
      const CreateUserCommand& command,
      const std::vector<permission::ScopedPermission>& initial_scoped_permissions) override {
    auto connection = OpenConnection();
    BeginTransaction(connection.get());

    try {
      const auto user_id = GenerateUuidLikeString();
      ExecuteStatement(
          connection.get(),
          "INSERT INTO users "
          "(user_id, login_id, user_name, email, password_hash, role, is_active) VALUES ('" +
              EscapeSqlString(connection.get(), user_id) + "', '" +
              EscapeSqlString(connection.get(), command.login_id) + "', '" +
              EscapeSqlString(connection.get(), command.user_name) + "', '" +
              EscapeSqlString(connection.get(), command.email) + "', '" +
              EscapeSqlString(connection.get(), command.password) + "', '" +
              EscapeSqlString(connection.get(), RoleToString(command.role)) + "', 1)");

      for (const auto& scoped_permission : initial_scoped_permissions) {
        ExecuteStatement(
            connection.get(),
            "INSERT INTO user_scoped_permissions "
            "(user_id, group_id, read_level, write_level) VALUES ('" +
                EscapeSqlString(connection.get(), user_id) + "', '" +
                EscapeSqlString(connection.get(), scoped_permission.group_id) + "', " +
                std::to_string(scoped_permission.read) + ", " + std::to_string(scoped_permission.write) +
                ")");
      }

      CommitTransaction(connection.get());
      return *FindUserDetailsById(user_id);
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
  }

  auth::ApiKeyInfo UpsertApiKey(
      const std::string& user_id,
      const std::string& key_prefix,
      const std::string& key_hash) override {
    auto connection = OpenConnection();
    ExecuteStatement(
        connection.get(),
        "INSERT INTO user_api_keys (user_id, key_prefix, key_hash, enabled) VALUES ('" +
            EscapeSqlString(connection.get(), user_id) + "', '" +
            EscapeSqlString(connection.get(), key_prefix) + "', '" +
            EscapeSqlString(connection.get(), key_hash) +
            "', 1) ON DUPLICATE KEY UPDATE key_prefix = VALUES(key_prefix), key_hash = VALUES(key_hash), enabled = 1");
    return *FindApiKeyInfoByUserId(user_id);
  }

  void DeleteApiKey(const std::string& user_id) override {
    auto connection = OpenConnection();
    ExecuteStatement(
        connection.get(),
        "DELETE FROM user_api_keys WHERE user_id = '" +
            EscapeSqlString(connection.get(), user_id) + "'");
  }

  std::optional<group::GroupSummary> FindGroupSummaryById(const std::string& group_id) const override {
    auto connection = OpenConnection();
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT group_id, group_name, description, created_by_user_id "
        "FROM user_groups WHERE group_id = '" +
            EscapeSqlString(connection.get(), group_id) + "' LIMIT 1");

    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row == nullptr) {
      return std::nullopt;
    }

    return group::GroupSummary{
        .group_id = row[0] == nullptr ? "" : row[0],
        .group_name = row[1] == nullptr ? "" : row[1],
        .description = row[2] == nullptr ? std::nullopt : std::optional<std::string>(row[2]),
        .created_by_user_id = row[3] == nullptr ? "" : row[3],
    };
  }

  bool GroupExistsByName(const std::string& group_name) const override {
    auto connection = OpenConnection();
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT COUNT(*) FROM user_groups WHERE group_name = '" +
            EscapeSqlString(connection.get(), group_name) + "'");
    return ReadCountResult(result.get()) > 0;
  }

  bool HasActiveMembership(const std::string& group_id, const std::string& user_id) const override {
    auto connection = OpenConnection();
    auto membership_result = ExecuteQuery(
        connection.get(),
        "SELECT COUNT(*) FROM group_memberships WHERE group_id = '" +
            EscapeSqlString(connection.get(), group_id) + "' AND user_id = '" +
            EscapeSqlString(connection.get(), user_id) + "' AND membership_status = 'active'");
    if (ReadCountResult(membership_result.get()) > 0) {
      return true;
    }

    auto owner_result = ExecuteQuery(
        connection.get(),
        "SELECT COUNT(*) FROM group_owners WHERE group_id = '" +
            EscapeSqlString(connection.get(), group_id) + "' AND user_id = '" +
            EscapeSqlString(connection.get(), user_id) + "'");
    return ReadCountResult(owner_result.get()) > 0;
  }

  std::optional<group::GroupInvitation> FindInvitationById(
      const std::string& invitation_id) const override {
    auto connection = OpenConnection();
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT invitation_id, group_id, invited_user_id, invited_by_user_id, invitation_status "
        "FROM group_invitations WHERE invitation_id = '" +
            EscapeSqlString(connection.get(), invitation_id) + "' LIMIT 1");

    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row == nullptr) {
      return std::nullopt;
    }

    return group::GroupInvitation{
        .invitation_id = row[0] == nullptr ? "" : row[0],
        .group_id = row[1] == nullptr ? "" : row[1],
        .invited_user_id = row[2] == nullptr ? "" : row[2],
        .invited_by_user_id = row[3] == nullptr ? "" : row[3],
        .status = ParseInvitationStatus(row[4]),
    };
  }

  bool HasPendingInvitation(const std::string& group_id, const std::string& invited_user_id) const override {
    auto connection = OpenConnection();
    auto result = ExecuteQuery(
        connection.get(),
        "SELECT COUNT(*) FROM group_invitations WHERE group_id = '" +
            EscapeSqlString(connection.get(), group_id) + "' AND invited_user_id = '" +
            EscapeSqlString(connection.get(), invited_user_id) +
            "' AND invitation_status = 'pending'");
    return ReadCountResult(result.get()) > 0;
  }

  group::GroupDetails CreateGroup(
      const group::CreateGroupCommand& command,
      const UserDetails& actor) override {
    auto connection = OpenConnection();
    BeginTransaction(connection.get());

    try {
      const auto group_id = GenerateUuidLikeString();
      ExecuteStatement(
          connection.get(),
          "INSERT INTO user_groups (group_id, group_name, description, created_by_user_id) VALUES ('" +
              EscapeSqlString(connection.get(), group_id) + "', '" +
              EscapeSqlString(connection.get(), command.group_name) + "', " +
              SqlStringOrNull(connection.get(), command.description) + ", '" +
              EscapeSqlString(connection.get(), actor.summary.user_id) + "')");

      ExecuteStatement(
          connection.get(),
          "INSERT INTO group_memberships "
          "(group_id, user_id, membership_status, joined_at) VALUES ('" +
              EscapeSqlString(connection.get(), group_id) + "', '" +
              EscapeSqlString(connection.get(), actor.summary.user_id) + "', 'active', "
              "CURRENT_TIMESTAMP(3))");

      if (actor.summary.role == permission::Role::Owner) {
        ExecuteStatement(
            connection.get(),
            "INSERT INTO group_owners (group_id, user_id) VALUES ('" +
                EscapeSqlString(connection.get(), group_id) + "', '" +
                EscapeSqlString(connection.get(), actor.summary.user_id) + "')");
      } else {
        ExecuteStatement(
            connection.get(),
            "INSERT INTO user_scoped_permissions "
            "(user_id, group_id, read_level, write_level) VALUES ('" +
                EscapeSqlString(connection.get(), actor.summary.user_id) + "', '" +
                EscapeSqlString(connection.get(), group_id) + "', 10, 10)");
      }

      CommitTransaction(connection.get());
      return LoadGroupDetails(connection.get(), group_id);
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
  }

  group::GroupInvitation CreateInvitation(const group::InviteUserCommand& command) override {
    auto connection = OpenConnection();
    BeginTransaction(connection.get());

    try {
      const auto invitation_id = GenerateUuidLikeString();
      ExecuteStatement(
          connection.get(),
          "INSERT INTO group_memberships "
          "(group_id, user_id, membership_status, joined_at) VALUES ('" +
              EscapeSqlString(connection.get(), command.group_id) + "', '" +
              EscapeSqlString(connection.get(), command.invited_user_id) +
              "', 'invited', NULL) "
              "ON DUPLICATE KEY UPDATE membership_status = 'invited', joined_at = NULL");

      ExecuteStatement(
          connection.get(),
          "INSERT INTO group_invitations "
          "(invitation_id, group_id, invited_user_id, invited_by_user_id, invitation_status) VALUES ('" +
              EscapeSqlString(connection.get(), invitation_id) + "', '" +
              EscapeSqlString(connection.get(), command.group_id) + "', '" +
              EscapeSqlString(connection.get(), command.invited_user_id) + "', '" +
              EscapeSqlString(connection.get(), command.actor_user_id) + "', 'pending')");

      CommitTransaction(connection.get());
      return *FindInvitationById(invitation_id);
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
  }

  group::GroupInvitation AcceptInvitation(const group::AcceptInvitationCommand& command) override {
    auto connection = OpenConnection();
    BeginTransaction(connection.get());

    try {
      const auto invitation = FindInvitationWithConnection(connection.get(), command.invitation_id);
      if (!invitation.has_value()) {
        throw std::runtime_error("invitation_not_found");
      }

      ExecuteStatement(
          connection.get(),
          "UPDATE group_invitations SET invitation_status = 'accepted', accepted_at = CURRENT_TIMESTAMP(3) "
          "WHERE invitation_id = '" +
              EscapeSqlString(connection.get(), command.invitation_id) + "'");
      ExecuteStatement(
          connection.get(),
          "UPDATE group_memberships SET membership_status = 'active', joined_at = CURRENT_TIMESTAMP(3) "
          "WHERE group_id = '" +
              EscapeSqlString(connection.get(), invitation->group_id) + "' AND user_id = '" +
              EscapeSqlString(connection.get(), command.actor_user_id) + "'");
      ExecuteStatement(
          connection.get(),
          "INSERT INTO user_scoped_permissions "
          "(user_id, group_id, read_level, write_level) VALUES ('" +
              EscapeSqlString(connection.get(), command.actor_user_id) + "', '" +
              EscapeSqlString(connection.get(), invitation->group_id) +
              "', 10, 10) ON DUPLICATE KEY UPDATE group_id = group_id");

      CommitTransaction(connection.get());
      return *FindInvitationById(command.invitation_id);
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
  }

  permission::ScopedPermission UpsertScopedPermission(
      const std::string& user_id,
      const permission::ScopedPermission& scoped_permission) override {
    auto connection = OpenConnection();
    ExecuteStatement(
        connection.get(),
        "INSERT INTO user_scoped_permissions "
        "(user_id, group_id, read_level, write_level) VALUES ('" +
            EscapeSqlString(connection.get(), user_id) + "', '" +
            EscapeSqlString(connection.get(), scoped_permission.group_id) + "', " +
            std::to_string(scoped_permission.read) + ", " + std::to_string(scoped_permission.write) +
            ") ON DUPLICATE KEY UPDATE read_level = VALUES(read_level), write_level = VALUES(write_level)");
    return scoped_permission;
  }

  group::GroupDetails AssignOwnerGroup(
      const group::AssignOwnerGroupCommand& command,
      const UserDetails& target) override {
    auto connection = OpenConnection();
    BeginTransaction(connection.get());

    try {
      if (target.summary.role != permission::Role::Admin) {
        ExecuteStatement(
            connection.get(),
            "UPDATE users SET role = 'owner' WHERE user_id = '" +
                EscapeSqlString(connection.get(), command.target_user_id) + "'");
      }

      ExecuteStatement(
          connection.get(),
          "INSERT INTO group_owners (group_id, user_id) VALUES ('" +
              EscapeSqlString(connection.get(), command.group_id) + "', '" +
              EscapeSqlString(connection.get(), command.target_user_id) +
              "') ON DUPLICATE KEY UPDATE user_id = VALUES(user_id)");

      ExecuteStatement(
          connection.get(),
          "INSERT INTO group_memberships "
          "(group_id, user_id, membership_status, joined_at) VALUES ('" +
              EscapeSqlString(connection.get(), command.group_id) + "', '" +
              EscapeSqlString(connection.get(), command.target_user_id) +
              "', 'active', CURRENT_TIMESTAMP(3)) "
              "ON DUPLICATE KEY UPDATE membership_status = 'active', joined_at = COALESCE(joined_at, CURRENT_TIMESTAMP(3))");

      CommitTransaction(connection.get());
      return LoadGroupDetails(connection.get(), command.group_id);
    } catch (...) {
      RollbackTransaction(connection.get());
      throw;
    }
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

  static void ExecuteStatement(MYSQL* connection, const std::string& statement) {
    auto result = ExecuteQuery(connection, statement);
    if (result != nullptr) {
      return;
    }
    if (mysql_field_count(connection) != 0) {
      throw std::runtime_error("mysql result fetch failed: " + std::string(mysql_error(connection)));
    }
  }

  static std::size_t ReadCountResult(MYSQL_RES* result) {
    if (result == nullptr) {
      return 0;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == nullptr || row[0] == nullptr) {
      return 0;
    }
    return static_cast<std::size_t>(std::stoull(row[0]));
  }

  static void BeginTransaction(MYSQL* connection) {
    ExecuteStatement(connection, "START TRANSACTION");
  }

  static void CommitTransaction(MYSQL* connection) {
    ExecuteStatement(connection, "COMMIT");
  }

  static void RollbackTransaction(MYSQL* connection) {
    try {
      ExecuteStatement(connection, "ROLLBACK");
    } catch (...) {
    }
  }

  static std::string EscapeSqlString(MYSQL* connection, const std::string& value) {
    std::string escaped;
    escaped.resize(value.size() * 2 + 1);
    const auto escaped_size =
        mysql_real_escape_string(connection, escaped.data(), value.c_str(), value.size());
    escaped.resize(escaped_size);
    return escaped;
  }

  static std::string SqlStringOrNull(MYSQL* connection, const std::optional<std::string>& value) {
    if (!value.has_value()) {
      return "NULL";
    }
    return "'" + EscapeSqlString(connection, *value) + "'";
  }

  static const char* RoleToString(permission::Role role) {
    switch (role) {
      case permission::Role::Admin:
        return "admin";
      case permission::Role::Owner:
        return "owner";
      case permission::Role::Member:
        return "member";
    }
    return "member";
  }

  static permission::Role ParseRole(const char* value) {
    const std::string text = value == nullptr ? "" : value;
    if (text == "admin") {
      return permission::Role::Admin;
    }
    if (text == "owner") {
      return permission::Role::Owner;
    }
    return permission::Role::Member;
  }

  static group::InvitationStatus ParseInvitationStatus(const char* value) {
    const std::string text = value == nullptr ? "" : value;
    if (text == "accepted") {
      return group::InvitationStatus::Accepted;
    }
    if (text == "declined") {
      return group::InvitationStatus::Declined;
    }
    if (text == "expired") {
      return group::InvitationStatus::Expired;
    }
    return group::InvitationStatus::Pending;
  }

  static std::uint8_t ParseUint8(const char* value) {
    return value == nullptr ? 0 : static_cast<std::uint8_t>(std::stoul(value));
  }

  static std::string GenerateUuidLikeString() {
    static std::mt19937_64 generator(std::random_device{}());
    static constexpr char kHex[] = "0123456789abcdef";
    const int groups[] = {8, 4, 4, 4, 12};

    std::string value;
    for (std::size_t index = 0; index < std::size(groups); ++index) {
      if (!value.empty()) {
        value.push_back('-');
      }
      for (int i = 0; i < groups[index]; ++i) {
        value.push_back(kHex[generator() % 16]);
      }
    }
    return value;
  }

  std::optional<group::GroupInvitation> FindInvitationWithConnection(
      MYSQL* connection,
      const std::string& invitation_id) const {
    auto result = ExecuteQuery(
        connection,
        "SELECT invitation_id, group_id, invited_user_id, invited_by_user_id, invitation_status "
        "FROM group_invitations WHERE invitation_id = '" +
            EscapeSqlString(connection, invitation_id) + "' LIMIT 1");
    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row == nullptr) {
      return std::nullopt;
    }
    return group::GroupInvitation{
        .invitation_id = row[0] == nullptr ? "" : row[0],
        .group_id = row[1] == nullptr ? "" : row[1],
        .invited_user_id = row[2] == nullptr ? "" : row[2],
        .invited_by_user_id = row[3] == nullptr ? "" : row[3],
        .status = ParseInvitationStatus(row[4]),
    };
  }

  group::GroupDetails LoadGroupDetails(MYSQL* connection, const std::string& group_id) const {
    auto summary = FindGroupSummaryById(group_id);
    if (!summary.has_value()) {
      throw std::runtime_error("group_not_found");
    }

    group::GroupDetails details{.summary = *summary};

    auto owner_result = ExecuteQuery(
        connection,
        "SELECT user_id FROM group_owners WHERE group_id = '" + EscapeSqlString(connection, group_id) +
            "' ORDER BY user_id");
    std::set<std::string> member_ids;
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(owner_result.get())) != nullptr) {
      if (row[0] != nullptr) {
        details.owner_user_ids.emplace_back(row[0]);
        member_ids.emplace(row[0]);
      }
    }

    auto member_result = ExecuteQuery(
        connection,
        "SELECT user_id FROM group_memberships WHERE group_id = '" +
            EscapeSqlString(connection, group_id) +
            "' AND membership_status = 'active' ORDER BY user_id");
    while ((row = mysql_fetch_row(member_result.get())) != nullptr) {
      if (row[0] != nullptr) {
        member_ids.emplace(row[0]);
      }
    }

    details.member_user_ids.assign(member_ids.begin(), member_ids.end());
    return details;
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

UserGroupRepository& GetMySqlUserGroupRepository() {
  static MySqlUserGroupRepository repository(LoadDbConfig(ResolveConfigPath()));
  return repository;
}

}  // namespace picaresque::user
