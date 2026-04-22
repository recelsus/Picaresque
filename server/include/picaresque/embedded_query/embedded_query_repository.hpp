#pragma once

#include <optional>
#include <string>
#include <vector>

#include "picaresque/embedded_query/embedded_query_types.hpp"

namespace picaresque::embedded_query {

class EmbeddedQueryRepository {
 public:
  virtual ~EmbeddedQueryRepository() = default;

  virtual std::vector<StoredEmbeddedQuery> ListByArticleId(const std::string& article_id) const = 0;
  virtual std::optional<StoredEmbeddedQuery> FindById(
      const std::string& article_id,
      const std::string& query_id) const = 0;
  virtual void ReplaceArticleQueries(
      const std::string& article_id,
      const std::vector<StoredEmbeddedQuery>& queries) = 0;
};

}  // namespace picaresque::embedded_query
