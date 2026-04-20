#pragma once

#include <string_view>

#include "picaresque/permission/types.hpp"
#include "picaresque/user/user_types.hpp"

namespace Json {
class Value;
}

namespace picaresque::http {

Json::Value ToJson(permission::Role role);
Json::Value ToJson(const permission::ScopedPermission& permission);
Json::Value ToJson(const user::UserSummary& summary);
Json::Value ToJson(const user::UserDetails& details);
Json::Value BuildMeta(std::string_view request_id);

}  // namespace picaresque::http
