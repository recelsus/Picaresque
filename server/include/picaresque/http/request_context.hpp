#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <optional>
#include <string>

#include "picaresque/access/types.hpp"
#include "picaresque/user/user_types.hpp"

namespace picaresque::http {

struct RequestContext {
  std::string request_id;
  std::string client_ip;
  access::AccessSurface surface = access::AccessSurface::RestApi;
  std::optional<user::UserDetails> authenticated_user;
};

RequestContext BuildRequestContext(const drogon::HttpRequestPtr& request, bool require_api_key);
drogon::HttpResponsePtr BuildRequestContextErrorResponse(
    const drogon::HttpRequestPtr& request,
    const std::string& error_code);

}  // namespace picaresque::http
