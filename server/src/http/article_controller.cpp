#include <drogon/HttpController.h>
#include <json/json.h>

#include "picaresque/article/article_service.hpp"
#include "picaresque/http/json_serialization.hpp"
#include "picaresque/http/request_context.hpp"
#include "picaresque/permission/errors.hpp"
#include "../article/mysql_article_repository.hpp"
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

std::vector<permission::AccessRequirement> ParseRequiredPermissions(const Json::Value& json) {
  std::vector<permission::AccessRequirement> requirements;
  if (!json.isMember("required_permissions")) {
    return requirements;
  }

  const auto& values = json["required_permissions"];
  if (!values.isArray()) {
    throw std::runtime_error("invalid_required_permissions");
  }
  for (const auto& value : values) {
    requirements.push_back({
        .group_id = value.get("group_id", "").asString(),
        .read = static_cast<std::uint8_t>(value.get("read", 0).asUInt()),
        .write = static_cast<std::uint8_t>(value.get("write", 0).asUInt()),
    });
  }
  return requirements;
}

}  // namespace

class ArticleController : public drogon::HttpController<ArticleController> {
 public:
  ArticleController()
      : service_(article::GetMySqlArticleRepository(), user::GetMySqlUserGroupRepository()) {}

  METHOD_LIST_BEGIN
  ADD_METHOD_TO(ArticleController::CreateArticle, "/api/v1/articles", drogon::Post);
  ADD_METHOD_TO(ArticleController::ListArticles, "/api/v1/articles", drogon::Get);
  ADD_METHOD_TO(ArticleController::GetArticle, "/api/v1/articles/{1}", drogon::Get);
  ADD_METHOD_TO(ArticleController::UpdateArticle, "/api/v1/articles/{1}", drogon::Put);
  ADD_METHOD_TO(ArticleController::DeleteArticle, "/api/v1/articles/{1}", drogon::Delete);
  METHOD_LIST_END

  void CreateArticle(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }

    const auto json = request->getJsonObject();
    if (!json) {
      callback(BuildErrorResponse(request, drogon::k400BadRequest, "invalid_json", "request body must be valid json"));
      return;
    }

    try {
      const bool is_locked = (*json).get("is_locked", false).asBool();
      article::CreateArticleCommand command{
          .actor_user_id = context.authenticated_user->summary.user_id,
          .title = (*json).get("title", "").asString(),
          .body = (*json).get("body", "").asString(),
          .is_locked = is_locked,
          .locked_by_user_id = is_locked
              ? std::optional<std::string>(context.authenticated_user->summary.user_id)
              : std::nullopt,
          .required_permissions = ParseRequiredPermissions(*json),
      };

      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.CreateArticle(command));
      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      response->setStatusCode(drogon::k201Created);
      callback(response);
    } catch (const permission::ValidationError& error) {
      callback(BuildErrorResponse(request, drogon::k400BadRequest, "invalid_permission", error.what()));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void ListArticles(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }

    Json::Value body(Json::objectValue);
    body["meta"] = BuildMeta(request->getHeader("x-request-id"));
    Json::Value data(Json::arrayValue);
    for (const auto& article : service_.ListArticles(context.authenticated_user->summary.user_id)) {
      data.append(ToJson(article));
    }
    body["data"] = data;
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
  }

  void GetArticle(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& article_id) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }

    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.GetArticle(context.authenticated_user->summary.user_id, article_id));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void UpdateArticle(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& article_id) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }

    const auto json = request->getJsonObject();
    if (!json) {
      callback(BuildErrorResponse(request, drogon::k400BadRequest, "invalid_json", "request body must be valid json"));
      return;
    }

    try {
      const bool is_locked = (*json).get("is_locked", false).asBool();
      article::UpdateArticleCommand command{
          .actor_user_id = context.authenticated_user->summary.user_id,
          .article_id = article_id,
          .title = (*json).get("title", "").asString(),
          .body = (*json).get("body", "").asString(),
          .is_locked = is_locked,
          .locked_by_user_id = is_locked
              ? std::optional<std::string>(context.authenticated_user->summary.user_id)
              : std::nullopt,
          .required_permissions = ParseRequiredPermissions(*json),
      };

      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.UpdateArticle(command));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const permission::ValidationError& error) {
      callback(BuildErrorResponse(request, drogon::k400BadRequest, "invalid_permission", error.what()));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void DeleteArticle(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& article_id) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }

    try {
      service_.DeleteArticle({
          .actor_user_id = context.authenticated_user->summary.user_id,
          .article_id = article_id,
      });
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"]["deleted"] = true;
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

 private:
  bool BuildContext(
      const drogon::HttpRequestPtr& request,
      const std::function<void(const drogon::HttpResponsePtr&)>& callback,
      RequestContext& context) const {
    try {
      context = BuildRequestContext(request, true);
      return true;
    } catch (const std::runtime_error& error) {
      callback(BuildRequestContextErrorResponse(request, error.what()));
      return false;
    }
  }

  drogon::HttpResponsePtr MapError(
      const drogon::HttpRequestPtr& request,
      const std::string& error_code) const {
    if (error_code == "forbidden") {
      return BuildErrorResponse(request, drogon::k403Forbidden, error_code, "operation is forbidden");
    }
    if (error_code == "article_not_found") {
      return BuildErrorResponse(request, drogon::k404NotFound, error_code, error_code);
    }
    if (error_code == "article_locked") {
      return BuildErrorResponse(request, drogon::k409Conflict, error_code, error_code);
    }
    return BuildErrorResponse(request, drogon::k400BadRequest, error_code, error_code);
  }

  article::ArticleService service_;
};

}  // namespace picaresque::http
