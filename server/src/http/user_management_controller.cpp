#include <drogon/HttpController.h>
#include <json/json.h>

#include <optional>

#include "picaresque/http/json_serialization.hpp"
#include "picaresque/http/request_context.hpp"
#include "picaresque/user/user_management_service.hpp"
#include "../user/mysql_user_group_repository.hpp"

namespace picaresque::http {
namespace {

std::string ReadStringField(
    const Json::Value& json,
    std::string_view snake_case_key,
    std::string_view camel_case_key,
    const std::string& default_value = "") {
  if (json.isMember(std::string(snake_case_key))) {
    return json[std::string(snake_case_key)].asString();
  }
  if (json.isMember(std::string(camel_case_key))) {
    return json[std::string(camel_case_key)].asString();
  }
  return default_value;
}

std::optional<permission::Role> ParseRole(const std::string& value) {
  if (value == "admin") {
    return permission::Role::Admin;
  }
  if (value == "owner") {
    return permission::Role::Owner;
  }
  if (value == "member") {
    return permission::Role::Member;
  }
  return std::nullopt;
}

}  // namespace

class UserManagementController : public drogon::HttpController<UserManagementController> {
 public:
  UserManagementController() : service_(user::GetMySqlUserGroupRepository()) {}

  METHOD_LIST_BEGIN
  ADD_METHOD_TO(UserManagementController::GetSetupStatus, "/api/v1/setup/status", drogon::Get);
  ADD_METHOD_TO(UserManagementController::CreateInitialAdmin, "/api/v1/setup/admin", drogon::Post);
  ADD_METHOD_TO(UserManagementController::CreateUser, "/api/v1/admin/users", drogon::Post);
  ADD_METHOD_TO(UserManagementController::ListUsers, "/api/v1/admin/users", drogon::Get);
  ADD_METHOD_TO(UserManagementController::GetUser, "/api/v1/admin/users/{1}", drogon::Get);
  METHOD_LIST_END

  void GetSetupStatus(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
      static_cast<void>(BuildRequestContext(request, false));
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }

    Json::Value body(Json::objectValue);
    body["meta"] = BuildMeta(request->getHeader("x-request-id"));
    body["data"]["setup_complete"] = service_.IsSetupComplete();
    body["data"]["user_count"] = static_cast<Json::UInt64>(service_.CountUsers());
    body["data"]["can_create_initial_admin"] = !service_.IsSetupComplete();
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
  }

  void CreateInitialAdmin(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
      static_cast<void>(BuildRequestContext(request, false));
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }

