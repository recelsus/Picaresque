#include "picaresque/permission/resource.hpp"

#include "picaresque/permission/access.hpp"

namespace picaresque::permission {

bool CanReadArticle(const User& user, const ArticleResource& article) {
  return CanRead(user, article.required_permissions);
}

bool CanWriteArticle(const User& user, const ArticleResource& article) {
  return CanWrite(user, article.required_permissions);
}

bool CanReadTable(const User& user, const TableResource& table) {
  return CanRead(user, table.required_permissions);
}

bool CanWriteTable(const User& user, const TableResource& table) {
  return CanWrite(user, table.required_permissions);
}

bool CanViewEmbeddedQuery(const User& user, const EmbeddedQueryResource& query) {
  return CanReadArticle(user, query.article);
}

bool CanEditEmbeddedQuery(const User& user, const EmbeddedQueryResource& query) {
  return CanWriteTable(user, query.table);
}

}  // namespace picaresque::permission
