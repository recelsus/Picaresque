#pragma once

#include "picaresque/embedded_query/query_validator.hpp"

#include "query_tokenizer.hpp"

namespace picaresque::embedded_query {

std::optional<QueryValidationResult> ParseSelectQueryTokens(
    std::vector<Token> tokens,
    const QueryValidationConfig& config,
    std::string& error_message);

}  // namespace picaresque::embedded_query
