#include <cassert>
#include <string>

#include "picaresque/embedded_query/query_validator.hpp"

namespace embedded_query = picaresque::embedded_query;

namespace {

embedded_query::QueryValidationResult ExpectValid(const std::string& sql) {
  std::string error_message;
  const auto result = embedded_query::ValidateSelectQuery(sql, {}, error_message);
  assert(result.has_value());
  assert(error_message.empty());
  return *result;
}

void ExpectRejected(const std::string& sql, const std::string& expected_message) {
  std::string error_message;
  const auto result = embedded_query::ValidateSelectQuery(sql, {}, error_message);
  assert(!result.has_value());
  assert(error_message == expected_message);
}

const embedded_query::QueryValidationResult::WhereExpression& RequiredWhere(
    const embedded_query::QueryValidationResult& result) {
  assert(result.where.has_value());
  return *result.where;
}

}  // namespace

int main() {
  {
    const auto result = ExpectValid(
        "SELECT name FROM table_orders WHERE (status = 'open' OR status = 'pending') AND amount >= 10 LIMIT 20");
    assert(result.table_id == "table_orders");
    assert(result.limit == 20);
    assert(result.selected_columns.size() == 1);
    assert(RequiredWhere(result).kind == embedded_query::QueryValidationResult::WhereExpression::Kind::And);
  }

  {
    const auto result = ExpectValid(
        "SELECT name FROM table_orders WHERE name LIKE 'a%' OR status IN ('open', 'closed') OR amount BETWEEN 10 AND 20");
    assert(result.table_ids.size() == 1);
    assert(RequiredWhere(result).kind == embedded_query::QueryValidationResult::WhereExpression::Kind::Or);
  }

  {
    const auto result = ExpectValid(
        "SELECT owner_key, count(*), sum(amount), avg(amount), min(amount), max(amount) "
        "FROM table_orders GROUP BY owner_key ORDER BY owner_key DESC LIMIT 100");
    assert(result.aggregate_selections.size() == 5);
    assert(result.group_by_columns.size() == 1);
    assert(result.order_by.has_value());
    assert(result.order_by->descending);
  }

  {
    const auto result = ExpectValid(
        "SELECT table_orders.name, table_owners.label, table_regions.region_name "
        "FROM table_orders "
        "JOIN table_owners ON table_orders.owner_key = table_owners.owner_key "
        "JOIN table_regions ON table_owners.region_key = table_regions.region_key");
    assert(result.table_ids.size() == 3);
    assert(result.joins.size() == 2);
    assert(result.limit == 1);
  }

  {
    const auto result = ExpectValid("SELECT name FROM table_orders WHERE note = 'owner''s memo'");
    assert(result.where->condition.literal == "owner's memo");
  }

  ExpectRejected("UPDATE table_orders SET name = 'x'", "only SELECT is allowed");
  ExpectRejected("SELECT name FROM table_orders LIMIT 1; SELECT name FROM table_orders", "multiple statements are not allowed");
  ExpectRejected("SELECT name FROM table_orders -- comment", "comments are not allowed");
  ExpectRejected("SELECT name FROM table_orders /* comment */", "comments are not allowed");
  ExpectRejected("SELECT name FROM table_orders WHERE note = 'unterminated", "unterminated string literal");
  ExpectRejected("SELECT name FROM table_orders LIMIT 0", "row limit exceeded");
  ExpectRejected("SELECT name FROM table_orders LIMIT 501", "row limit exceeded");
  ExpectRejected("SELECT owner_key, count(*) FROM table_orders", "GROUP BY required");
  ExpectRejected(
      "SELECT owner_key, name, count(*) FROM table_orders GROUP BY owner_key",
      "selected column must appear in GROUP BY");
  ExpectRejected("SELECT name FROM table_orders GROUP BY name", "unsupported GROUP BY");
  ExpectRejected("SELECT table_missing.name FROM table_orders", "undeclared table reference");
  ExpectRejected(
      "SELECT name FROM table_orders JOIN table_owners ON table_orders.owner_key = table_owners.owner_key",
      "qualified column reference required");
  ExpectRejected(
      "SELECT table_orders.name FROM table_orders JOIN table_owners ON owner_key = table_owners.owner_key",
      "qualified column reference required");
  ExpectRejected(
      "SELECT table_orders.name FROM table_orders JOIN table_owners ON table_orders.owner_key = table_owners.owner_key "
      "WHERE status = 'open'",
      "qualified column reference required");
  ExpectRejected(
      "SELECT table_orders.name FROM table_orders JOIN table_owners ON table_orders.owner_key = table_owners.owner_key "
      "ORDER BY name",
      "qualified column reference required");
  ExpectRejected(
      "SELECT name FROM table_orders LEFT JOIN table_owners ON table_orders.owner_key = table_owners.owner_key",
      "unsupported JOIN");
  ExpectRejected(
      "SELECT table_orders.name FROM table_orders JOIN table_owners USING (owner_key)",
      "unsupported JOIN");
  ExpectRejected(
      "SELECT owner_key, count(*) FROM table_orders GROUP BY owner_key HAVING count(*) > 1",
      "unsupported query tail");

  return 0;
}
