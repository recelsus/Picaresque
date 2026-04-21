#include <cassert>

#include "picaresque/permission/api.hpp"

namespace permission = picaresque::permission;

int main() {
  const permission::User member_user{
      .user_id = "user_member_resource",
      .user_name = "Member Resource",
      .role = permission::Role::Member,
      .owned_groups = {},
      .scoped_permissions = {
          {"group_alpha", 60, 30},
          {"group_beta", 10, 10},
      },
  };

  const permission::ArticleResource public_article{
      .article_id = "article_public",
      .required_permissions = {},
  };

  const permission::ArticleResource restricted_article{
      .article_id = "article_group_alpha",
      .required_permissions = {
          {"group_alpha", 30, 30},
      },
  };

  const permission::ArticleResource write_restricted_article{
      .article_id = "article_group_alpha_write_60",
      .required_permissions = {
          {"group_alpha", 60, 60},
      },
  };

  const permission::TableResource public_table{
      .table_name = "table_public",
      .required_permissions = {},
  };

  const permission::TableResource restricted_table{
      .table_name = "table_group_alpha",
      .required_permissions = {
          {"group_alpha", 30, 30},
      },
  };

  const permission::TableResource write_restricted_table{
      .table_name = "table_group_alpha_write_60",
      .required_permissions = {
          {"group_alpha", 60, 60},
      },
  };

  const permission::TableResource or_table{
      .table_name = "table_group_alpha_or_beta",
      .required_permissions = {
          {"group_missing", 90, 90},
          {"group_alpha", 30, 30},
      },
  };

  const permission::EmbeddedQueryResource article_open_table_locked_query{
      .query_id = "query_article_open_table_locked",
      .article =
          {
              .article_id = "article_open",
              .required_permissions = {},
          },
      .table =
          {
              .table_name = "table_locked",
              .required_permissions = {
                  {"group_alpha", 60, 60},
              },
          },
  };

  const permission::EmbeddedQueryResource article_locked_table_open_query{
      .query_id = "query_article_locked_table_open",
      .article =
          {
              .article_id = "article_locked",
              .required_permissions = {
                  {"group_missing", 90, 90},
              },
          },
      .table =
          {
              .table_name = "table_open",
              .required_permissions = {},
          },
  };

  const permission::EmbeddedQueryResource article_10_table_30_query{
      .query_id = "query_article_10_table_30",
      .article =
          {
              .article_id = "article_beta_10",
              .required_permissions = {
                  {"group_beta", 10, 10},
              },
          },
      .table =
          {
              .table_name = "table_missing_30",
              .required_permissions = {
                  {"group_missing", 30, 30},
              },
          },
  };

  const permission::User owner_user{
      .user_id = "user_owner_resource",
      .user_name = "Owner Resource",
      .role = permission::Role::Owner,
      .owned_groups = {"group_owned"},
      .scoped_permissions = {},
  };

  const permission::ArticleResource owned_article{
      .article_id = "article_owned",
      .required_permissions = {
          {"group_owned", 99, 99},
      },
  };

  const permission::TableResource owned_table{
      .table_name = "table_owned",
      .required_permissions = {
          {"group_owned", 99, 99},
      },
  };

  const permission::User wildcard_user{
      .user_id = "user_wildcard_resource",
      .user_name = "Wildcard Resource",
      .role = permission::Role::Member,
      .owned_groups = {},
      .scoped_permissions = {
          {"*", 30, 30},
      },
  };

  const permission::ArticleResource wildcard_article{
      .article_id = "article_wildcard",
      .required_permissions = {
          {"group_any", 30, 30},
      },
  };

  const permission::TableResource wildcard_table{
      .table_name = "table_wildcard",
      .required_permissions = {
          {"group_any", 30, 30},
      },
  };

  const permission::User direct_over_wildcard_user{
      .user_id = "user_direct_over_wildcard",
      .user_name = "Direct Over Wildcard",
      .role = permission::Role::Member,
      .owned_groups = {},
      .scoped_permissions = {
          {"group_direct", 10, 10},
          {"*", 90, 90},
      },
  };

  const permission::ArticleResource direct_scope_article{
      .article_id = "article_direct_scope",
      .required_permissions = {
          {"group_direct", 30, 30},
      },
  };

  assert(permission::CanReadArticle(member_user, public_article));
  assert(permission::CanWriteArticle(member_user, public_article));
  assert(permission::CanReadArticle(member_user, restricted_article));
  assert(permission::CanWriteArticle(member_user, restricted_article));
  assert(permission::CanReadArticle(member_user, write_restricted_article));
  assert(!permission::CanWriteArticle(member_user, write_restricted_article));

  assert(permission::CanReadTable(member_user, public_table));
  assert(permission::CanWriteTable(member_user, public_table));
  assert(permission::CanReadTable(member_user, restricted_table));
  assert(permission::CanWriteTable(member_user, restricted_table));
  assert(permission::CanReadTable(member_user, write_restricted_table));
  assert(!permission::CanWriteTable(member_user, write_restricted_table));
  assert(permission::CanReadTable(member_user, or_table));
  assert(permission::CanWriteTable(member_user, or_table));

  assert(permission::CanViewEmbeddedQuery(member_user, article_open_table_locked_query));
  assert(!permission::CanEditEmbeddedQuery(member_user, article_open_table_locked_query));
  assert(!permission::CanViewEmbeddedQuery(member_user, article_locked_table_open_query));
  assert(permission::CanEditEmbeddedQuery(member_user, article_locked_table_open_query));
  assert(permission::CanViewEmbeddedQuery(member_user, article_10_table_30_query));
  assert(!permission::CanEditEmbeddedQuery(member_user, article_10_table_30_query));

  assert(permission::CanReadArticle(owner_user, owned_article));
  assert(permission::CanWriteArticle(owner_user, owned_article));
  assert(permission::CanReadTable(owner_user, owned_table));
  assert(permission::CanWriteTable(owner_user, owned_table));

  assert(permission::CanReadArticle(wildcard_user, wildcard_article));
  assert(permission::CanWriteArticle(wildcard_user, wildcard_article));
  assert(permission::CanReadTable(wildcard_user, wildcard_table));
  assert(permission::CanWriteTable(wildcard_user, wildcard_table));

  assert(!permission::CanReadArticle(direct_over_wildcard_user, direct_scope_article));
  assert(!permission::CanWriteArticle(direct_over_wildcard_user, direct_scope_article));

  return 0;
}
