#pragma once

#include <string>
#include <vector>

#include "picaresque/embedded_query/embedded_query_types.hpp"

namespace picaresque::embedded_query {

std::vector<QueryFragment> ExtractQueryFragments(const std::string& body);
std::string RewriteBodyWithQueryIds(
    const std::string& body,
    const std::vector<QueryFragment>& fragments,
    const std::vector<StoredEmbeddedQuery>& queries);

}  // namespace picaresque::embedded_query
