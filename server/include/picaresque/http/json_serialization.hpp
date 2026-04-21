#pragma once

#include <string_view>

#include "picaresque/article/article_types.hpp"
#include "picaresque/group/group_types.hpp"
#include "picaresque/permission/types.hpp"
#include "picaresque/user/user_types.hpp"

namespace Json {
class Value;
}

namespace picaresque::http {

Json::Value ToJson(permission::Role role);
Json::Value ToJson(const permission::ScopedPermission& permission);
Json::Value ToJson(const permission::AccessRequirement& requirement);
Json::Value ToJson(const article::ArticleSummary& summary);
Json::Value ToJson(const article::ArticleDetails& details);
Json::Value ToJson(group::InvitationStatus status);
Json::Value ToJson(const group::GroupSummary& summary);
Json::Value ToJson(const group::GroupDetails& details);
Json::Value ToJson(const group::GroupInvitation& invitation);
Json::Value ToJson(const user::UserSummary& summary);
Json::Value ToJson(const user::UserDetails& details);
Json::Value BuildMeta(std::string_view request_id);

}  // namespace picaresque::http
