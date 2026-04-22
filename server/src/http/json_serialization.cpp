#include "picaresque/http/json_serialization.hpp"

#include <json/json.h>

#include "picaresque/table/table_service.hpp"

namespace picaresque::http {
namespace {

const char* RoleToString(permission::Role role) {
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

const char* InvitationStatusToString(group::InvitationStatus status) {
  switch (status) {
    case group::InvitationStatus::Pending:
      return "pending";
    case group::InvitationStatus::Accepted:
      return "accepted";
    case group::InvitationStatus::Declined:
      return "declined";
    case group::InvitationStatus::Expired:
      return "expired";
  }

  return "pending";
}

}  // namespace

Json::Value ToJson(permission::Role role) {
  return Json::Value(RoleToString(role));
}

Json::Value ToJson(const permission::ScopedPermission& permission) {
  Json::Value value(Json::objectValue);
  value["group_id"] = permission.group_id;
  value["read"] = permission.read;
  value["write"] = permission.write;
  return value;
}

Json::Value ToJson(const permission::AccessRequirement& requirement) {
  Json::Value value(Json::objectValue);
  value["group_id"] = requirement.group_id;
  value["read"] = requirement.read;
  value["write"] = requirement.write;
  return value;
}

Json::Value ToJson(const article::ArticleSummary& summary) {
  Json::Value value(Json::objectValue);
  value["article_id"] = summary.article_id;
  value["title"] = summary.title;
  value["created_by_user_id"] = summary.created_by_user_id;
  value["updated_by_user_id"] = summary.updated_by_user_id;
  value["is_locked"] = summary.is_locked;
  value["locked_by_user_id"] = summary.locked_by_user_id.has_value() ? *summary.locked_by_user_id : "";
  value["locked_at"] = summary.locked_at.has_value() ? *summary.locked_at : "";

  Json::Value required_permissions(Json::arrayValue);
  for (const auto& permission : summary.required_permissions) {
    required_permissions.append(ToJson(permission));
  }
  value["required_permissions"] = required_permissions;
  return value;
}

Json::Value ToJson(const article::ArticleDetails& details) {
  Json::Value value = ToJson(details.summary);
  value["body"] = details.body;
  return value;
}

Json::Value ToJson(table::ColumnType column_type) {
  return Json::Value(table::ColumnTypeToString(column_type));
}

Json::Value ToJson(const table::ColumnDefinition& column) {
  Json::Value value(Json::objectValue);
  value["column_id"] = column.column_id;
  value["column_name"] = column.column_name;
  value["column_type"] = ToJson(column.column_type);
  value["is_required"] = column.is_required;
  return value;
}

Json::Value ToJson(const table::TableSummary& summary) {
  Json::Value value(Json::objectValue);
  value["table_id"] = summary.table_id;
  value["table_name"] = summary.table_name;
  value["created_by_user_id"] = summary.created_by_user_id;
  value["updated_by_user_id"] = summary.updated_by_user_id;

  Json::Value required_permissions(Json::arrayValue);
  for (const auto& permission : summary.required_permissions) {
    required_permissions.append(ToJson(permission));
  }
  value["required_permissions"] = required_permissions;
  return value;
}

Json::Value ToJson(const table::TableDetails& details) {
  Json::Value value = ToJson(details.summary);
  Json::Value columns(Json::arrayValue);
  for (const auto& column : details.columns) {
    columns.append(ToJson(column));
  }
  value["columns"] = columns;
  return value;
}

Json::Value ToJson(const table::TableRow& row) {
  Json::Value value(Json::objectValue);
  value["row_id"] = row.row_id;
  value["table_id"] = row.table_id;
  value["created_by_user_id"] = row.created_by_user_id;
  value["updated_by_user_id"] = row.updated_by_user_id;
  value["values"] = row.values;
  return value;
}

Json::Value ToJson(group::InvitationStatus status) {
  return Json::Value(InvitationStatusToString(status));
}

Json::Value ToJson(const group::GroupSummary& summary) {
  Json::Value value(Json::objectValue);
  value["group_id"] = summary.group_id;
  value["group_name"] = summary.group_name;
  value["description"] = summary.description.has_value() ? *summary.description : "";
  value["created_by_user_id"] = summary.created_by_user_id;
  return value;
}

Json::Value ToJson(const group::GroupDetails& details) {
  Json::Value value = ToJson(details.summary);

  Json::Value owner_user_ids(Json::arrayValue);
  for (const auto& owner_user_id : details.owner_user_ids) {
    owner_user_ids.append(owner_user_id);
  }

  Json::Value member_user_ids(Json::arrayValue);
  for (const auto& member_user_id : details.member_user_ids) {
    member_user_ids.append(member_user_id);
  }

  value["owner_user_ids"] = owner_user_ids;
  value["member_user_ids"] = member_user_ids;
  return value;
}

Json::Value ToJson(const group::GroupInvitation& invitation) {
  Json::Value value(Json::objectValue);
  value["invitation_id"] = invitation.invitation_id;
  value["group_id"] = invitation.group_id;
  value["invited_user_id"] = invitation.invited_user_id;
  value["invited_by_user_id"] = invitation.invited_by_user_id;
  value["status"] = ToJson(invitation.status);
  return value;
}

Json::Value ToJson(const user::UserSummary& summary) {
  Json::Value value(Json::objectValue);
  value["user_id"] = summary.user_id;
  value["login_id"] = summary.login_id;
  value["user_name"] = summary.user_name;
  value["email"] = summary.email;
  value["role"] = ToJson(summary.role);
  value["is_active"] = summary.is_active;
  return value;
}

Json::Value ToJson(const user::UserDetails& details) {
  Json::Value value = ToJson(details.summary);

  Json::Value owned_groups(Json::arrayValue);
  for (const auto& owned_group_id : details.owned_groups) {
    owned_groups.append(owned_group_id);
  }

  Json::Value scoped_permissions(Json::arrayValue);
  for (const auto& scoped_permission : details.scoped_permissions) {
    scoped_permissions.append(ToJson(scoped_permission));
  }

  value["owned_groups"] = owned_groups;
  value["scoped_permissions"] = scoped_permissions;
  return value;
}

Json::Value BuildMeta(std::string_view request_id) {
  Json::Value meta(Json::objectValue);
  meta["request_id"] = std::string(request_id);
  meta["version"] = "v1";
  return meta;
}

}  // namespace picaresque::http
