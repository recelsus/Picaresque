#include "picaresque/http/json_serialization.hpp"

#include <json/json.h>

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
  value["name"] = permission.name;
  value["read"] = permission.read;
  value["write"] = permission.write;
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
  for (const auto& group_name : details.owned_groups) {
    owned_groups.append(group_name);
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
