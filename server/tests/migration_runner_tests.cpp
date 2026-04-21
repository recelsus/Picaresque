#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

#include "../src/migration/migration_runner.hpp"

namespace migration = picaresque::migration;

int main() {
  {
    const auto statements = migration::SplitSqlStatements(
        "CREATE TABLE sample (value VARCHAR(255));\n"
        "INSERT INTO sample (value) VALUES ('a;b');\n"
        "-- comment ;\n"
        "/* block ; comment */\n"
        "ALTER TABLE sample ADD COLUMN note VARCHAR(255);");
    assert(statements.size() == 3);
    assert(statements[0] == "CREATE TABLE sample (value VARCHAR(255))");
    assert(statements[1] == "INSERT INTO sample (value) VALUES ('a;b')");
    assert(statements[2] == "ALTER TABLE sample ADD COLUMN note VARCHAR(255)");
  }

  {
    const auto temp_dir = std::filesystem::temp_directory_path() / "picaresque_migration_runner_tests";
    std::filesystem::create_directories(temp_dir);

    {
      std::ofstream stream(temp_dir / "002_second.sql");
      stream << "SELECT 2;";
    }
    {
      std::ofstream stream(temp_dir / "001_first.sql");
      stream << "SELECT 1;";
    }
    {
      std::ofstream stream(temp_dir / "README.md");
      stream << "ignored";
    }

    const auto files = migration::DiscoverMigrationFiles(temp_dir);
    assert(files.size() == 2);
    assert(files[0].filename() == "001_first.sql");
    assert(files[1].filename() == "002_second.sql");

    std::filesystem::remove(temp_dir / "001_first.sql");
    std::filesystem::remove(temp_dir / "002_second.sql");
    std::filesystem::remove(temp_dir / "README.md");
    std::filesystem::remove(temp_dir);
  }

  return 0;
}
