#include "ConfigManager.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

// Для YAML парсинга
#ifdef HAS_YAML_CPP
#include <yaml-cpp/yaml.h>
#endif

namespace iot_core::core {

ConfigManager& ConfigManager::instance() {
  static ConfigManager instance;
  return instance;
}

ConfigManager::ConfigManager() { loadDefaults(); }

bool ConfigManager::load() {
  std::cout << "🔧 Loading configuration..." << std::endl;

  // Reset
  config_.clear();
  loadDefaults();

  // Load from different sources (order matters - last wins)
  bool yamlLoaded = false;
  bool envFileLoaded = false;
  bool envVarsLoaded = false;

#ifdef HAS_YAML_CPP
  yamlLoaded = loadFromYamlFile();
#else
  std::cout << "   ⚠️  YAML support disabled (no yaml-cpp)" << std::endl;
#endif

  envFileLoaded = loadFromEnvFile();
  envVarsLoaded = loadFromEnvironment();

  // Merge configurations
  mergeConfigurations();

  loaded_ = yamlLoaded || envFileLoaded || envVarsLoaded;

  // Build source info
  source_ = "Sources: ";
  if (yamlLoaded) source_ += "yaml ";
  if (envFileLoaded) source_ += "env-file ";
  if (envVarsLoaded) source_ += "env-vars ";

  if (!loaded_) {
    source_ = "defaults only";
    std::cout << "   ⚠️  No configuration files found, using defaults"
              << std::endl;
  } else {
    std::cout << "   ✅ Configuration loaded from: " << source_ << std::endl;
  }

  return loaded_;
}

void ConfigManager::reload() { load(); }

std::string ConfigManager::getString(const std::string& key,
                                     const std::string& defaultValue) const {
  auto it = config_.find(key);
  if (it != config_.end() && !it->second.empty()) {
    return it->second;
  }
  return defaultValue;
}

int ConfigManager::getInt(const std::string& key, int defaultValue) const {
  auto it = config_.find(key);
  if (it != config_.end() && !it->second.empty()) {
    try {
      return std::stoi(it->second);
    } catch (...) {
      return defaultValue;
    }
  }
  return defaultValue;
}

double ConfigManager::getDouble(const std::string& key,
                                double defaultValue) const {
  auto it = config_.find(key);
  if (it != config_.end() && !it->second.empty()) {
    try {
      return std::stod(it->second);
    } catch (...) {
      return defaultValue;
    }
  }
  return defaultValue;
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) const {
  auto it = config_.find(key);
  if (it != config_.end() && !it->second.empty()) {
    return parseBool(it->second);
  }
  return defaultValue;
}

ConfigManager::DatabaseConfig ConfigManager::getDatabaseConfig() const {
  DatabaseConfig db;

  // Check for direct connection string first
  std::string connStr = getString("DB_CONNECTION_STRING");
  if (!connStr.empty()) {
    db.connectionString = connStr;
    // Parse components from connection string if needed
    size_t start = connStr.find("://");
    if (start != std::string::npos) {
      size_t userEnd = connStr.find(':', start + 3);
      size_t passEnd = connStr.find('@', userEnd);
      size_t hostEnd = connStr.find(':', passEnd);
      size_t portEnd = connStr.find('/', hostEnd);

      if (userEnd != std::string::npos && passEnd != std::string::npos) {
        db.user = connStr.substr(start + 3, userEnd - (start + 3));
        db.password = connStr.substr(userEnd + 1, passEnd - (userEnd + 1));
      }

      if (hostEnd != std::string::npos && portEnd != std::string::npos) {
        db.host = connStr.substr(passEnd + 1, hostEnd - (passEnd + 1));
        std::string portStr =
            connStr.substr(hostEnd + 1, portEnd - (hostEnd + 1));
        try {
          db.port = std::stoi(portStr);
        } catch (...) {
          db.port = 5432;
        }

        db.name = connStr.substr(portEnd + 1);
      }
    }
  } else {
    // Use individual parameters
    db.host = getString("database.host", "localhost");
    db.port = getInt("database.port", 5432);
    db.name = getString("database.name", "iot_devices");
    db.user = getString("database.user", "iot_user");
    db.password = getString("database.password", "pass2025");

    // Build connection string
    std::ostringstream oss;
    oss << "host=" << db.host << " port=" << db.port << " dbname=" << db.name
        << " user=" << db.user << " password=" << db.password;
    db.connectionString = oss.str();
  }

  db.maxConnections = getInt("database.max_connections", 10);
  db.connectionTimeout = getInt("database.connection_timeout", 30);

  return db;
}

ConfigManager::ServerConfig ConfigManager::getServerConfig() const {
  ServerConfig server;
  server.host = getString("server.host", "0.0.0.0");
  server.port = getInt("server.port", 8080);
  server.threads = getInt("server.threads", 4);
  server.timeout = getInt("server.timeout", 30);
  server.corsEnabled = getBool("server.cors_enabled", true);
  return server;
}

ConfigManager::TelegramConfig ConfigManager::getTelegramConfig() const {
  TelegramConfig telegram;
  telegram.enabled = getBool("telegram.enabled", true);
  telegram.token = getString("TELEGRAM_BOT_TOKEN", "");

  // Fallback to config file if env var not set
  if (telegram.token.empty()) {
    telegram.token = getString("telegram.token", "");
  }

  telegram.parseMode = getString("telegram.parse_mode", "Markdown");
  return telegram;
}

ConfigManager::EmailConfig ConfigManager::getEmailConfig() const {
  EmailConfig email;
  email.enabled = getBool("email.enabled", false);
  email.smtpHost = getString("email.smtp_host", "smtp.gmail.com");
  email.smtpPort = getInt("email.smtp_port", 587);

  // Get from environment variables first
  email.username = getString("SMTP_USERNAME", "");
  email.password = getString("SMTP_PASSWORD", "");
  email.fromAddress = getString("SMTP_FROM_EMAIL", "");

  // Fallback to config file
  if (email.username.empty()) email.username = getString("email.username", "");
  if (email.password.empty()) email.password = getString("email.password", "");
  if (email.fromAddress.empty())
    email.fromAddress = getString("email.from_address", "");

  // Collect recipients
  std::vector<std::string> recipients;

  // Check environment variables
  std::string alertEmail1 = getString("ALERT_EMAIL_1", "");
  std::string alertEmail2 = getString("ALERT_EMAIL_2", "");

  if (!alertEmail1.empty()) recipients.push_back(alertEmail1);
  if (!alertEmail2.empty()) recipients.push_back(alertEmail2);

  // Check config file
  std::string configRecipients = getString("email.recipients", "");
  if (!configRecipients.empty()) {
    auto additional = split(configRecipients, ',');
    recipients.insert(recipients.end(), additional.begin(), additional.end());
  }

  email.recipients = recipients;

  return email;
}

ConfigManager::SimulationConfig ConfigManager::getSimulationConfig() const {
  SimulationConfig sim;
  sim.enabled = getBool("ENABLE_SIMULATION", true);

  // Fallback to config file
  if (!sim.enabled) {
    sim.enabled = getBool("simulation.enabled", true);
  }

  sim.deviceCount = getInt("SIMULATION_DEVICE_COUNT", 3);
  if (sim.deviceCount == 0) {
    sim.deviceCount = getInt("simulation.device_count", 3);
  }

  sim.updateIntervalMs = getInt("simulation.update_interval_ms", 10000);
  sim.failureProbability = getDouble("simulation.failure_probability", 0.01);

  return sim;
}

ConfigManager::LoggingConfig ConfigManager::getLoggingConfig() const {
  LoggingConfig log;
  log.level = getString("LOG_LEVEL", "INFO");
  if (log.level == "INFO") {
    log.level = getString("logging.level", "INFO");
  }

  log.file = getString("logging.file", "logs/iot_core.log");
  log.maxSizeMB = getInt("logging.max_size_mb", 10);
  log.maxFiles = getInt("logging.max_files", 5);

  return log;
}

ConfigManager::AlertConfig ConfigManager::getAlertConfig() const {
  AlertConfig alert;
  alert.cacheDurationMinutes = getInt("alerts.cache_duration_minutes", 5);
  alert.maxAlertsPerHour = getInt("alerts.max_alerts_per_hour", 60);
  alert.cooldownSeconds = getInt("alerts.cooldown_seconds", 300);
  return alert;
}

// Получение конфигурации удаленной БД
ConfigManager::RemoteDatabaseConfig ConfigManager::getRemoteDatabaseConfig()
    const {
  RemoteDatabaseConfig remote;

  remote.enabled = getBool("REMOTE_DB_ENABLED", false);
  remote.host = getString("REMOTE_DB_HOST", "localhost");
  remote.port = getInt("REMOTE_DB_PORT", 5432);
  remote.name = getString("REMOTE_DB_NAME", "iot_db");
  remote.user = getString("REMOTE_DB_USER", "iot_user");
  remote.password = getString("REMOTE_DB_PASSWORD", "iot_pass");
  remote.pollingIntervalSeconds = getInt("REMOTE_POLLING_INTERVAL", 30);

  // Проверяем, есть ли готовая строка подключения
  std::string connStr = getString("REMOTE_DB_CONNECTION_STRING");
  if (!connStr.empty()) {
    remote.connectionString = connStr;
  } else {
    // Собираем строку подключения из компонентов
    std::ostringstream oss;
    oss << "host=" << remote.host << " port=" << remote.port
        << " dbname=" << remote.name << " user=" << remote.user
        << " password=" << remote.password;
    remote.connectionString = oss.str();
  }

  return remote;
}

void ConfigManager::loadDefaults() {
  // Database
  config_["database.host"] = "localhost";
  config_["database.port"] = "5432";
  config_["database.name"] = "iot_devices";
  config_["database.user"] = "iot_user";
  config_["database.password"] = "pass2025";
  config_["database.max_connections"] = "10";
  config_["database.connection_timeout"] = "30";

  // Server
  config_["server.host"] = "0.0.0.0";
  config_["server.port"] = "8080";
  config_["server.threads"] = "4";
  config_["server.timeout"] = "30";
  config_["server.cors_enabled"] = "true";

  // Telegram
  config_["telegram.enabled"] = "true";
  config_["telegram.token"] = "";
  config_["telegram.parse_mode"] = "Markdown";

  // Email
  config_["email.enabled"] = "false";
  config_["email.smtp_host"] = "smtp.gmail.com";
  config_["email.smtp_port"] = "587";
  config_["email.username"] = "";
  config_["email.password"] = "";
  config_["email.from_address"] = "";
  config_["email.recipients"] = "";

  // Simulation
  config_["simulation.enabled"] = "true";
  config_["simulation.device_count"] = "3";
  config_["simulation.update_interval_ms"] = "10000";
  config_["simulation.failure_probability"] = "0.01";

  // Logging
  config_["logging.level"] = "INFO";
  config_["logging.file"] = "logs/iot_core.log";
  config_["logging.max_size_mb"] = "10";
  config_["logging.max_files"] = "5";

  // Alerts
  config_["alerts.cache_duration_minutes"] = "5";
  config_["alerts.max_alerts_per_hour"] = "60";
  config_["alerts.cooldown_seconds"] = "300";

  // НОВЫЕ ДЕФОЛТЫ: Удаленная БД
  config_["REMOTE_DB_ENABLED"] = "false";
  config_["REMOTE_DB_HOST"] = "localhost";
  config_["REMOTE_DB_PORT"] = "5432";
  config_["REMOTE_DB_NAME"] = "iot_db";
  config_["REMOTE_DB_USER"] = "iot_user";
  config_["REMOTE_DB_PASSWORD"] = "iot_pass";
  config_["REMOTE_POLLING_INTERVAL"] = "30";
}

bool ConfigManager::loadFromEnvFile(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  std::cout << "   📄 Loading from .env file..." << std::endl;
  int count = 0;
  std::string line;

  while (std::getline(file, line)) {
    line = trim(line);

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // Parse KEY=VALUE
    size_t equalsPos = line.find('=');
    if (equalsPos != std::string::npos) {
      std::string key = trim(line.substr(0, equalsPos));
      std::string value = trim(line.substr(equalsPos + 1));

      // Remove quotes if present
      if (!value.empty() && value[0] == '"' && value.back() == '"') {
        value = value.substr(1, value.length() - 2);
      }

      if (!key.empty() && !value.empty()) {
        config_[key] = value;
        count++;
      }
    }
  }

  if (count > 0) {
    std::cout << "     ✅ Loaded " << count << " variables from .env"
              << std::endl;
    return true;
  }

  return false;
}

#ifdef HAS_YAML_CPP
bool ConfigManager::loadFromYamlFile(const std::string& filename) {
  try {
    YAML::Node config = YAML::LoadFile(filename);
    if (!config) {
      return false;
    }

    std::cout << "   📄 Loading from YAML config..." << std::endl;
    int count = 0;

    // Helper lambda to load YAML values
    auto loadYamlValue = [&](const YAML::Node& node,
                             const std::string& prefix) {
      if (node.IsMap()) {
        for (const auto& entry : node) {
          std::string key = prefix.empty()
                                ? entry.first.as<std::string>()
                                : prefix + "." + entry.first.as<std::string>();

          if (entry.second.IsScalar()) {
            config_[key] = entry.second.as<std::string>();
            count++;
          } else if (entry.second.IsMap()) {
            loadYamlValue(entry.second, key);
          }
        }
      }
    };

    loadYamlValue(config, "");

    if (count > 0) {
      std::cout << "     ✅ Loaded " << count << " values from YAML"
                << std::endl;
      return true;
    }

  } catch (const YAML::Exception& e) {
    std::cerr << "     ❌ YAML parsing error: " << e.what() << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "     ❌ Error loading YAML: " << e.what() << std::endl;
  }

  return false;
}
#else
bool ConfigManager::loadFromYamlFile(const std::string& filename) {
  // YAML support disabled at compile time
  return false;
}
#endif

bool ConfigManager::loadFromEnvironment() {
  std::cout << "   🔍 Checking environment variables..." << std::endl;
  int count = 0;

  // List of environment variables we care about
  std::vector<std::string> envVars = {
      "DB_CONNECTION_STRING",    "TELEGRAM_BOT_TOKEN", "SMTP_USERNAME",
      "SMTP_PASSWORD",           "SMTP_FROM_EMAIL",    "ALERT_EMAIL_1",
      "ALERT_EMAIL_2",           "SERVER_PORT",        "ENABLE_SIMULATION",
      "SIMULATION_DEVICE_COUNT", "LOG_LEVEL",          "RUN_MIGRATIONS",
      "REMOTE_DB_ENABLED",       "REMOTE_DB_HOST",     "REMOTE_DB_PORT",
      "REMOTE_DB_NAME",          "REMOTE_DB_USER",     "REMOTE_DB_PASSWORD",
      "REMOTE_POLLING_INTERVAL"};

  for (const auto& var : envVars) {
    const char* value = std::getenv(var.c_str());
    if (value != nullptr && std::strlen(value) > 0) {
      config_[var] = value;
      count++;
    }
  }

  if (count > 0) {
    std::cout << "     ✅ Found " << count << " environment variables"
              << std::endl;
    return true;
  }

  return false;
}

void ConfigManager::mergeConfigurations() {}

std::string ConfigManager::trim(const std::string& str) const {
  size_t first = str.find_first_not_of(" \t\n\r");
  if (first == std::string::npos) return "";

  size_t last = str.find_last_not_of(" \t\n\r");
  return str.substr(first, (last - first + 1));
}

bool ConfigManager::parseBool(const std::string& value) const {
  std::string lower = value;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  return (lower == "true" || lower == "1" || lower == "yes" || lower == "on");
}

std::vector<std::string> ConfigManager::split(const std::string& str,
                                              char delimiter) const {
  std::vector<std::string> result;
  std::stringstream ss(str);
  std::string item;

  while (std::getline(ss, item, delimiter)) {
    item = trim(item);
    if (!item.empty()) {
      result.push_back(item);
    }
  }

  return result;
}

}  // namespace iot_core::core