#include <cassert>
#include <stdexcept>
#include <string>

#include "picaresque/article/article_service.hpp"
#include "picaresque/article/in_memory_article_repository.hpp"
#include "picaresque/group/group_management_service.hpp"
#include "picaresque/permission/api.hpp"
#include "picaresque/user/in_memory_user_group_repository.hpp"
#include "picaresque/user/user_management_service.hpp"

namespace article = picaresque::article;
namespace group = picaresque::group;
namespace permission = picaresque::permission;
namespace user = picaresque::user;

int main() {
  user::InMemoryUserGroupRepository user_repository;
  article::InMemoryArticleRepository article_repository;
  const user::UserManagementService user_service(user_repository);
  const group::GroupManagementService group_service(user_repository);
  const article::ArticleService article_service(article_repository, user_repository);

  const auto admin = user_service.CreateInitialAdmin({
      .login_id = "admin",
      .user_name = "Initial Admin",
      .email = "admin@example.local",
      .password = "change-me",
      .role = permission::Role::Admin,
  });
  const auto owner = user_service.CreateUser({
      .login_id = "owner",
      .user_name = "Owner",
      .email = "owner@example.local",
      .password = "change-me",
      .role = permission::Role::Owner,
  });
  const auto member = user_service.CreateUser({
      .login_id = "member",
      .user_name = "Member",
      .email = "member@example.local",
      .password = "change-me",
      .role = permission::Role::Member,
  });

  const auto group_details = group_service.CreateGroup({
      .actor_user_id = owner.summary.user_id,
      .group_name = "article-group",
      .description = std::nullopt,
  });
  const auto invitation = group_service.InviteUser({
      .actor_user_id = owner.summary.user_id,
      .group_id = group_details.summary.group_id,
      .invited_user_id = member.summary.user_id,
  });
  group_service.AcceptInvitation({
      .actor_user_id = member.summary.user_id,
      .invitation_id = invitation.invitation_id,
  });
  group_service.AssignScopedPermission({
      .actor_user_id = owner.summary.user_id,
      .target_user_id = member.summary.user_id,
      .group_id = group_details.summary.group_id,
      .scoped_permission =
          {
              .group_id = group_details.summary.group_id,
              .read = 30,
              .write = 30,
          },
  });

  const auto default_article = article_service.CreateArticle({
      .actor_user_id = admin.summary.user_id,
      .title = "Default Article",
      .body = "Default body",
      .required_permissions = {},
  });
  assert(article_service.GetArticle(member.summary.user_id, default_article.summary.article_id)
             .summary.title == "Default Article");

  const auto restricted_article = article_service.CreateArticle({
      .actor_user_id = owner.summary.user_id,
      .title = "Restricted Article",
      .body = "Restricted body",
      .required_permissions =
          {
              {
                  .group_id = group_details.summary.group_id,
                  .read = 30,
                  .write = 30,
              },
          },
  });
  assert(article_service.GetArticle(member.summary.user_id, restricted_article.summary.article_id)
             .body == "Restricted body");

  const auto updated_article = article_service.UpdateArticle({
      .actor_user_id = member.summary.user_id,
      .article_id = restricted_article.summary.article_id,
      .title = "Member Updated",
      .body = "Updated body",
      .required_permissions =
          {
              {
                  .group_id = group_details.summary.group_id,
                  .read = 30,
                  .write = 30,
              },
          },
  });
  assert(updated_article.summary.updated_by_user_id == member.summary.user_id);

  const auto locked_article = article_service.CreateArticle({
      .actor_user_id = admin.summary.user_id,
      .title = "Locked Article",
      .body = "Locked body",
      .is_locked = true,
      .locked_by_user_id = admin.summary.user_id,
      .required_permissions = {},
  });
  bool locked_delete_rejected = false;
  try {
    article_service.DeleteArticle({
        .actor_user_id = admin.summary.user_id,
        .article_id = locked_article.summary.article_id,
    });
  } catch (const std::runtime_error& error) {
    locked_delete_rejected = std::string(error.what()) == "article_locked";
  }
  assert(locked_delete_rejected);

  article_service.DeleteArticle({
      .actor_user_id = member.summary.user_id,
      .article_id = restricted_article.summary.article_id,
  });
  bool deleted_article_missing = false;
  try {
    static_cast<void>(article_service.GetArticle(admin.summary.user_id, restricted_article.summary.article_id));
  } catch (const std::runtime_error& error) {
    deleted_article_missing = std::string(error.what()) == "article_not_found";
  }
  assert(deleted_article_missing);

  return 0;
}
