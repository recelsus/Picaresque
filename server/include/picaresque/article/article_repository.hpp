#pragma once

#include <optional>
#include <string>
#include <vector>

#include "picaresque/article/article_types.hpp"

namespace picaresque::article {

class ArticleRepository {
 public:
  virtual ~ArticleRepository() = default;

  virtual std::vector<ArticleSummary> ListArticles() const = 0;
  virtual std::optional<ArticleDetails> FindArticleById(const std::string& article_id) const = 0;
  virtual ArticleDetails CreateArticle(const CreateArticleCommand& command) = 0;
  virtual ArticleDetails UpdateArticle(const UpdateArticleCommand& command) = 0;
  virtual void DeleteArticle(const std::string& article_id) = 0;
};

}  // namespace picaresque::article
