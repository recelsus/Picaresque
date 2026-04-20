#include <drogon/HttpController.h>
#include <json/json.h>

#include "picaresque/http/json_serialization.hpp"
#include "picaresque/user/user_management_service.hpp"

namespace picaresque::http {

class UserManagementController : public drogon::HttpController<UserManagementController> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(UserManagementController::GetSetupStatus, "/api/v1/setup/status", drogon::Get);
  ADD_METHOD_TO(UserManagementController::CreateInitialAdmin, "/api/v1/setup/admin", drogon::Post);
  ADD_METHOD_TO(UserManagementController::ListUsers, "/api/v1/admin/users", drogon::Get);
  ADD_METHOD_TO(UserManagementController::GetUser, "/api/v1/admin/users/{1}", drogon::Get);
  METHOD_LIST_END

  void GetSetupStatus(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    Json::Value body(Json::objectValue);
    body["meta"] = BuildMeta(request->getHeader("x-request-id"));
    body["data"]["setupComplete"] = service_.IsSetupComplete();
    body["data"]["userCount"] = static_cast<Json::UInt64>(service_.CountUsers());
    body["data"]["canCreateInitialAdmin"] = !service_.IsSetupComplete();
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
  }

  void CreateInitialAdmin(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
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
    command.login_id = (*json).get("loginId", "").asString();
    command.user_name = (*json).get("userName", "").asString();
    command.email = (*json).get("email", "").asString();
    command.password = (*json).get("password", "").asString();
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
    user::UserListFilter filter;
    const auto role_param = request->getOptionalParameter<std::string>("role");
    const auto active_param = request->getOptionalParameter<bool>("isActive");

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

  void GetUser(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& user_id) const {
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
