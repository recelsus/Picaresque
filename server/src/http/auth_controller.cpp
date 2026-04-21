#include <drogon/HttpController.h>
#include <drogon/Cookie.h>
#include <json/json.h>

#include "picaresque/auth/auth_service.hpp"
#include "picaresque/http/json_serialization.hpp"
#include "picaresque/http/request_context.hpp"
#include "../user/mysql_user_group_repository.hpp"

namespace picaresque::http {
namespace {

constexpr const char* kWebSessionCookieName = "picaresque_session";

std::string ReadWebSessionFromRequest(const drogon::HttpRequestPtr& request) {
  const auto header_token = request->getHeader("x-web-session");
  if (!header_token.empty()) {
    return header_token;
  }
  return request->getCookie(kWebSessionCookieName);
}

Json::Value ToJsonApiKey(const auth::ApiKeyInfo& api_key_info) {
  Json::Value value(Json::objectValue);
  value["user_id"] = api_key_info.user_id;
  value["key_prefix"] = api_key_info.key_prefix;
  value["enabled"] = api_key_info.enabled;
  return value;
}

Json::Value ToJsonIssuedApiKey(const auth::IssuedApiKey& issued_api_key) {
  Json::Value value(Json::objectValue);
  value["user_id"] = issued_api_key.info.user_id;
  value["key_prefix"] = issued_api_key.info.key_prefix;
  value["enabled"] = issued_api_key.info.enabled;
  value["plain_api_key"] = issued_api_key.plain_api_key;
  return value;
}

Json::Value ToJsonIssuedWebSession(
    const auth::IssuedWebSession& issued_session,
    const user::UserDetails& user_details) {
  Json::Value value(Json::objectValue);
  value["session_id"] = issued_session.info.session_id;
  value["user"] = picaresque::http::ToJson(user_details);
  return value;
}

bool IsAdmin(const RequestContext& context) {
  return context.authenticated_user.has_value() &&
      context.authenticated_user->summary.role == permission::Role::Admin;
}

}  // namespace

class AuthController : public drogon::HttpController<AuthController> {
 public:
  AuthController() : auth_service_(user::GetMySqlUserGroupRepository()) {}

  METHOD_LIST_BEGIN
  ADD_METHOD_TO(AuthController::Login, "/api/v1/auth/login", drogon::Post);
  ADD_METHOD_TO(AuthController::Logout, "/api/v1/auth/logout", drogon::Post);
  ADD_METHOD_TO(AuthController::GetUserApiKey, "/api/v1/users/{1}/api-key", drogon::Get);
  ADD_METHOD_TO(AuthController::IssueUserApiKey, "/api/v1/users/{1}/api-key", drogon::Post);
  ADD_METHOD_TO(AuthController::RevokeUserApiKey, "/api/v1/users/{1}/api-key", drogon::Delete);
  ADD_METHOD_TO(AuthController::GetUserApiKey, "/api/v1/admin/users/{1}/api-key", drogon::Get);
  ADD_METHOD_TO(AuthController::IssueUserApiKey, "/api/v1/admin/users/{1}/api-key", drogon::Post);
  ADD_METHOD_TO(AuthController::RevokeUserApiKey, "/api/v1/admin/users/{1}/api-key", drogon::Delete);
  ADD_METHOD_TO(AuthController::GetAuthenticatedUser, "/api/v1/auth/me", drogon::Get);
  METHOD_LIST_END

  void Login(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
      static_cast<void>(BuildRequestContext(request, false));
    } catch (const std::runtime_error& error) {
      const std::string code = error.what();
      if (code.rfind("access_denied:", 0) == 0) {
        callback(BuildRequestContextErrorResponse(request, code));
        return;
      }
    }

    const auto json = request->getJsonObject();
    if (!json) {
      callback(BuildErrorResponse(request, "invalid_json"));
      return;
    }

    try {
      const auto issued_session = auth_service_.LoginWithPassword(
          (*json).get("login_id", "").asString(),
          (*json).get("password", "").asString());
      const auto user_details = auth_service_.AuthenticateWebSession(issued_session.plain_session_token);

      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJsonIssuedWebSession(issued_session, user_details);

      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      drogon::Cookie cookie(kWebSessionCookieName, issued_session.plain_session_token);
      cookie.setHttpOnly(true);
      cookie.setPath("/");
      cookie.setSameSite(drogon::Cookie::SameSite::kLax);
      cookie.setMaxAge(60 * 60 * 24 * 7);
      response->addCookie(std::move(cookie));
      callback(response);
    } catch (const std::runtime_error& error) {
      callback(BuildErrorResponse(request, error.what()));
    }
  }

  void Logout(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
      auth_service_.LogoutWebSession(ReadWebSessionFromRequest(request));

      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"]["logged_out"] = true;
      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      drogon::Cookie cookie(kWebSessionCookieName, "");
      cookie.setHttpOnly(true);
      cookie.setPath("/");
      cookie.setMaxAge(0);
      response->addCookie(std::move(cookie));
      callback(response);
    } catch (const std::runtime_error& error) {
      callback(BuildErrorResponse(request, error.what()));
    }
  }

  void GetUserApiKey(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& user_id) const {
    RequestContext context;
    try {
      context = BuildRequestContext(request, true);
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }
    if (!IsAdmin(context)) {
      callback(BuildErrorResponse(request, "forbidden"));
      return;
    }

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      const auto api_key = auth_service_.GetUserApiKey(user_id);
      body["data"] = api_key.has_value() ? ToJsonApiKey(*api_key) : Json::Value(Json::nullValue);
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(BuildErrorResponse(request, error.what()));
    }
  }

  void IssueUserApiKey(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& user_id) const {
    RequestContext context;
    try {
      context = BuildRequestContext(request, true);
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }
    if (!IsAdmin(context)) {
      callback(BuildErrorResponse(request, "forbidden"));
      return;
    }

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJsonIssuedApiKey(auth_service_.IssueUserApiKey(user_id));

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
    RequestContext context;
    try {
      context = BuildRequestContext(request, true);
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return;
    }
    if (!IsAdmin(context)) {
      callback(BuildErrorResponse(request, "forbidden"));
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
      const auto context = BuildRequestContext(request, true);

      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(*context.authenticated_user);
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

    if (error_code == "invalid_json") {
      error["error"]["code"] = error_code;
      error["error"]["message"] = "request body must be valid json";
      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k400BadRequest);
      return response;
    }

    if (error_code == "invalid_credentials") {
      error["error"]["code"] = error_code;
      error["error"]["message"] = error_code;
      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k401Unauthorized);
      return response;
    }

    if (error_code == "auth_required" || error_code == "api_key_required" || error_code == "invalid_api_key" ||
        error_code == "session_required" || error_code == "invalid_session") {
      error["error"]["code"] = error_code;
      error["error"]["message"] = error_code;
      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k401Unauthorized);
      return response;
    }

    if (error_code == "forbidden") {
      error["error"]["code"] = error_code;
      error["error"]["message"] = "operation is forbidden";
      auto response = drogon::HttpResponse::newHttpJsonResponse(error);
      response->setStatusCode(drogon::k403Forbidden);
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
