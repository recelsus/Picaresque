#pragma once

#include <optional>
#include <string>
#include <vector>

#include "picaresque/permission/types.hpp"

namespace picaresque::article {

struct ArticleSummary {
  std::string article_id;
  std::string title;
  std::string created_by_user_id;
  std::string updated_by_user_id;
  bool is_locked = false;
  std::optional<std::string> locked_by_user_id;
  std::optional<std::string> locked_at;
  std::vector<permission::AccessRequirement> required_permissions;
};

struct ArticleDetails {
  ArticleSummary summary;
  std::string body;
};

struct CreateArticleCommand {
  std::string actor_user_id;
  std::string title;
  std::string body;
  bool is_locked = false;
  std::optional<std::string> locked_by_user_id;
  std::vector<permission::AccessRequirement> required_permissions;
};

struct UpdateArticleCommand {
  std::string actor_user_id;
  std::string article_id;
  std::string title;
  std::string body;
  bool is_locked = false;
  std::optional<std::string> locked_by_user_id;
  std::vector<permission::AccessRequirement> required_permissions;
};

struct DeleteArticleCommand {
  std::string actor_user_id;
  std::string article_id;
};

}  // namespace picaresque::article
