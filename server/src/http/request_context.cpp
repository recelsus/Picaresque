#include "picaresque/http/request_context.hpp"

#include <ctime>
#include <stdexcept>

#include <json/json.h>

#include "picaresque/access/policy.hpp"
#include "picaresque/auth/auth_service.hpp"
#include "picaresque/http/json_serialization.hpp"
#include "../access/mysql_access_repository.hpp"
#include "../user/mysql_user_group_repository.hpp"

namespace picaresque::http {
namespace {

std::string ResolveClientIp(const drogon::HttpRequestPtr& request) {
  const auto forwarded_for = request->getHeader("x-forwarded-for");
  if (!forwarded_for.empty()) {
    const auto comma = forwarded_for.find(',');
    return forwarded_for.substr(0, comma);
  }

  const auto real_ip = request->getHeader("x-real-ip");
  if (!real_ip.empty()) {
    return real_ip;
  }

  return request->peerAddr().toIp();
}

access::AccessSurface ResolveSurface(const drogon::HttpRequestPtr& request) {
  const auto path = request->path();
  if (path.rfind("/api/", 0) == 0) {
    return access::AccessSurface::RestApi;
  }
  return access::AccessSurface::Web;
}

}  // namespace

RequestContext BuildRequestContext(
    const drogon::HttpRequestPtr& request,
    bool require_api_key) {
  return BuildRequestContext(
      request,
      require_api_key,
      access::GetMySqlAccessRepository(),
      {
          .web_default_policy = access::DefaultPolicy::Allow,
          .rest_api_default_policy = access::DefaultPolicy::Allow,
      },
      user::GetMySqlUserGroupRepository());
}

RequestContext BuildRequestContext(
    const drogon::HttpRequestPtr& request,
    bool require_api_key,
    const access::AccessRepository& access_repository,
    access::AccessConfiguration access_configuration,
    user::UserGroupRepository& user_repository) {
  RequestContext context{
      .request_id = request->getHeader("x-request-id"),
      .client_ip = ResolveClientIp(request),
      .surface = ResolveSurface(request),
      .authenticated_user = std::nullopt,
  };

  access::AccessPolicyService access_policy(access_repository, access_configuration);
  const auto access_result = access_policy.Evaluate(
      {
          .client_ip = context.client_ip,
          .surface = context.surface,
      },
      std::time(nullptr));
  if (access_result.decision == access::AccessDecision::Deny) {
    throw std::runtime_error("access_denied:" + access_result.reason);
  }

  const auto api_key = request->getHeader("x-api-key");
  if (!api_key.empty()) {
    auth::AuthService auth_service(user_repository);
    context.authenticated_user = auth_service.AuthenticateApiKey(api_key);
    return context;
  }

  if (require_api_key) {
    throw std::runtime_error("api_key_required");
  }

  return context;
}

drogon::HttpResponsePtr BuildRequestContextErrorResponse(
    const drogon::HttpRequestPtr& request,
    const std::string& error_code) {
  Json::Value error(Json::objectValue);
  error["meta"] = BuildMeta(request->getHeader("x-request-id"));

  if (error_code.rfind("access_denied:", 0) == 0) {
    error["error"]["code"] = "access_denied";
    error["error"]["message"] = error_code.substr(std::string("access_denied:").size());
    auto response = drogon::HttpResponse::newHttpJsonResponse(error);
    response->setStatusCode(drogon::k403Forbidden);
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

}  // namespace picaresque::http
