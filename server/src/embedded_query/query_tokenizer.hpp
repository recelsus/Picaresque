#pragma once

#include <optional>
#include <string>
#include <vector>

namespace picaresque::embedded_query {

enum class TokenKind {
  Identifier,
  Number,
  String,
  Comma,
  Dot,
  Star,
  LeftParen,
  RightParen,
  Operator,
  Semicolon,
  End,
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
};

bool IsKeyword(const Token& token, const std::string& keyword);

std::optional<std::vector<Token>> Tokenize(const std::string& sql, std::string& error_message);

}  // namespace picaresque::embedded_query
