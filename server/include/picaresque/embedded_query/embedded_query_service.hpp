#pragma once

#include "picaresque/embedded_query/embedded_query_repository.hpp"
#include "picaresque/embedded_query/query_executor.hpp"
#include "picaresque/embedded_query/query_validator.hpp"
#include "picaresque/table/table_repository.hpp"
#include "picaresque/user/user_group_repository.hpp"

namespace picaresque::embedded_query {

class EmbeddedQueryService {
 public:
  EmbeddedQueryService(
      EmbeddedQueryRepository& query_repository,
      table::TableRepository& table_repository,
      user::UserGroupRepository& user_repository);

  ProcessedArticleQueries PrepareForCreate(const std::string& actor_user_id, const std::string& body) const;
  ProcessedArticleQueries PrepareForUpdate(
      const std::string& actor_user_id,
      const std::string& article_id,
      const std::string& body) const;
  void SaveArticleQueries(const std::string& article_id, const std::vector<StoredEmbeddedQuery>& queries) const;
  EmbeddedQueryExecutionResult ExecuteSavedQuery(
      const std::string& article_id,
      const std::string& query_id) const;

 private:
  ProcessedArticleQueries Process(
      const std::string& actor_user_id,
      const std::optional<std::string>& article_id,
      const std::string& body) const;
  std::string GenerateQueryId() const;
  bool CanWriteTable(const std::string& actor_user_id, const table::TableSummary& table) const;

  EmbeddedQueryRepository& query_repository_;
  table::TableRepository& table_repository_;
  user::UserGroupRepository& user_repository_;
  QueryExecutor query_executor_;
  QueryValidationConfig config_;
};

}  // namespace picaresque::embedded_query
