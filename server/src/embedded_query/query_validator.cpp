#include "picaresque/embedded_query/query_validator.hpp"

#include "query_select_parser.hpp"
#include "query_tokenizer.hpp"

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace picaresque::embedded_query {
namespace {

std::optional<std::string> FindForbiddenKeywordError(const std::vector<Token>& tokens) {
  static const std::unordered_set<std::string> write_words = {
      "insert", "update", "delete", "drop", "alter", "create", "replace", "truncate", "grant", "revoke"};
  for (const auto& token : tokens) {
    if (token.kind != TokenKind::Identifier) {
      continue;
    }
    if (write_words.contains(token.text)) {
      return "only SELECT is allowed";
    }
    if (token.text == "left" || token.text == "right" || token.text == "full" || token.text == "using") {
      return "unsupported JOIN";
    }
    if (token.text == "having") {
      return "unsupported query tail";
    }
  }
  return std::nullopt;
}

}  // namespace

std::optional<QueryValidationResult> ValidateSelectQuery(
    const std::string& sql,
    const QueryValidationConfig& config,
    std::string& error_message) {
  if (sql.empty()) {
    error_message = "query is empty";
    return std::nullopt;
  }
  if (sql.size() > config.max_sql_length) {
    error_message = "query is too long";
    return std::nullopt;
  }
  auto tokens = Tokenize(sql, error_message);
  if (!tokens.has_value()) {
    return std::nullopt;
  }
  const auto forbidden_error = FindForbiddenKeywordError(*tokens);
  if (forbidden_error.has_value()) {
    error_message = *forbidden_error;
    return std::nullopt;
  }
  return ParseSelectQueryTokens(std::move(*tokens), config, error_message);
}

}  // namespace picaresque::embedded_query
