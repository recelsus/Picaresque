#include "query_tokenizer.hpp"

#include <algorithm>
#include <cctype>

namespace picaresque::embedded_query {
namespace {

std::string ToLower(std::string value) {
  std::transform(
      value.begin(),
      value.end(),
      value.begin(),
      [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
  return value;
}

bool IsIdentifierStart(char value) {
  return std::isalpha(static_cast<unsigned char>(value)) || value == '_';
}

bool IsIdentifierPart(char value) {
  return std::isalnum(static_cast<unsigned char>(value)) || value == '_';
}

}  // namespace

bool IsKeyword(const Token& token, const std::string& keyword) {
  return token.kind == TokenKind::Identifier && token.text == keyword;
}

std::optional<std::vector<Token>> Tokenize(const std::string& sql, std::string& error_message) {
  std::vector<Token> tokens;
  for (std::size_t index = 0; index < sql.size();) {
    const char current = sql[index];
    if (std::isspace(static_cast<unsigned char>(current))) {
      ++index;
      continue;
    }
    if (current == '-' && index + 1 < sql.size() && sql[index + 1] == '-') {
      error_message = "comments are not allowed";
      return std::nullopt;
    }
    if (current == '/' && index + 1 < sql.size() && sql[index + 1] == '*') {
      error_message = "comments are not allowed";
      return std::nullopt;
    }
    if (current == '#') {
      error_message = "comments are not allowed";
      return std::nullopt;
    }
    if (IsIdentifierStart(current)) {
      const auto start = index++;
      while (index < sql.size() && IsIdentifierPart(sql[index])) {
        ++index;
      }
      tokens.push_back({.kind = TokenKind::Identifier, .text = ToLower(sql.substr(start, index - start))});
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(current))) {
      const auto start = index++;
      while (index < sql.size() &&
             (std::isdigit(static_cast<unsigned char>(sql[index])) || sql[index] == '.')) {
        ++index;
      }
      tokens.push_back({.kind = TokenKind::Number, .text = sql.substr(start, index - start)});
      continue;
    }
    if (current == '\'') {
      ++index;
      std::string value;
      while (index < sql.size() && sql[index] != '\'') {
        value.push_back(sql[index++]);
      }
      if (index >= sql.size()) {
        error_message = "unterminated string literal";
        return std::nullopt;
      }
      ++index;
      tokens.push_back({.kind = TokenKind::String, .text = value});
      continue;
    }
    if (current == ',') {
      tokens.push_back({.kind = TokenKind::Comma, .text = ","});
      ++index;
      continue;
    }
    if (current == '.') {
      tokens.push_back({.kind = TokenKind::Dot, .text = "."});
      ++index;
      continue;
    }
    if (current == '*') {
      tokens.push_back({.kind = TokenKind::Star, .text = "*"});
      ++index;
      continue;
    }
    if (current == '(') {
      tokens.push_back({.kind = TokenKind::LeftParen, .text = "("});
      ++index;
      continue;
    }
    if (current == ')') {
      tokens.push_back({.kind = TokenKind::RightParen, .text = ")"});
      ++index;
      continue;
    }
    if (current == ';') {
      tokens.push_back({.kind = TokenKind::Semicolon, .text = ";"});
      ++index;
      continue;
    }
    if (current == '=' || current == '<' || current == '>' || current == '!') {
      std::string op(1, current);
      if (index + 1 < sql.size() && sql[index + 1] == '=') {
        op.push_back('=');
        ++index;
      }
      if (op == "!") {
        error_message = "unsupported operator";
        return std::nullopt;
      }
      tokens.push_back({.kind = TokenKind::Operator, .text = op});
      ++index;
      continue;
    }
    error_message = "unsupported token";
    return std::nullopt;
  }
  tokens.push_back({.kind = TokenKind::End, .text = ""});
  return tokens;
}

}  // namespace picaresque::embedded_query
