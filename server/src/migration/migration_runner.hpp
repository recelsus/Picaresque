#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace picaresque::migration {

struct DbConfig {
  std::string host;
  unsigned int port = 3306;
  std::string dbname;
  std::string user;
  std::string passwd;
};

DbConfig LoadDbConfig(const std::filesystem::path& config_path);
std::vector<std::filesystem::path> DiscoverMigrationFiles(const std::filesystem::path& sql_dir);
std::vector<std::string> SplitSqlStatements(const std::string& sql);
int RunMigrations(const std::filesystem::path& config_path, const std::filesystem::path& sql_dir);

}  // namespace picaresque::migration
