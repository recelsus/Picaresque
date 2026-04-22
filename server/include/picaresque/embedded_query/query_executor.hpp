#pragma once

#include "picaresque/embedded_query/embedded_query_types.hpp"
#include "picaresque/embedded_query/query_validator.hpp"
#include "picaresque/table/table_repository.hpp"

namespace picaresque::embedded_query {

class QueryExecutor {
 public:
  explicit QueryExecutor(table::TableRepository& table_repository);

  EmbeddedQueryExecutionResult Execute(const StoredEmbeddedQuery& query) const;

 private:
  table::TableRepository& table_repository_;
  QueryValidationConfig config_;
};

}  // namespace picaresque::embedded_query
