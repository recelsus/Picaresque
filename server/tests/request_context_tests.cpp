#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

#include <drogon/HttpRequest.h>

#include "picaresque/access/api.hpp"
#include "picaresque/auth/auth_service.hpp"
#include "picaresque/group/group_management_service.hpp"
#include "picaresque/http/request_context.hpp"
#include "picaresque/user/in_memory_user_group_repository.hpp"
#include "picaresque/user/user_management_service.hpp"

namespace auth = picaresque::auth;
namespace group = picaresque::group;
namespace http = picaresque::http;
namespace p_access = picaresque::access;
namespace permission = picaresque::permission;
namespace user = picaresque::user;

namespace {

drogon::HttpRequestPtr BuildRequest(
    const std::string& path,
    const std::string& client_ip,
    const std::string& api_key = "") {
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setPath(path);
  request->addHeader("X-Request-Id", "request_context_test");
  request->addHeader("X-Real-IP", client_ip);
  if (!api_key.empty()) {
    request->addHeader("X-API-Key", api_key);
  }
  return request;
}

}  // namespace

int main() {
  user::InMemoryUserGroupRepository user_repository;
  const user::UserManagementService user_service(user_repository);
  const auth::AuthService auth_service(user_repository);
  const group::GroupManagementService group_service(user_repository);

  const auto admin = user_service.CreateInitialAdmin({
      .login_id = "admin",
      .user_name = "Initial Admin",
      .email = "admin@example.local",
      .password = "change-me",
      .role = permission::Role::Admin,
  });
  const auto member = user_service.CreateUser({
      .login_id = "member",
      .user_name = "Member",
      .email = "member@example.local",
      .password = "change-me",
      .role = permission::Role::Member,
  });
  const auto issued_key = auth_service.IssueUserApiKey(admin.summary.user_id);

  const p_access::InMemoryAccessRepository access_repository(
      {
          {
              .id = 1,
              .value_text = "203.0.113.0/24",
              .address_family = p_access::AddressFamily::IPv4,
              .rule_type = p_access::IpRuleType::Cidr,
              .prefix_length = 24,
              .effect = p_access::RuleEffect::Allow,
              .enabled = true,
              .surface = p_access::AccessSurface::RestApi,
          },
          {
              .id = 2,
              .value_text = "203.0.113.99",
              .address_family = p_access::AddressFamily::IPv4,
              .rule_type = p_access::IpRuleType::Single,
              .prefix_length = 32,
              .effect = p_access::RuleEffect::Deny,
              .enabled = true,
              .surface = std::nullopt,
          },
      });
  const p_access::AccessConfiguration deny_by_default{
      .web_default_policy = p_access::DefaultPolicy::Deny,
      .rest_api_default_policy = p_access::DefaultPolicy::Deny,
  };

  {
    const auto context = http::BuildRequestContext(
        BuildRequest("/api/v1/admin/groups", "203.0.113.10", issued_key.plain_api_key),
        true,
        access_repository,
        deny_by_default,
        user_repository);
    assert(context.request_id == "request_context_test");
    assert(context.client_ip == "203.0.113.10");
    assert(context.surface == p_access::AccessSurface::RestApi);
    assert(context.authenticated_user.has_value());
    assert(context.authenticated_user->summary.user_id == admin.summary.user_id);

    const group::CreateGroupCommand create_group_command{
        .actor_user_id = context.authenticated_user->summary.user_id,
        .group_name = "context-group",
        .description = std::string("Created from authenticated context"),
    };
    const auto created_group = group_service.CreateGroup(create_group_command);

    const group::InviteUserCommand invite_user_command{
        .actor_user_id = context.authenticated_user->summary.user_id,
        .group_id = created_group.summary.group_id,
        .invited_user_id = member.summary.user_id,
    };
    const auto invitation = group_service.InviteUser(invite_user_command);
    assert(invitation.status == group::InvitationStatus::Pending);
  }

  {
    bool failed = false;
    try {
      static_cast<void>(http::BuildRequestContext(
          BuildRequest("/api/v1/admin/groups", "203.0.113.10"),
          true,
          access_repository,
          deny_by_default,
          user_repository));
    } catch (const std::runtime_error& error) {
      failed = std::string(error.what()) == "api_key_required";
    }
    assert(failed);
  }

  {
    bool failed = false;
    try {
      static_cast<void>(http::BuildRequestContext(
          BuildRequest("/api/v1/admin/groups", "203.0.113.99", issued_key.plain_api_key),
          true,
          access_repository,
          deny_by_default,
          user_repository));
    } catch (const std::runtime_error& error) {
      failed = std::string(error.what()).rfind("access_denied:", 0) == 0;
    }
    assert(failed);
  }

  return 0;
}
