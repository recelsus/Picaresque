#include <cassert>
#include <stdexcept>
#include <string>

#include "picaresque/article/article_service.hpp"
#include "picaresque/article/in_memory_article_repository.hpp"
#include "picaresque/embedded_query/embedded_query_service.hpp"
#include "picaresque/embedded_query/in_memory_embedded_query_repository.hpp"
#include "picaresque/group/group_management_service.hpp"
#include "picaresque/permission/api.hpp"
#include "picaresque/table/in_memory_table_repository.hpp"
#include "picaresque/table/table_service.hpp"
#include "picaresque/user/in_memory_user_group_repository.hpp"
#include "picaresque/user/user_management_service.hpp"

namespace article = picaresque::article;
namespace embedded_query = picaresque::embedded_query;
namespace group = picaresque::group;
namespace permission = picaresque::permission;
namespace table = picaresque::table;
namespace user = picaresque::user;

int main() {
  user::InMemoryUserGroupRepository user_repository;
  article::InMemoryArticleRepository article_repository;
  table::InMemoryTableRepository table_repository;
  embedded_query::InMemoryEmbeddedQueryRepository query_repository;

  const user::UserManagementService user_service(user_repository);
  const group::GroupManagementService group_service(user_repository);
  const table::TableService table_service(table_repository, user_repository);
  embedded_query::EmbeddedQueryService query_service(query_repository, table_repository, user_repository);
  const article::ArticleService article_service(article_repository, user_repository, query_service);

  const auto admin = user_service.CreateInitialAdmin({
      .login_id = "admin",
      .user_name = "Initial Admin",
      .email = "admin@example.local",
      .password = "change-me",
      .role = permission::Role::Admin,
  });
  const auto owner = user_service.CreateUser({
      .login_id = "owner",
      .user_name = "Owner",
      .email = "owner@example.local",
      .password = "change-me",
      .role = permission::Role::Owner,
  });
  const auto member = user_service.CreateUser({
      .login_id = "member",
      .user_name = "Member",
      .email = "member@example.local",
      .password = "change-me",
      .role = permission::Role::Member,
  });

  const auto details = group_service.CreateGroup({
      .actor_user_id = owner.summary.user_id,
      .group_name = "query-group",
      .description = std::nullopt,
  });
  const auto invitation = group_service.InviteUser({
      .actor_user_id = owner.summary.user_id,
      .group_id = details.summary.group_id,
      .invited_user_id = member.summary.user_id,
  });
  group_service.AcceptInvitation({
      .actor_user_id = member.summary.user_id,
      .invitation_id = invitation.invitation_id,
  });
  group_service.AssignScopedPermission({
      .actor_user_id = owner.summary.user_id,
      .target_user_id = member.summary.user_id,
      .group_id = details.summary.group_id,
      .scoped_permission =
          {
              .group_id = details.summary.group_id,
              .read = 60,
              .write = 30,
          },
  });

  const auto writable_table = table_service.CreateTable({
      .actor_user_id = owner.summary.user_id,
      .table_name = "Orders",
      .columns =
          {
              {.column_name = "name", .column_type = table::ColumnType::Varchar},
              {.column_name = "owner_key", .column_type = table::ColumnType::Varchar},
              {.column_name = "amount", .column_type = table::ColumnType::Integer},
              {.column_name = "note", .column_type = table::ColumnType::Varchar},
          },
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  const auto joined_table = table_service.CreateTable({
      .actor_user_id = owner.summary.user_id,
      .table_name = "Owners",
      .columns =
          {
              {.column_name = "owner_key", .column_type = table::ColumnType::Varchar},
              {.column_name = "label", .column_type = table::ColumnType::Varchar},
              {.column_name = "region_key", .column_type = table::ColumnType::Varchar},
          },
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  const auto second_joined_table = table_service.CreateTable({
      .actor_user_id = owner.summary.user_id,
      .table_name = "Regions",
      .columns =
          {
              {.column_name = "region_key", .column_type = table::ColumnType::Varchar},
              {.column_name = "region_name", .column_type = table::ColumnType::Varchar},
          },
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  const auto read_only_table = table_service.CreateTable({
      .actor_user_id = owner.summary.user_id,
      .table_name = "Readonly",
      .columns = {{{.column_name = "name", .column_type = table::ColumnType::Varchar}}},
      .required_permissions = {{.group_id = details.summary.group_id, .read = 60, .write = 60}},
  });

  const std::string body =
      "inline `query: SELECT count(*) FROM " + writable_table.summary.table_id + " LIMIT 1`.\n"
      "```query\nSELECT name FROM " + writable_table.summary.table_id + " LIMIT 10\n```";
  const auto article_with_queries = article_service.CreateArticle({
      .actor_user_id = member.summary.user_id,
      .title = "query article",
      .body = body,
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  assert(article_with_queries.body.find("`query: id=q_") != std::string::npos);
  assert(article_with_queries.body.find("```query\nid=q_") != std::string::npos);
  assert(query_repository.ListByArticleId(article_with_queries.summary.article_id).size() == 2);
  assert(article_service.GetArticle(member.summary.user_id, article_with_queries.summary.article_id)
             .body.find("SELECT count(*)") != std::string::npos);

  Json::Value row_values(Json::objectValue);
  row_values["name"] = "first";
  row_values["owner_key"] = "owner_a";
  row_values["amount"] = 10;
  static_cast<void>(table_service.CreateRow({
      .actor_user_id = member.summary.user_id,
      .table_id = writable_table.summary.table_id,
      .values = row_values,
  }));
  Json::Value second_row_values(Json::objectValue);
  second_row_values["name"] = "second";
  second_row_values["owner_key"] = "owner_b";
  second_row_values["amount"] = 20;
  second_row_values["note"] = "memo";
  static_cast<void>(table_service.CreateRow({
      .actor_user_id = member.summary.user_id,
      .table_id = writable_table.summary.table_id,
      .values = second_row_values,
  }));
  Json::Value joined_row_values(Json::objectValue);
  joined_row_values["owner_key"] = "owner_a";
  joined_row_values["label"] = "alpha";
  joined_row_values["region_key"] = "region_1";
  static_cast<void>(table_service.CreateRow({
      .actor_user_id = member.summary.user_id,
      .table_id = joined_table.summary.table_id,
      .values = joined_row_values,
  }));
  Json::Value second_joined_row_values(Json::objectValue);
  second_joined_row_values["region_key"] = "region_1";
  second_joined_row_values["region_name"] = "north";
  static_cast<void>(table_service.CreateRow({
      .actor_user_id = member.summary.user_id,
      .table_id = second_joined_table.summary.table_id,
      .values = second_joined_row_values,
  }));

  const auto initial_queries = query_repository.ListByArticleId(article_with_queries.summary.article_id);
  const auto count_result = query_service.ExecuteSavedQuery(
      article_with_queries.summary.article_id,
      initial_queries.front().query_id);
  assert(count_result.columns.size() == 1);
  assert(count_result.columns.front() == "count");
  assert(count_result.rows.size() == 1);
  assert(count_result.rows[0]["count"].asUInt64() == 2);

  const auto select_result = query_service.ExecuteSavedQuery(
      article_with_queries.summary.article_id,
      initial_queries.back().query_id);
  assert(select_result.columns.size() == 1);
  assert(select_result.columns.front() == "name");
  assert(select_result.rows.size() == 2);
  assert(select_result.rows[0]["name"].asString() == "first");

  const auto where_order_article = article_service.CreateArticle({
      .actor_user_id = member.summary.user_id,
      .title = "where order article",
      .body = "`query: SELECT name FROM " + writable_table.summary.table_id + " WHERE name = 'second' ORDER BY name DESC`",
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  const auto where_order_query = query_repository.ListByArticleId(where_order_article.summary.article_id).front();
  const auto where_order_result = query_service.ExecuteSavedQuery(
      where_order_article.summary.article_id,
      where_order_query.query_id);
  assert(where_order_result.rows.size() == 1);
  assert(where_order_result.rows[0]["name"].asString() == "second");

  const auto where_and_or_article = article_service.CreateArticle({
      .actor_user_id = member.summary.user_id,
      .title = "where and or article",
      .body = "`query: SELECT name FROM " + writable_table.summary.table_id +
          " WHERE (name = 'missing' OR name = 'first') AND owner_key = 'owner_a'`",
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  const auto where_and_or_query =
      query_repository.ListByArticleId(where_and_or_article.summary.article_id).front();
  const auto where_and_or_result =
      query_service.ExecuteSavedQuery(where_and_or_article.summary.article_id, where_and_or_query.query_id);
  assert(where_and_or_result.rows.size() == 1);
  assert(where_and_or_result.rows[0]["name"].asString() == "first");

  const auto where_extended_article = article_service.CreateArticle({
      .actor_user_id = member.summary.user_id,
      .title = "where extended article",
      .body = "`query: SELECT name FROM " + writable_table.summary.table_id +
          " WHERE name LIKE 'sec%' OR amount BETWEEN 9 AND 10 OR name IN ('missing', 'first') OR note IS NULL LIMIT 2`",
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  const auto where_extended_query =
      query_repository.ListByArticleId(where_extended_article.summary.article_id).front();
  const auto where_extended_result =
      query_service.ExecuteSavedQuery(where_extended_article.summary.article_id, where_extended_query.query_id);
  assert(where_extended_result.rows.size() == 2);

  const auto aggregate_article = article_service.CreateArticle({
      .actor_user_id = member.summary.user_id,
      .title = "aggregate article",
      .body = "`query: SELECT count(name), sum(amount), avg(amount), min(amount), max(amount) FROM " +
          writable_table.summary.table_id + "`",
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  const auto aggregate_query = query_repository.ListByArticleId(aggregate_article.summary.article_id).front();
  const auto aggregate_result = query_service.ExecuteSavedQuery(aggregate_article.summary.article_id, aggregate_query.query_id);
  assert(aggregate_result.rows.size() == 1);
  assert(aggregate_result.rows[0]["count(name)"].asUInt64() == 2);
  assert(aggregate_result.rows[0]["sum(amount)"].asDouble() == 30.0);
  assert(aggregate_result.rows[0]["avg(amount)"].asDouble() == 15.0);
  assert(aggregate_result.rows[0]["min(amount)"].asInt() == 10);
  assert(aggregate_result.rows[0]["max(amount)"].asInt() == 20);

  const auto group_by_article = article_service.CreateArticle({
      .actor_user_id = member.summary.user_id,
      .title = "group by article",
      .body = "`query: SELECT owner_key, count(*), sum(amount) FROM " + writable_table.summary.table_id +
          " GROUP BY owner_key ORDER BY owner_key LIMIT 10`",
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  const auto group_by_query = query_repository.ListByArticleId(group_by_article.summary.article_id).front();
  const auto group_by_result = query_service.ExecuteSavedQuery(group_by_article.summary.article_id, group_by_query.query_id);
  assert(group_by_result.rows.size() == 2);
  assert(group_by_result.rows[0]["owner_key"].asString() == "owner_a");
  assert(group_by_result.rows[0]["count"].asUInt64() == 1);
  assert(group_by_result.rows[0]["sum(amount)"].asDouble() == 10.0);
  assert(group_by_result.rows[1]["owner_key"].asString() == "owner_b");
  assert(group_by_result.rows[1]["sum(amount)"].asDouble() == 20.0);

  const auto join_article = article_service.CreateArticle({
      .actor_user_id = member.summary.user_id,
      .title = "join article",
      .body = "`query: SELECT " + writable_table.summary.table_id + ".name, " + joined_table.summary.table_id +
          ".label FROM " + writable_table.summary.table_id + " JOIN " + joined_table.summary.table_id + " ON " +
          writable_table.summary.table_id + ".owner_key = " + joined_table.summary.table_id + ".owner_key`",
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  const auto join_query = query_repository.ListByArticleId(join_article.summary.article_id).front();
  const auto join_result = query_service.ExecuteSavedQuery(join_article.summary.article_id, join_query.query_id);
  assert(join_result.rows.size() == 1);
  assert(join_result.rows[0][writable_table.summary.table_id + ".name"].asString() == "first");
  assert(join_result.rows[0][joined_table.summary.table_id + ".label"].asString() == "alpha");

  const auto multi_join_article = article_service.CreateArticle({
      .actor_user_id = member.summary.user_id,
      .title = "multi join article",
      .body = "`query: SELECT " + writable_table.summary.table_id + ".name, " + joined_table.summary.table_id +
          ".label, " + second_joined_table.summary.table_id + ".region_name FROM " +
          writable_table.summary.table_id + " JOIN " + joined_table.summary.table_id + " ON " +
          writable_table.summary.table_id + ".owner_key = " + joined_table.summary.table_id + ".owner_key JOIN " +
          second_joined_table.summary.table_id + " ON " + joined_table.summary.table_id + ".region_key = " +
          second_joined_table.summary.table_id + ".region_key`",
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  const auto multi_join_query = query_repository.ListByArticleId(multi_join_article.summary.article_id).front();
  const auto multi_join_result =
      query_service.ExecuteSavedQuery(multi_join_article.summary.article_id, multi_join_query.query_id);
  assert(multi_join_result.rows.size() == 1);
  assert(multi_join_result.rows[0][second_joined_table.summary.table_id + ".region_name"].asString() == "north");

  const auto current_queries = query_repository.ListByArticleId(article_with_queries.summary.article_id);
  const auto updated_body = "`query: id=" + current_queries.front().query_id + " SELECT name FROM " +
      writable_table.summary.table_id + " LIMIT 2`";
  const auto updated_article = article_service.UpdateArticle({
      .actor_user_id = member.summary.user_id,
      .article_id = article_with_queries.summary.article_id,
      .title = "query article updated",
      .body = updated_body,
      .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
  });
  assert(updated_article.body.find(current_queries.front().query_id) != std::string::npos);
  assert(query_repository.ListByArticleId(article_with_queries.summary.article_id).size() == 1);

  auto expect_query_rejected = [&](const std::string& rejected_body, const std::string& expected_message) {
    bool rejected = false;
    try {
      static_cast<void>(article_service.UpdateArticle({
          .actor_user_id = member.summary.user_id,
          .article_id = article_with_queries.summary.article_id,
          .title = "rejected",
          .body = rejected_body,
          .required_permissions = {{.group_id = details.summary.group_id, .read = 30, .write = 30}},
      }));
    } catch (const embedded_query::QueryValidationException& error) {
      rejected = !error.query_errors.empty() && error.query_errors.front().message == expected_message;
    }
    assert(rejected);
  };

  expect_query_rejected("`query: UPDATE " + writable_table.summary.table_id + " SET name = 'x' LIMIT 1`", "only SELECT is allowed");
  expect_query_rejected(
      "`query: SELECT name FROM " + writable_table.summary.table_id + " LIMIT 1; SELECT 1`",
      "multiple statements are not allowed");
  expect_query_rejected("`query: SELECT name FROM table_missing LIMIT 1`", "table not found");
  expect_query_rejected(
      "`query: SELECT name FROM " + writable_table.summary.table_id + " LIMIT 501`",
      "row limit exceeded");
  expect_query_rejected(
      "`query: id=q_unknown SELECT name FROM " + writable_table.summary.table_id + " LIMIT 1`",
      "unknown query_id");
  expect_query_rejected(
      "`query: SELECT name FROM " + read_only_table.summary.table_id + " LIMIT 1`",
      "table write permission required");
  expect_query_rejected(
      "`query: SELECT name FROM " + writable_table.summary.table_id + " JOIN " + joined_table.summary.table_id +
          " ON " + writable_table.summary.table_id + ".owner_key = " + joined_table.summary.table_id + ".owner_key`",
      "qualified column reference required");
  expect_query_rejected(
      "`query: SELECT " + writable_table.summary.table_id + ".name FROM " + writable_table.summary.table_id +
          " JOIN " + joined_table.summary.table_id + " ON owner_key = " + joined_table.summary.table_id + ".owner_key`",
      "qualified column reference required");
  expect_query_rejected(
      "`query: SELECT table_missing.name FROM " + writable_table.summary.table_id + "`",
      "undeclared table reference");
  expect_query_rejected(
      "`query: SELECT name FROM " + writable_table.summary.table_id + " LEFT JOIN " + joined_table.summary.table_id +
          " ON " + writable_table.summary.table_id + ".owner_key = " + joined_table.summary.table_id + ".owner_key`",
      "unsupported JOIN");
  expect_query_rejected(
      "`query: SELECT name FROM " + writable_table.summary.table_id + " GROUP BY name`",
      "unsupported GROUP BY");

  const auto read_restricted_article = article_service.CreateArticle({
      .actor_user_id = admin.summary.user_id,
      .title = "admin only",
      .body = "hidden",
      .required_permissions = {{.group_id = "admin_only_group", .read = 90, .write = 90}},
  });
  bool read_rejected = false;
  try {
    static_cast<void>(article_service.GetArticle(member.summary.user_id, read_restricted_article.summary.article_id));
  } catch (const std::runtime_error& error) {
    read_rejected = std::string(error.what()) == "forbidden";
  }
  assert(read_rejected);

  return 0;
}
