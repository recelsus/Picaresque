#pragma once

#include "picaresque/embedded_query/embedded_query_repository.hpp"

namespace picaresque::embedded_query {

EmbeddedQueryRepository& GetMySqlEmbeddedQueryRepository();

}  // namespace picaresque::embedded_query
