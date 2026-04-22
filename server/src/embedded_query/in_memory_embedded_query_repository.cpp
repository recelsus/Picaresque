#include "picaresque/embedded_query/in_memory_embedded_query_repository.hpp"

#include <algorithm>

namespace picaresque::embedded_query {

std::vector<StoredEmbeddedQuery> InMemoryEmbeddedQueryRepository::ListByArticleId(
    const std::string& article_id) const {
  std::scoped_lock lock(mutex_);
  std::vector<StoredEmbeddedQuery> result;
  for (const auto& query : queries_) {
    if (query.article_id == article_id) {
      result.push_back(query);
    }
  }
  return result;
}

std::optional<StoredEmbeddedQuery> InMemoryEmbeddedQueryRepository::FindById(
    const std::string& article_id,
    const std::string& query_id) const {
  std::scoped_lock lock(mutex_);
  const auto it = std::find_if(
      queries_.begin(),
      queries_.end(),
      [&](const StoredEmbeddedQuery& query) {
        return query.article_id == article_id && query.query_id == query_id;
      });
  if (it == queries_.end()) {
    return std::nullopt;
  }
  return *it;
}

void InMemoryEmbeddedQueryRepository::ReplaceArticleQueries(
    const std::string& article_id,
    const std::vector<StoredEmbeddedQuery>& queries) {
  std::scoped_lock lock(mutex_);
  queries_.erase(
      std::remove_if(
          queries_.begin(),
          queries_.end(),
          [&](const StoredEmbeddedQuery& query) {
            return query.article_id == article_id;
          }),
      queries_.end());
  for (auto query : queries) {
    query.article_id = article_id;
    queries_.push_back(query);
  }
}

}  // namespace picaresque::embedded_query
