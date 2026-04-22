#include <cassert>
#include <stdexcept>
#include <string>

#include "picaresque/group/group_management_service.hpp"
#include "picaresque/permission/api.hpp"
#include "picaresque/table/in_memory_table_repository.hpp"
#include "picaresque/table/table_service.hpp"
#include "picaresque/user/in_memory_user_group_repository.hpp"
#include "picaresque/user/user_management_service.hpp"

namespace group = picaresque::group;
namespace permission = picaresque::permission;
namespace table = picaresque::table;
namespace user = picaresque::user;

int main() {
  user::InMemoryUserGroupRepository user_repository;
  table::InMemoryTableRepository table_repository;
  const user::UserManagementService user_service(user_repository);
  const group::GroupManagementService group_service(user_repository);
  const table::TableService table_service(table_repository, user_repository);

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
  const auto unrelated_member = user_service.CreateUser({
      .login_id = "unrelated-member",
      .user_name = "Unrelated Member",
      .email = "unrelated-member@example.local",
      .password = "change-me",
      .role = permission::Role::Member,
  });

  const auto group_details = group_service.CreateGroup({
      .actor_user_id = owner.summary.user_id,
      .group_name = "table-group",
      .description = std::nullopt,
  });
  const auto invitation = group_service.InviteUser({
      .actor_user_id = owner.summary.user_id,
      .group_id = group_details.summary.group_id,
      .invited_user_id = member.summary.user_id,
  });
  group_service.AcceptInvitation({
      .actor_user_id = member.summary.user_id,
      .invitation_id = invitation.invitation_id,
  });
  group_service.AssignScopedPermission({
      .actor_user_id = owner.summary.user_id,
      .target_user_id = member.summary.user_id,
      .group_id = group_details.summary.group_id,
      .scoped_permission =
          {
              .group_id = group_details.summary.group_id,
              .read = 60,
              .write = 30,
          },
  });

  const auto public_table = table_service.CreateTable({
      .actor_user_id = admin.summary.user_id,
      .table_name = "Public Table",
      .columns =
          {
              {
                  .column_name = "title",
                  .column_type = table::ColumnType::Varchar,
                  .is_required = true,
              },
              {
                  .column_name = "body",
                  .column_type = table::ColumnType::LongText,
              },
              {
                  .column_name = "count",
                  .column_type = table::ColumnType::Integer,
              },
              {
                  .column_name = "price",
                  .column_type = table::ColumnType::Decimal,
              },
              {
                  .column_name = "enabled",
                  .column_type = table::ColumnType::Boolean,
              },
              {
                  .column_name = "scheduled_date",
                  .column_type = table::ColumnType::Date,
              },
              {
                  .column_name = "scheduled_time",
                  .column_type = table::ColumnType::Time,
              },
              {
                  .column_name = "published_at",
                  .column_type = table::ColumnType::DateTime,
              },
              {
                  .column_name = "metadata",
                  .column_type = table::ColumnType::Json,
              },
          },
      .required_permissions = {},
  });
  assert(public_table.columns.size() == 9);
  assert(table_service.GetTable(member.summary.user_id, public_table.summary.table_id)
             .summary.table_name == "Public Table");

  Json::Value values(Json::objectValue);
  values["title"] = "first";
  values["body"] = "long text";
  values["count"] = 3;
  values["price"] = "1200.50";
  values["enabled"] = true;
  values["scheduled_date"] = "2026-04-22";
  values["scheduled_time"] = "06:00";
  values["published_at"] = "2026-04-22T06:00:00";
  values["metadata"]["source"] = "test";
  const auto row = table_service.CreateRow({
      .actor_user_id = member.summary.user_id,
      .table_id = public_table.summary.table_id,
      .values = values,
  });
  assert(table_service.GetRow(member.summary.user_id, public_table.summary.table_id, row.row_id)
             .values["title"]
             .asString() == "first");
  assert(row.values["price"].isString());
  assert(row.values["price"].asString() == "1200.50");

  auto expect_invalid_public_row = [&](Json::Value candidate_values) {
    bool rejected = false;
    try {
      static_cast<void>(table_service.CreateRow({
          .actor_user_id = admin.summary.user_id,
          .table_id = public_table.summary.table_id,
          .values = candidate_values,
      }));
    } catch (const std::runtime_error& error) {
      rejected = std::string(error.what()) == "invalid_column_value";
    }
    assert(rejected);
  };

  Json::Value normalized_values = values;
  normalized_values["title"] = "normalized";
  normalized_values["price"] = 1.1;
  normalized_values["scheduled_date"] = "2024-02-29";
  normalized_values["scheduled_time"] = "18:30:59";
  normalized_values["published_at"] = "2026-04-22 06:00:00";
  const auto normalized_row = table_service.CreateRow({
      .actor_user_id = admin.summary.user_id,
      .table_id = public_table.summary.table_id,
      .values = normalized_values,
  });
  assert(normalized_row.values["price"].isString());
  assert(normalized_row.values["price"].asString() == "1.1");

  Json::Value valid_integer_decimal_values = values;
  valid_integer_decimal_values["title"] = "integer decimal";
  valid_integer_decimal_values["price"] = 10;
  const auto integer_decimal_row = table_service.CreateRow({
      .actor_user_id = admin.summary.user_id,
      .table_id = public_table.summary.table_id,
      .values = valid_integer_decimal_values,
  });
  assert(integer_decimal_row.values["price"].isString());
  assert(integer_decimal_row.values["price"].asString() == "10");

  for (const auto& invalid_time : {"24:00", "06:60", "ab:cd"}) {
    Json::Value candidate = values;
    candidate["scheduled_time"] = invalid_time;
    expect_invalid_public_row(candidate);
  }
  for (const auto& invalid_date : {"2026-02-30", "2026-13-01", "2025-02-29"}) {
    Json::Value candidate = values;
    candidate["scheduled_date"] = invalid_date;
    expect_invalid_public_row(candidate);
  }
  for (const auto& invalid_datetime : {"2026-04-22T24:00:00", "2026-99-99T06:00:00"}) {
    Json::Value candidate = values;
    candidate["published_at"] = invalid_datetime;
    expect_invalid_public_row(candidate);
  }
  for (const auto& invalid_decimal : {"12.34.56", "abc", "--10", "10."}) {
    Json::Value candidate = values;
    candidate["price"] = invalid_decimal;
    expect_invalid_public_row(candidate);
  }

  values["title"] = "updated";
  const auto updated_row = table_service.UpdateRow({
      .actor_user_id = member.summary.user_id,
      .table_id = public_table.summary.table_id,
      .row_id = row.row_id,
      .values = values,
  });
  assert(updated_row.values["title"].asString() == "updated");

  const auto restricted_table = table_service.CreateTable({
      .actor_user_id = owner.summary.user_id,
      .table_name = "Restricted Table",
      .columns =
          {
              {
                  .column_name = "title",
                  .column_type = table::ColumnType::Varchar,
                  .is_required = true,
              },
          },
      .required_permissions =
          {
              {
                  .group_id = group_details.summary.group_id,
                  .read = 60,
                  .write = 60,
              },
          },
  });
  assert(table_service.GetTable(member.summary.user_id, restricted_table.summary.table_id)
             .summary.table_name == "Restricted Table");

  bool unrelated_read_rejected = false;
  try {
    static_cast<void>(table_service.GetTable(
        unrelated_member.summary.user_id,
        restricted_table.summary.table_id));
  } catch (const std::runtime_error& error) {
    unrelated_read_rejected = std::string(error.what()) == "forbidden";
  }
  assert(unrelated_read_rejected);

  Json::Value restricted_values(Json::objectValue);
  restricted_values["title"] = "restricted row";
  bool write_rejected = false;
  try {
    static_cast<void>(table_service.CreateRow({
        .actor_user_id = member.summary.user_id,
        .table_id = restricted_table.summary.table_id,
        .values = restricted_values,
    }));
  } catch (const std::runtime_error& error) {
    write_rejected = std::string(error.what()) == "forbidden";
  }
  assert(write_rejected);

  Json::Value invalid_values(Json::objectValue);
  invalid_values["title"] = "invalid row";
  invalid_values["unknown"] = "not allowed";
  bool unknown_column_rejected = false;
  try {
    static_cast<void>(table_service.CreateRow({
        .actor_user_id = admin.summary.user_id,
        .table_id = restricted_table.summary.table_id,
        .values = invalid_values,
    }));
  } catch (const std::runtime_error& error) {
    unknown_column_rejected = std::string(error.what()) == "unknown_column";
  }
  assert(unknown_column_rejected);

  table_service.DeleteRow({
      .actor_user_id = member.summary.user_id,
      .table_id = public_table.summary.table_id,
      .row_id = row.row_id,
  });
  bool deleted_row_missing = false;
  try {
    static_cast<void>(table_service.GetRow(admin.summary.user_id, public_table.summary.table_id, row.row_id));
  } catch (const std::runtime_error& error) {
    deleted_row_missing = std::string(error.what()) == "row_not_found";
  }
  assert(deleted_row_missing);

  table_service.DeleteTable({
      .actor_user_id = admin.summary.user_id,
      .table_id = public_table.summary.table_id,
  });
  bool deleted_table_missing = false;
  try {
    static_cast<void>(table_service.GetTable(admin.summary.user_id, public_table.summary.table_id));
  } catch (const std::runtime_error& error) {
    deleted_table_missing = std::string(error.what()) == "table_not_found";
  }
  assert(deleted_table_missing);

  return 0;
}
