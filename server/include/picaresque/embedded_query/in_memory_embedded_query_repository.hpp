#pragma once

#include <mutex>
#include <vector>

#include "picaresque/embedded_query/embedded_query_repository.hpp"

namespace picaresque::embedded_query {

class InMemoryEmbeddedQueryRepository final : public EmbeddedQueryRepository {
 public:
  std::vector<StoredEmbeddedQuery> ListByArticleId(const std::string& article_id) const override;
  std::optional<StoredEmbeddedQuery> FindById(
      const std::string& article_id,
      const std::string& query_id) const override;
  void ReplaceArticleQueries(
      const std::string& article_id,
      const std::vector<StoredEmbeddedQuery>& queries) override;

 private:
  mutable std::mutex mutex_;
  std::vector<StoredEmbeddedQuery> queries_;
};

}  // namespace picaresque::embedded_query
