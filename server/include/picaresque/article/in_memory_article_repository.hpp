#pragma once

#include <mutex>
#include <vector>

#include "picaresque/article/article_repository.hpp"

namespace picaresque::article {

class InMemoryArticleRepository final : public ArticleRepository {
 public:
  std::vector<ArticleSummary> ListArticles() const override;
  std::optional<ArticleDetails> FindArticleById(const std::string& article_id) const override;
  ArticleDetails CreateArticle(const CreateArticleCommand& command) override;
  ArticleDetails UpdateArticle(const UpdateArticleCommand& command) override;
  void DeleteArticle(const std::string& article_id) override;

 private:
  std::string BuildNextArticleId();

  mutable std::mutex mutex_;
  std::vector<ArticleDetails> articles_;
  std::size_t next_article_id_ = 1;
};

}  // namespace picaresque::article
