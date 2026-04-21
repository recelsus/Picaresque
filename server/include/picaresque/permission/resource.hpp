#pragma once

#include <string>
#include <vector>

#include "picaresque/permission/types.hpp"

namespace picaresque::permission {

struct TableResource {
  std::string table_name;
  std::vector<AccessRequirement> required_permissions;
};

struct ArticleResource {
  std::string article_id;
  std::vector<AccessRequirement> required_permissions;
};

struct EmbeddedQueryResource {
  std::string query_id;
  ArticleResource article;
  TableResource table;
};

bool CanReadArticle(const User& user, const ArticleResource& article);
bool CanWriteArticle(const User& user, const ArticleResource& article);
bool CanReadTable(const User& user, const TableResource& table);
bool CanWriteTable(const User& user, const TableResource& table);
bool CanViewEmbeddedQuery(const User& user, const EmbeddedQueryResource& query);
bool CanEditEmbeddedQuery(const User& user, const EmbeddedQueryResource& query);

}  // namespace picaresque::permission
