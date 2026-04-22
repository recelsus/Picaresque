#include "picaresque/embedded_query/query_parser.hpp"

#include <algorithm>

namespace picaresque::embedded_query {
namespace {

std::string Trim(const std::string& value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

void ParseIdAndSql(QueryFragment& fragment, const std::string& raw) {
  auto text = Trim(raw);
  if (text.rfind("id=", 0) != 0) {
    fragment.sql = text;
    return;
  }
  const auto id_end = text.find_first_of(" \t\r\n");
  if (id_end == std::string::npos) {
    fragment.query_id = text.substr(3);
    fragment.sql = "";
    return;
  }
  fragment.query_id = text.substr(3, id_end - 3);
  fragment.sql = Trim(text.substr(id_end + 1));
}

}  // namespace

std::vector<QueryFragment> ExtractQueryFragments(const std::string& body) {
  std::vector<QueryFragment> fragments;
  std::size_t block_index = 0;
  std::size_t search = 0;
  while (true) {
    const auto start = body.find("```query", search);
    if (start == std::string::npos) {
      break;
    }
    const auto content_start = body.find('\n', start);
    if (content_start == std::string::npos) {
      break;
    }
    const auto end = body.find("```", content_start + 1);
    if (end == std::string::npos) {
      break;
    }

    QueryFragment fragment{
        .fragment_kind = FragmentKind::Block,
        .location_index = block_index++,
        .start_offset = start,
        .end_offset = end + 3,
    };
    auto content = body.substr(content_start + 1, end - content_start - 1);
    const auto first_line_end = content.find('\n');
    if (first_line_end != std::string::npos && Trim(content.substr(0, first_line_end)).rfind("id=", 0) == 0) {
      fragment.query_id = Trim(content.substr(0, first_line_end)).substr(3);
      fragment.sql = Trim(content.substr(first_line_end + 1));
    } else {
      fragment.sql = Trim(content);
    }
    fragments.push_back(fragment);
    search = end + 3;
  }

  std::size_t inline_index = 0;
  search = 0;
  while (true) {
    const auto start = body.find('`', search);
    if (start == std::string::npos) {
      break;
    }
    if (body.compare(start, 3, "```") == 0) {
      const auto block_end = body.find("```", start + 3);
      search = block_end == std::string::npos ? body.size() : block_end + 3;
      continue;
    }
    const auto end = body.find('`', start + 1);
    if (end == std::string::npos) {
      break;
    }
    const auto content = body.substr(start + 1, end - start - 1);
    if (content.rfind("query:", 0) == 0) {
      QueryFragment fragment{
          .fragment_kind = FragmentKind::Inline,
          .location_index = inline_index++,
          .start_offset = start,
          .end_offset = end + 1,
      };
      ParseIdAndSql(fragment, content.substr(6));
      fragments.push_back(fragment);
    }
    search = end + 1;
  }

  std::sort(
      fragments.begin(),
      fragments.end(),
      [](const QueryFragment& lhs, const QueryFragment& rhs) {
        return lhs.start_offset < rhs.start_offset;
      });
  return fragments;
}

std::string RewriteBodyWithQueryIds(
    const std::string& body,
    const std::vector<QueryFragment>& fragments,
    const std::vector<StoredEmbeddedQuery>& queries) {
  std::string rewritten;
  std::size_t cursor = 0;
  for (std::size_t index = 0; index < fragments.size(); ++index) {
    const auto& fragment = fragments[index];
    const auto& query = queries[index];
    rewritten.append(body.substr(cursor, fragment.start_offset - cursor));
    if (fragment.fragment_kind == FragmentKind::Inline) {
      rewritten.append("`query: id=");
      rewritten.append(query.query_id);
      rewritten.push_back(' ');
      rewritten.append(query.sql);
      rewritten.push_back('`');
    } else {
      rewritten.append("```query\nid=");
      rewritten.append(query.query_id);
      rewritten.push_back('\n');
      rewritten.append(query.sql);
      rewritten.append("\n```");
    }
    cursor = fragment.end_offset;
  }
  rewritten.append(body.substr(cursor));
  return rewritten;
}

}  // namespace picaresque::embedded_query
