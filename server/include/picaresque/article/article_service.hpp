#pragma once

#include <vector>

#include "picaresque/article/article_repository.hpp"
#include "picaresque/user/user_group_repository.hpp"

namespace picaresque::article {

class ArticleService {
 public:
  ArticleService(ArticleRepository& article_repository, user::UserGroupRepository& user_repository);

  std::vector<ArticleSummary> ListArticles(const std::string& actor_user_id) const;
  ArticleDetails GetArticle(const std::string& actor_user_id, const std::string& article_id) const;
  ArticleDetails CreateArticle(const CreateArticleCommand& command) const;
  ArticleDetails UpdateArticle(const UpdateArticleCommand& command) const;
  void DeleteArticle(const DeleteArticleCommand& command) const;

 private:
  permission::User BuildPermissionUser(const std::string& actor_user_id) const;
  void ValidateArticleInput(
      const std::string& title,
      const std::vector<permission::AccessRequirement>& required_permissions) const;
  bool CanRead(const permission::User& actor, const ArticleSummary& article) const;
  bool CanWrite(const permission::User& actor, const ArticleSummary& article) const;

  ArticleRepository& article_repository_;
  user::UserGroupRepository& user_repository_;
};

}  // namespace picaresque::article
