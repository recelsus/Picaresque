#include "migration_runner.hpp"

#include <filesystem>
#include <iostream>

namespace {

std::filesystem::path ResolveConfigPath() {
  if (std::filesystem::exists("config/config.local.json")) {
    return "config/config.local.json";
  }
  if (std::filesystem::exists("server/config/config.local.json")) {
    return "server/config/config.local.json";
  }
  if (std::filesystem::exists("config/config.example.json")) {
    return "config/config.example.json";
  }
  return "server/config/config.example.json";
}

std::filesystem::path ResolveSqlDir() {
  if (std::filesystem::exists("sql")) {
    return "sql";
  }
  return "server/sql";
}

}  // namespace

int main() {
  try {
    return picaresque::migration::RunMigrations(ResolveConfigPath(), ResolveSqlDir());
  } catch (const std::exception& error) {
    std::cerr << "[picaresque_migrate] " << error.what() << '\n';
    return 1;
  }
}
