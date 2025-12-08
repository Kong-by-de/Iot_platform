// src/core/DatabaseMigrator.h
#pragma once

#include <string>

namespace iot_core::core {
class DatabaseMigrator {
 public:
  explicit DatabaseMigrator(const std::string& connectionString);
  bool runMigrations();

 private:
  std::string connectionString_;
  bool waitForDatabase(int maxRetries = 10, int delaySeconds = 2);
  bool executeCommand(const std::string& command);
  bool createDbmateConfig();
  bool createMigrationsDirectory();
};

}  // namespace iot_core::core