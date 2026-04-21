#include <drogon/HttpController.h>
#include <json/json.h>

#include "picaresque/group/group_management_service.hpp"
#include "picaresque/http/json_serialization.hpp"
#include "picaresque/http/request_context.hpp"
#include "picaresque/permission/errors.hpp"
#include "../user/mysql_user_group_repository.hpp"

namespace picaresque::http {
namespace {

drogon::HttpResponsePtr BuildErrorResponse(
    const drogon::HttpRequestPtr& request,
    drogon::HttpStatusCode status_code,
    const std::string& code,
    const std::string& message) {
  Json::Value error(Json::objectValue);
  error["meta"] = BuildMeta(request->getHeader("x-request-id"));
  error["error"]["code"] = code;
  error["error"]["message"] = message;

  auto response = drogon::HttpResponse::newHttpJsonResponse(error);
  response->setStatusCode(status_code);
  return response;
}

}  // namespace

class GroupManagementController : public drogon::HttpController<GroupManagementController> {
 public:
  GroupManagementController() : service_(user::GetMySqlUserGroupRepository()) {}

  METHOD_LIST_BEGIN
  ADD_METHOD_TO(GroupManagementController::CreateGroup, "/api/v1/admin/groups", drogon::Post);
  ADD_METHOD_TO(
      GroupManagementController::InviteUser,
      "/api/v1/admin/groups/{1}/invitations",
      drogon::Post);
  ADD_METHOD_TO(
      GroupManagementController::AcceptInvitation,
      "/api/v1/invitations/{1}/accept",
      drogon::Post);
  ADD_METHOD_TO(
      GroupManagementController::AssignScopedPermission,
      "/api/v1/admin/users/{1}/scoped-permissions/{2}",
      drogon::Put);
  ADD_METHOD_TO(
      GroupManagementController::AssignOwnerGroup,
      "/api/v1/admin/groups/{1}/owners",
      drogon::Post);
  METHOD_LIST_END

  void CreateGroup(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    RequestContext context;
    try {
      context = BuildRequestContext(request, true);
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }

    const auto json = request->getJsonObject();
    if (!json) {
      callback(BuildErrorResponse(
          request,
          drogon::k400BadRequest,
          "invalid_json",
          "request body must be valid json"));
      return;
    }

    group::CreateGroupCommand command{
        .actor_user_id = context.authenticated_user->summary.user_id,
        .group_name = (*json).get("group_name", "").asString(),
        .description = (*json).isMember("description")
            ? std::optional<std::string>((*json)["description"].asString())
            : std::nullopt,
    };

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.CreateGroup(command));

      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      response->setStatusCode(drogon::k201Created);
      callback(response);
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void InviteUser(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& group_id) const {
    RequestContext context;
    try {
      context = BuildRequestContext(request, true);
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }

    const auto json = request->getJsonObject();
    if (!json) {
      callback(BuildErrorResponse(
          request,
          drogon::k400BadRequest,
          "invalid_json",
          "request body must be valid json"));
      return;
    }

    group::InviteUserCommand command{
        .actor_user_id = context.authenticated_user->summary.user_id,
        .group_id = group_id,
        .invited_user_id = (*json).get("invited_user_id", "").asString(),
    };

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.InviteUser(command));

      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      response->setStatusCode(drogon::k201Created);
      callback(response);
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void AcceptInvitation(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& invitation_id) const {
    RequestContext context;
    try {
      context = BuildRequestContext(request, true);
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }

    group::AcceptInvitationCommand command{
        .actor_user_id = context.authenticated_user->summary.user_id,
        .invitation_id = invitation_id,
    };

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.AcceptInvitation(command));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void AssignScopedPermission(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& user_id,
      const std::string& group_id) const {
    RequestContext context;
    try {
      context = BuildRequestContext(request, true);
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }

    const auto json = request->getJsonObject();
    if (!json) {
      callback(BuildErrorResponse(
          request,
          drogon::k400BadRequest,
          "invalid_json",
          "request body must be valid json"));
      return;
    }

    group::AssignScopedPermissionCommand command{
        .actor_user_id = context.authenticated_user->summary.user_id,
        .target_user_id = user_id,
        .group_id = group_id,
        .scoped_permission =
            {
                .name = group_id,
                .read = static_cast<std::uint8_t>((*json).get("read", 0).asUInt()),
                .write = static_cast<std::uint8_t>((*json).get("write", 0).asUInt()),
            },
    };

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.AssignScopedPermission(command));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const permission::ValidationError& error) {
      callback(BuildErrorResponse(
          request,
          drogon::k400BadRequest,
          "invalid_permission",
          error.what()));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void AssignOwnerGroup(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& group_id) const {
    RequestContext context;
    try {
      context = BuildRequestContext(request, true);
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }

    const auto json = request->getJsonObject();
    if (!json) {
      callback(BuildErrorResponse(
          request,
          drogon::k400BadRequest,
          "invalid_json",
          "request body must be valid json"));
      return;
    }

    group::AssignOwnerGroupCommand command{
        .actor_user_id = context.authenticated_user->summary.user_id,
        .target_user_id = (*json).get("target_user_id", "").asString(),
        .group_id = group_id,
    };

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.AssignOwnerGroup(command));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

 private:
  drogon::HttpResponsePtr MapError(
      const drogon::HttpRequestPtr& request,
      const std::string& error_code) const {
    if (error_code == "forbidden") {
      return BuildErrorResponse(request, drogon::k403Forbidden, error_code, "operation is forbidden");
    }
    if (error_code == "user_not_found" || error_code == "group_not_found" ||
        error_code == "invitation_not_found") {
      return BuildErrorResponse(request, drogon::k404NotFound, error_code, error_code);
    }
    if (error_code == "group_name_already_exists" || error_code == "invitation_already_exists" ||
        error_code == "user_already_member") {
      return BuildErrorResponse(request, drogon::k409Conflict, error_code, error_code);
    }
    if (error_code == "target_not_group_member") {
      return BuildErrorResponse(request, drogon::k400BadRequest, error_code, error_code);
    }
    return BuildErrorResponse(request, drogon::k400BadRequest, error_code, error_code);
  }

  group::GroupManagementService service_;
};

}  // namespace picaresque::http
