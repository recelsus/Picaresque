#include "picaresque/article/in_memory_article_repository.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace picaresque::article {

std::vector<ArticleSummary> InMemoryArticleRepository::ListArticles() const {
  std::scoped_lock lock(mutex_);
  std::vector<ArticleSummary> articles;
  articles.reserve(articles_.size());
  for (const auto& article : articles_) {
    articles.push_back(article.summary);
  }
  return articles;
}

std::optional<ArticleDetails> InMemoryArticleRepository::FindArticleById(
    const std::string& article_id) const {
  std::scoped_lock lock(mutex_);
  const auto it = std::find_if(
      articles_.begin(),
      articles_.end(),
      [&article_id](const ArticleDetails& article) {
        return article.summary.article_id == article_id;
      });
  if (it == articles_.end()) {
    return std::nullopt;
  }
  return *it;
}

ArticleDetails InMemoryArticleRepository::CreateArticle(const CreateArticleCommand& command) {
  std::scoped_lock lock(mutex_);
  ArticleDetails article{
      .summary =
          {
              .article_id = BuildNextArticleId(),
              .title = command.title,
              .created_by_user_id = command.actor_user_id,
              .updated_by_user_id = command.actor_user_id,
              .is_locked = command.is_locked,
              .locked_by_user_id = command.is_locked ? command.locked_by_user_id : std::nullopt,
              .locked_at = std::nullopt,
              .required_permissions = command.required_permissions,
          },
      .body = command.body,
  };
  articles_.push_back(article);
  return article;
}

ArticleDetails InMemoryArticleRepository::UpdateArticle(const UpdateArticleCommand& command) {
  std::scoped_lock lock(mutex_);
  auto it = std::find_if(
      articles_.begin(),
      articles_.end(),
      [&command](const ArticleDetails& article) {
        return article.summary.article_id == command.article_id;
      });
  if (it == articles_.end()) {
    throw std::runtime_error("article_not_found");
  }

  it->summary.title = command.title;
  it->summary.updated_by_user_id = command.actor_user_id;
  it->summary.is_locked = command.is_locked;
  it->summary.locked_by_user_id = command.is_locked ? command.locked_by_user_id : std::nullopt;
  it->summary.required_permissions = command.required_permissions;
  it->body = command.body;
  return *it;
}

void InMemoryArticleRepository::DeleteArticle(const std::string& article_id) {
  std::scoped_lock lock(mutex_);
  const auto before = articles_.size();
  articles_.erase(
      std::remove_if(
          articles_.begin(),
          articles_.end(),
          [&article_id](const ArticleDetails& article) {
            return article.summary.article_id == article_id;
          }),
      articles_.end());
  if (articles_.size() == before) {
    throw std::runtime_error("article_not_found");
  }
}

std::string InMemoryArticleRepository::BuildNextArticleId() {
  std::ostringstream stream;
  stream << "article_" << next_article_id_++;
  return stream.str();
}

}  // namespace picaresque::article
