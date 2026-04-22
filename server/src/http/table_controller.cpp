#include <drogon/HttpController.h>
#include <json/json.h>

#include "picaresque/http/json_serialization.hpp"
#include "picaresque/http/request_context.hpp"
#include "picaresque/permission/errors.hpp"
#include "picaresque/table/table_service.hpp"
#include "../table/mysql_table_repository.hpp"
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

table::ColumnDefinition ParseColumn(const Json::Value& json) {
  return {
      .column_id = json.get("column_id", "").asString(),
      .column_name = json.get("column_name", "").asString(),
      .column_type = table::ColumnTypeFromString(json.get("column_type", "varchar").asString()),
      .is_required = json.get("is_required", false).asBool(),
  };
}

std::vector<table::ColumnDefinition> ParseColumns(const Json::Value& json) {
  std::vector<table::ColumnDefinition> columns;
  if (!json.isMember("columns")) {
    return columns;
  }
  const auto& values = json["columns"];
  if (!values.isArray()) {
    throw std::runtime_error("invalid_columns");
  }
  for (const auto& value : values) {
    columns.push_back(ParseColumn(value));
  }
  return columns;
}

Json::Value ParseValues(const Json::Value& json) {
  if (!json.isMember("values") || !json["values"].isObject()) {
    throw std::runtime_error("row_values_must_be_object");
  }
  return json["values"];
}

}  // namespace

class TableController : public drogon::HttpController<TableController> {
 public:
  TableController()
      : service_(table::GetMySqlTableRepository(), user::GetMySqlUserGroupRepository()) {}

  METHOD_LIST_BEGIN
  ADD_METHOD_TO(TableController::CreateTable, "/api/v1/tables", drogon::Post);
  ADD_METHOD_TO(TableController::ListTables, "/api/v1/tables", drogon::Get);
  ADD_METHOD_TO(TableController::GetTable, "/api/v1/tables/{1}", drogon::Get);
  ADD_METHOD_TO(TableController::UpdateTable, "/api/v1/tables/{1}", drogon::Put);
  ADD_METHOD_TO(TableController::DeleteTable, "/api/v1/tables/{1}", drogon::Delete);
  ADD_METHOD_TO(TableController::AddColumn, "/api/v1/tables/{1}/columns", drogon::Post);
  ADD_METHOD_TO(TableController::ListRows, "/api/v1/tables/{1}/rows", drogon::Get);
  ADD_METHOD_TO(TableController::CreateRow, "/api/v1/tables/{1}/rows", drogon::Post);
  ADD_METHOD_TO(TableController::GetRow, "/api/v1/tables/{1}/rows/{2}", drogon::Get);
  ADD_METHOD_TO(TableController::UpdateRow, "/api/v1/tables/{1}/rows/{2}", drogon::Put);
  ADD_METHOD_TO(TableController::DeleteRow, "/api/v1/tables/{1}/rows/{2}", drogon::Delete);
  METHOD_LIST_END

  void CreateTable(
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
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.CreateTable({
          .actor_user_id = context.authenticated_user->summary.user_id,
          .table_name = (*json).get("table_name", "").asString(),
          .columns = ParseColumns(*json),
          .required_permissions = ParseRequiredPermissions(*json),
      }));
      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      response->setStatusCode(drogon::k201Created);
      callback(response);
    } catch (const permission::ValidationError& error) {
      callback(BuildErrorResponse(request, drogon::k400BadRequest, "invalid_permission", error.what()));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void ListTables(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }

    Json::Value body(Json::objectValue);
    body["meta"] = BuildMeta(request->getHeader("x-request-id"));
    Json::Value data(Json::arrayValue);
    for (const auto& table : service_.ListTables(context.authenticated_user->summary.user_id)) {
      data.append(ToJson(table));
    }
    body["data"] = data;
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
  }

  void GetTable(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& table_id) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }
    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.GetTable(context.authenticated_user->summary.user_id, table_id));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void UpdateTable(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& table_id) const {
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
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.UpdateTable({
          .actor_user_id = context.authenticated_user->summary.user_id,
          .table_id = table_id,
          .table_name = (*json).get("table_name", "").asString(),
          .required_permissions = ParseRequiredPermissions(*json),
      }));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const permission::ValidationError& error) {
      callback(BuildErrorResponse(request, drogon::k400BadRequest, "invalid_permission", error.what()));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void DeleteTable(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& table_id) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }
    try {
      service_.DeleteTable({
          .actor_user_id = context.authenticated_user->summary.user_id,
          .table_id = table_id,
      });
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"]["deleted"] = true;
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void AddColumn(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& table_id) const {
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
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.AddColumn({
          .actor_user_id = context.authenticated_user->summary.user_id,
          .table_id = table_id,
          .column = ParseColumn(*json),
      }));
      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      response->setStatusCode(drogon::k201Created);
      callback(response);
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void ListRows(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& table_id) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }
    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      Json::Value data(Json::arrayValue);
      for (const auto& row : service_.ListRows(context.authenticated_user->summary.user_id, table_id)) {
        data.append(ToJson(row));
      }
      body["data"] = data;
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void CreateRow(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& table_id) const {
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
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.CreateRow({
          .actor_user_id = context.authenticated_user->summary.user_id,
          .table_id = table_id,
          .values = ParseValues(*json),
      }));
      auto response = drogon::HttpResponse::newHttpJsonResponse(body);
      response->setStatusCode(drogon::k201Created);
      callback(response);
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void GetRow(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& table_id,
      const std::string& row_id) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }
    try {
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.GetRow(context.authenticated_user->summary.user_id, table_id, row_id));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void UpdateRow(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& table_id,
      const std::string& row_id) const {
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
      Json::Value body(Json::objectValue);
      body["meta"] = BuildMeta(request->getHeader("x-request-id"));
      body["data"] = ToJson(service_.UpdateRow({
          .actor_user_id = context.authenticated_user->summary.user_id,
          .table_id = table_id,
          .row_id = row_id,
          .values = ParseValues(*json),
      }));
      callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::runtime_error& error) {
      callback(MapError(request, error.what()));
    }
  }

  void DeleteRow(
      const drogon::HttpRequestPtr& request,
      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
      const std::string& table_id,
      const std::string& row_id) const {
    RequestContext context;
    if (!BuildContext(request, callback, context)) {
      return;
    }
    try {
      service_.DeleteRow({
          .actor_user_id = context.authenticated_user->summary.user_id,
          .table_id = table_id,
          .row_id = row_id,
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
    if (error_code == "table_not_found" || error_code == "row_not_found") {
      return BuildErrorResponse(request, drogon::k404NotFound, error_code, error_code);
    }
    return BuildErrorResponse(request, drogon::k400BadRequest, error_code, error_code);
  }

  table::TableService service_;
};

}  // namespace picaresque::http
