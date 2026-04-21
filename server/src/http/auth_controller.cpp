#include <drogon/HttpController.h>
#include <json/json.h>

#include "picaresque/auth/auth_service.hpp"
#include "picaresque/http/json_serialization.hpp"
#include "picaresque/http/request_context.hpp"
#include "../user/mysql_user_group_repository.hpp"

namespace picaresque::http {
namespace {

std::string ReadApiKeyFromRequest(const drogon::HttpRequestPtr& request) {
  return request->getHeader("x-api-key");
}

Json::Value ToJson(const auth::ApiKeyInfo& api_key_info) {
  Json::Value value(Json::objectValue);
  value["user_id"] = api_key_info.user_id;
  value["key_prefix"] = api_key_info.key_prefix;
  value["enabled"] = api_key_info.enabled;
  return value;
}

Json::Value ToJson(const auth::IssuedApiKey& issued_api_key) {
  Json::Value value(Json::objectValue);
  value["user_id"] = issued_api_key.info.user_id;
  value["key_prefix"] = issued_api_key.info.key_prefix;
  value["enabled"] = issued_api_key.info.enabled;
  value["plain_api_key"] = issued_api_key.plain_api_key;
  return value;
}

}  // namespace

class AuthController : public drogon::HttpController<AuthController> {
 public:
  AuthController() : auth_service_(user::GetMySqlUserGroupRepository()) {}

  METHOD_LIST_BEGIN
  ADD_METHOD_TO(AuthController::GetUserApiKey, "/api/v1/admin/users/{1}/api-key", drogon::Get);
  ADD_METHOD_TO(AuthController::IssueUserApiKey, "/api/v1/admin/users/{1}/api-key", drogon::Post);
  ADD_METHOD_TO(AuthController::RevokeUserApiKey, "/api/v1/admin/users/{1}/api-key", drogon::Delete);
  ADD_METHOD_TO(AuthController::GetAuthenticatedUser, "/api/v1/auth/me", drogon::Get);
  METHOD_LIST_END

  void GetUserApiKey(
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
      const auto api_key = auth_service_.GetUserApiKey(user_id);
      body["data"] = api_key.has_value() ? ToJson(*api_key) : Json::Value(Json::nullValue);
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(BuildErrorResponse(request, error.what()));
    }
  }

  void IssueUserApiKey(
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
      body["data"] = ToJson(auth_service_.IssueUserApiKey(user_id));

      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      response->setStatusCode(drogon::k201Created);
      callback(response);
    } catch (const std::runtime_error& error) {
      callback(BuildErrorResponse(request, error.what()));
    }
  }

  void RevokeUserApiKey(
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
      auth_service_.RevokeUserApiKey(user_id);

      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"]["revoked"] = true;
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(BuildErrorResponse(request, error.what()));
    }
  }

  void GetAuthenticatedUser(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
      static_cast<void>(BuildRequestContext(request, true));

      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(auth_service_.AuthenticateApiKey(ReadApiKeyFromRequest(request)));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(BuildErrorResponse(request, error.what()));
    }
  }

 private:
  drogon::HttpResponsePtr BuildErrorResponse(
      const drogon::HttpRequestPtr& request,
      const std::string& error_code) const {
    Json::Value error(Json::objectValue);
    error["meta"] = BuildMeta(request->getHeader("x-request-id"));

    if (error_code == "user_not_found") {
      error["error"]["code"] = error_code;
      error["error"]["message"] = "requested user was not found";
      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k404NotFound);
      return response;
    }

    if (error_code == "api_key_required" || error_code == "invalid_api_key") {
      error["error"]["code"] = error_code;
      error["error"]["message"] = error_code;
      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k401Unauthorized);
      return response;
    }

    error["error"]["code"] = error_code;
    error["error"]["message"] = error_code;
    auto response = drogon::HttpResponse::newHttpJsonResponse(error);
    response->setStatusCode(drogon::k400BadRequest);
    return response;
  }

  auth::AuthService auth_service_;
};

}  // namespace picaresque::http
