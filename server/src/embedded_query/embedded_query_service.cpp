#include "picaresque/embedded_query/embedded_query_service.hpp"

#include <random>
#include <unordered_set>

#include "picaresque/embedded_query/query_parser.hpp"
#include "picaresque/permission/api.hpp"

namespace picaresque::embedded_query {
namespace {

std::vector<permission::AccessRequirement> EffectiveRequiredPermissions(
    const std::vector<permission::AccessRequirement>& required_permissions) {
  if (!required_permissions.empty()) {
    return required_permissions;
  }
  return {
      {
          .group_id = "*",
          .read = permission::kDefaultPermission,
          .write = permission::kDefaultPermission,
      },
  };
}

permission::TableResource BuildTableResource(const table::TableSummary& table) {
  return {
      .table_id = table.table_id,
      .required_permissions = EffectiveRequiredPermissions(table.required_permissions),
  };
}

}  // namespace

EmbeddedQueryService::EmbeddedQueryService(
    EmbeddedQueryRepository& query_repository,
    table::TableRepository& table_repository,
    user::UserGroupRepository& user_repository)
    : query_repository_(query_repository),
      table_repository_(table_repository),
      user_repository_(user_repository),
      query_executor_(table_repository) {}

ProcessedArticleQueries EmbeddedQueryService::PrepareForCreate(
    const std::string& actor_user_id,
    const std::string& body) const {
  return Process(actor_user_id, std::nullopt, body);
}

ProcessedArticleQueries EmbeddedQueryService::PrepareForUpdate(
    const std::string& actor_user_id,
    const std::string& article_id,
    const std::string& body) const {
  return Process(actor_user_id, article_id, body);
}

void EmbeddedQueryService::SaveArticleQueries(
    const std::string& article_id,
    const std::vector<StoredEmbeddedQuery>& queries) const {
  query_repository_.ReplaceArticleQueries(article_id, queries);
}

EmbeddedQueryExecutionResult EmbeddedQueryService::ExecuteSavedQuery(
    const std::string& article_id,
    const std::string& query_id) const {
  const auto query = query_repository_.FindById(article_id, query_id);
  if (!query.has_value()) {
    throw std::runtime_error("query_not_found");
  }
  return query_executor_.Execute(*query);
}

ProcessedArticleQueries EmbeddedQueryService::Process(
    const std::string& actor_user_id,
    const std::optional<std::string>& article_id,
    const std::string& body) const {
  const auto fragments = ExtractQueryFragments(body);
  std::vector<QueryValidationError> errors;
  std::vector<StoredEmbeddedQuery> queries;
  queries.reserve(fragments.size());
  std::unordered_set<std::string> seen_query_ids;

  for (const auto& fragment : fragments) {
    std::string message;
    const auto validation = ValidateSelectQuery(fragment.sql, config_, message);
    if (!validation.has_value()) {
      errors.push_back({fragment.fragment_kind, fragment.location_index, message});
      continue;
    }

    bool table_accessible = true;
    for (const auto& table_id : validation->table_ids) {
      const auto table = table_repository_.FindTableById(table_id);
      if (!table.has_value()) {
        errors.push_back({fragment.fragment_kind, fragment.location_index, "table not found"});
        table_accessible = false;
        break;
      }
      if (!CanWriteTable(actor_user_id, table->summary)) {
        errors.push_back({fragment.fragment_kind, fragment.location_index, "table write permission required"});
        table_accessible = false;
        break;
      }
    }
    if (!table_accessible) {
      continue;
    }

    std::string query_id = fragment.query_id.value_or("");
    if (!query_id.empty()) {
      if (!article_id.has_value() || !query_repository_.FindById(*article_id, query_id).has_value()) {
        errors.push_back({fragment.fragment_kind, fragment.location_index, "unknown query_id"});
        continue;
      }
    } else {
      query_id = GenerateQueryId();
    }
    if (!seen_query_ids.insert(query_id).second) {
      errors.push_back({fragment.fragment_kind, fragment.location_index, "duplicate query_id"});
      continue;
    }

    queries.push_back({
        .query_id = query_id,
        .article_id = article_id.value_or(""),
        .fragment_kind = fragment.fragment_kind,
        .location_index = fragment.location_index,
        .sql = fragment.sql,
        .table_id = validation->table_id,
    });
  }

  if (!errors.empty()) {
    throw QueryValidationException(std::move(errors));
  }

  return {
      .body = RewriteBodyWithQueryIds(body, fragments, queries),
      .queries = queries,
  };
}

std::string EmbeddedQueryService::GenerateQueryId() const {
  static std::mt19937_64 generator(std::random_device{}());
  static constexpr char kAlphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  std::string value = "q_";
  for (int i = 0; i < 8; ++i) {
    value.push_back(kAlphabet[generator() % 36]);
  }
  return value;
}

bool EmbeddedQueryService::CanWriteTable(
    const std::string& actor_user_id,
    const table::TableSummary& table) const {
  const auto actor = user_repository_.FindUserDetailsById(actor_user_id);
  if (!actor.has_value()) {
    throw std::runtime_error("user_not_found");
  }
  const permission::User permission_user{
      .user_id = actor->summary.user_id,
      .user_name = actor->summary.user_name,
      .role = actor->summary.role,
      .owned_groups = actor->owned_groups,
      .scoped_permissions = actor->scoped_permissions,
  };
  return permission::CanWriteTable(permission_user, BuildTableResource(table));
}

}  // namespace picaresque::embedded_query
