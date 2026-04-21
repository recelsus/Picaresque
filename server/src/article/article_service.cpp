#include "picaresque/article/article_service.hpp"

#include <stdexcept>

#include "picaresque/permission/api.hpp"

namespace picaresque::article {
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

permission::ArticleResource BuildArticleResource(const ArticleSummary& article) {
  return {
      .article_id = article.article_id,
      .required_permissions = EffectiveRequiredPermissions(article.required_permissions),
  };
}

}  // namespace

ArticleService::ArticleService(
    ArticleRepository& article_repository,
    user::UserGroupRepository& user_repository)
    : article_repository_(article_repository), user_repository_(user_repository) {}

std::vector<ArticleSummary> ArticleService::ListArticles(const std::string& actor_user_id) const {
  const auto actor = BuildPermissionUser(actor_user_id);
  std::vector<ArticleSummary> readable_articles;
  for (const auto& article : article_repository_.ListArticles()) {
    if (CanRead(actor, article)) {
      readable_articles.push_back(article);
    }
  }
  return readable_articles;
}

ArticleDetails ArticleService::GetArticle(
    const std::string& actor_user_id,
    const std::string& article_id) const {
  const auto article = article_repository_.FindArticleById(article_id);
  if (!article.has_value()) {
    throw std::runtime_error("article_not_found");
  }

  if (!CanRead(BuildPermissionUser(actor_user_id), article->summary)) {
    throw std::runtime_error("forbidden");
  }
  return *article;
}

ArticleDetails ArticleService::CreateArticle(const CreateArticleCommand& command) const {
  ValidateArticleInput(command.title, command.required_permissions);
  if (command.is_locked && !command.locked_by_user_id.has_value()) {
    throw std::runtime_error("locked_by_user_id_required");
  }

  const ArticleSummary proposed_article{
      .article_id = "",
      .title = command.title,
      .created_by_user_id = command.actor_user_id,
      .updated_by_user_id = command.actor_user_id,
      .is_locked = command.is_locked,
      .locked_by_user_id = command.locked_by_user_id,
      .required_permissions = command.required_permissions,
  };
  if (!CanWrite(BuildPermissionUser(command.actor_user_id), proposed_article)) {
    throw std::runtime_error("forbidden");
  }
  return article_repository_.CreateArticle(command);
}

ArticleDetails ArticleService::UpdateArticle(const UpdateArticleCommand& command) const {
  ValidateArticleInput(command.title, command.required_permissions);
  if (command.is_locked && !command.locked_by_user_id.has_value()) {
    throw std::runtime_error("locked_by_user_id_required");
  }

  const auto current = article_repository_.FindArticleById(command.article_id);
  if (!current.has_value()) {
    throw std::runtime_error("article_not_found");
  }

  const auto actor = BuildPermissionUser(command.actor_user_id);
  if (!CanWrite(actor, current->summary)) {
    throw std::runtime_error("forbidden");
  }

  const ArticleSummary proposed_article{
      .article_id = command.article_id,
      .title = command.title,
      .created_by_user_id = current->summary.created_by_user_id,
      .updated_by_user_id = command.actor_user_id,
      .is_locked = command.is_locked,
      .locked_by_user_id = command.locked_by_user_id,
      .required_permissions = command.required_permissions,
  };
  if (!CanWrite(actor, proposed_article)) {
    throw std::runtime_error("forbidden");
  }
  return article_repository_.UpdateArticle(command);
}

void ArticleService::DeleteArticle(const DeleteArticleCommand& command) const {
  const auto article = article_repository_.FindArticleById(command.article_id);
  if (!article.has_value()) {
    throw std::runtime_error("article_not_found");
  }
  if (article->summary.is_locked) {
    throw std::runtime_error("article_locked");
  }
  if (!CanWrite(BuildPermissionUser(command.actor_user_id), article->summary)) {
    throw std::runtime_error("forbidden");
  }
  article_repository_.DeleteArticle(command.article_id);
}

permission::User ArticleService::BuildPermissionUser(const std::string& actor_user_id) const {
  const auto actor = user_repository_.FindUserDetailsById(actor_user_id);
  if (!actor.has_value()) {
    throw std::runtime_error("user_not_found");
  }
  return {
      .user_id = actor->summary.user_id,
      .user_name = actor->summary.user_name,
      .role = actor->summary.role,
      .owned_groups = actor->owned_groups,
      .scoped_permissions = actor->scoped_permissions,
  };
}

void ArticleService::ValidateArticleInput(
    const std::string& title,
    const std::vector<permission::AccessRequirement>& required_permissions) const {
  if (title.empty()) {
    throw std::runtime_error("article_title_required");
  }
  for (const auto& requirement : required_permissions) {
    permission::ValidateAccessRequirement(requirement);
  }
}

bool ArticleService::CanRead(const permission::User& actor, const ArticleSummary& article) const {
  return permission::CanReadArticle(actor, BuildArticleResource(article));
}

bool ArticleService::CanWrite(const permission::User& actor, const ArticleSummary& article) const {
  return permission::CanWriteArticle(actor, BuildArticleResource(article));
}

}  // namespace picaresque::article