    const auto json = request->getJsonObject();
    if (!json) {
      Json::Value error(Json::objectValue);
      error["meta"] = BuildMeta(request->getHeader("x-request-id"));
      error["error"]["code"] = "invalid_json";
      error["error"]["message"] = "request body must be valid json";

      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k400BadRequest);
      callback(response);
      return;
    }

    user::CreateUserCommand command;
    command.login_id = ReadStringField(*json, "login_id", "loginId");
    command.user_name = ReadStringField(*json, "user_name", "userName");
    command.email = ReadStringField(*json, "email", "email");
    command.password = ReadStringField(*json, "password", "password");
    command.role = permission::Role::Admin;

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.CreateInitialAdmin(command));

      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      response->setStatusCode(drogon::k201Created);
      callback(response);
    } catch (const std::runtime_error& error_message) {
      Json::Value error(Json::objectValue);
      error["meta"] = BuildMeta(request->getHeader("x-request-id"));

      const std::string message = error_message.what();
      if (message == "setup already completed") {
        error["error"]["code"] = "setup_already_completed";
        error["error"]["message"] = "initial admin already exists";

        auto response = drogon::HttpResponse::newHttpJsonResponse(error);
        response->setStatusCode(drogon::k409Conflict);
        callback(response);
        return;
      }

      error["error"]["code"] = "invalid_setup_request";
      error["error"]["message"] = message;

      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k400BadRequest);
      callback(response);
    }
  }

  void ListUsers(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
      static_cast<void>(BuildRequestContext(request, false));
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }

    user::UserListFilter filter;
    const auto role_param = request->getOptionalParameter<std::string>("role");
    const auto active_param = request->getOptionalParameter<bool>("is_active");

    if (role_param.has_value()) {
      if (*role_param == "admin") {
        filter.role = permission::Role::Admin;
      } else if (*role_param == "owner") {
        filter.role = permission::Role::Owner;
      } else if (*role_param == "member") {
        filter.role = permission::Role::Member;
      }
    }

    if (active_param.has_value()) {
      filter.is_active = *active_param;
    }

    Json::Value body(Json::objectValue);
    body["meta"] = BuildMeta(request->getHeader("x-request-id"));

    Json::Value data(Json::arrayValue);
    for (const auto& summary : service_.ListUsers(filter)) {
      data.append(ToJson(summary));
    }
    body["data"] = data;

    callback(drogon::HttpResponse::newHttpJsonResponse(body));
  }

  void CreateUser(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
      static_cast<void>(BuildRequestContext(request, false));
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }

    const auto json = request->getJsonObject();
    if (!json) {
      Json::Value error(Json::objectValue);
      error["meta"] = BuildMeta(request->getHeader("x-request-id"));
      error["error"]["code"] = "invalid_json";
      error["error"]["message"] = "request body must be valid json";

      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k400BadRequest);
      callback(response);
      return;
    }

    const auto role_text = ReadStringField(*json, "role", "role", "member");
    const auto role = ParseRole(role_text);
    if (!role.has_value()) {
      Json::Value error(Json::objectValue);
      error["meta"] = BuildMeta(request->getHeader("x-request-id"));
      error["error"]["code"] = "invalid_role";
      error["error"]["message"] = "role must be admin, owner, or member";

      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k400BadRequest);
      callback(response);
      return;
    }

    user::CreateUserCommand command;
    command.login_id = ReadStringField(*json, "login_id", "loginId");
    command.user_name = ReadStringField(*json, "user_name", "userName");
    command.email = ReadStringField(*json, "email", "email");
    command.password = ReadStringField(*json, "password", "password");
    command.role = *role;

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.CreateUser(command));

      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      response->setStatusCode(drogon::k201Created);
      callback(response);
    } catch (const std::runtime_error& error_message) {
      Json::Value error(Json::objectValue);
      error["meta"] = BuildMeta(request->getHeader("x-request-id"));

      const std::string message = error_message.what();
      if (message == "setup_not_completed") {
        error["error"]["code"] = "setup_not_completed";
        error["error"]["message"] = "initial admin must be created first";

        auto response = drogon::HttpResponse::newHttpJsonResponse(error);
        response->setStatusCode(drogon::k409Conflict);
        callback(response);
        return;
      }

      if (message == "user_already_exists") {
        error["error"]["code"] = "user_already_exists";
        error["error"]["message"] = "loginId or email already exists";

        auto response = drogon::HttpResponse::newHttpJsonResponse(error);
        response->setStatusCode(drogon::k409Conflict);
        callback(response);
        return;
      }

      error["error"]["code"] = "invalid_user_request";
      error["error"]["message"] = message;

      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k400BadRequest);
      callback(response);
    }
  }

  void GetUser(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& user_id) const {
    try {
      static_cast<void>(BuildRequestContext(request, false));
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.GetUserDetails(user_id));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::exception&) {
      Json::Value error(Json::objectValue);
      error["meta"] = BuildMeta(request->getHeader("x-request-id"));
      error["error"]["code"] = "user_not_found";
      error["error"]["message"] = "requested user was not found";

      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k404NotFound);
      callback(response);
    }
  }

 private:
  user::UserManagementService service_;
};

}  // namespace picaresque::http
