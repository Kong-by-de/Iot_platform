#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace iot_core::core {

class ConfigManager {
 public:
  // Singleton pattern
  static ConfigManager& instance();

  // Load configuration from all sources
  bool load();
  void reload();

  // Access configuration values
  std::string getString(const std::string& key,
                        const std::string& defaultValue = "") const;
  int getInt(const std::string& key, int defaultValue = 0) const;
  double getDouble(const std::string& key, double defaultValue = 0.0) const;
  bool getBool(const std::string& key, bool defaultValue = false) const;

  // Structured configuration sections
  struct DatabaseConfig {
    std::string host;
    int port;
    std::string name;
    std::string user;
    std::string password;
    std::string connectionString;
    int maxConnections;
    int connectionTimeout;
  };

  struct ServerConfig {
    std::string host;
    int port;
    int threads;
    int timeout;
    bool corsEnabled;
  };

  struct TelegramConfig {
    bool enabled;
    std::string token;
    std::string parseMode;
  };

  struct EmailConfig {
    bool enabled;
    std::string smtpHost;
    int smtpPort;
    std::string username;
    std::string password;
    std::string fromAddress;
    std::vector<std::string> recipients;
  };

  struct SimulationConfig {
    bool enabled;
    int deviceCount;
    int updateIntervalMs;
    double failureProbability;
  };

  struct LoggingConfig {
    std::string level;
    std::string file;
    int maxSizeMB;
    int maxFiles;
  };

  struct AlertConfig {
    int cacheDurationMinutes;
    int maxAlertsPerHour;
    int cooldownSeconds;
  };

  // НОВАЯ СТРУКТУРА: Конфигурация удаленной БД
  struct RemoteDatabaseConfig {
    std::string host = "localhost";
    int port = 5432;
    std::string name = "iot_db";
    std::string user = "iot_user";
    std::string password = "iot_pass";
    std::string connectionString;
    int pollingIntervalSeconds = 30;
    bool enabled = false;
  };

  // Get structured configs
  DatabaseConfig getDatabaseConfig() const;
  ServerConfig getServerConfig() const;
  TelegramConfig getTelegramConfig() const;
  EmailConfig getEmailConfig() const;
  SimulationConfig getSimulationConfig() const;
  LoggingConfig getLoggingConfig() const;
  AlertConfig getAlertConfig() const;
  RemoteDatabaseConfig getRemoteDatabaseConfig() const;  // НОВЫЙ МЕТОД

  // Info
  bool isLoaded() const { return loaded_; }
  std::string getSource() const { return source_; }

 private:
  ConfigManager();
  ~ConfigManager() = default;

  // Delete copy constructor and assignment operator
  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;

  // Load methods
  void loadDefaults();
  bool loadFromEnvFile(const std::string& filename = ".env");
  bool loadFromYamlFile(const std::string& filename = "config.yaml");
  bool loadFromEnvironment();
  void mergeConfigurations();

  // Helper methods
  std::string trim(const std::string& str) const;
  bool parseBool(const std::string& value) const;
  std::vector<std::string> split(const std::string& str, char delimiter) const;

  // Configuration storage
  std::unordered_map<std::string, std::string> config_;
  bool loaded_ = false;
  std::string source_ = "default";
};

}  // namespace iot_core::core