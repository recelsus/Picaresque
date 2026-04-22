#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <json/json.h>

namespace picaresque::embedded_query {

enum class FragmentKind {
  Inline,
  Block,
};

struct QueryFragment {
  FragmentKind fragment_kind = FragmentKind::Inline;
  std::size_t location_index = 0;
  std::size_t start_offset = 0;
  std::size_t end_offset = 0;
  std::optional<std::string> query_id;
  std::string sql;
};

struct StoredEmbeddedQuery {
  std::string query_id;
  std::string article_id;
  FragmentKind fragment_kind = FragmentKind::Inline;
  std::size_t location_index = 0;
  std::string sql;
  std::string table_id;
};

struct QueryValidationError {
  FragmentKind fragment_kind = FragmentKind::Inline;
  std::size_t location_index = 0;
  std::string message;
};

struct QueryValidationException final : public std::runtime_error {
  explicit QueryValidationException(std::vector<QueryValidationError> errors)
      : std::runtime_error("embedded_query_validation_failed"), query_errors(std::move(errors)) {}

  std::vector<QueryValidationError> query_errors;
};

struct ProcessedArticleQueries {
  std::string body;
  std::vector<StoredEmbeddedQuery> queries;
};

struct EmbeddedQueryExecutionResult {
  std::string query_id;
  FragmentKind fragment_kind = FragmentKind::Inline;
  std::string table_id;
  std::vector<std::string> columns;
  Json::Value rows = Json::arrayValue;
  std::uint64_t row_count = 0;
};

}  // namespace picaresque::embedded_query
