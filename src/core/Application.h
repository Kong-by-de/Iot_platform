#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>  // НОВОЕ: добавлен thread

// Forward declarations
namespace iot_core {
namespace core {
class DatabaseRepository;
class NotificationService;
class ConfigManager;
}  // namespace core

namespace services {
class AlertProcessingService;
}

namespace api {
class TelemetryServer;
}

namespace bot {
class TelegramBotHandler;
}

namespace simulation {
class DeviceSimulator;
}

namespace engine {
class RuleEngine;
}

namespace models {
struct UserAlert;
}
}  // namespace iot_core

namespace iot_core::core {

class Application {
 public:
  Application();
  ~Application();

  // Initialize the application
  bool initialize();

  // Run the main loop
  void run();

  // Shutdown gracefully
  void shutdown();

  // Setup test user with default configuration
  void setupTestUser(long telegramId);

  // Component access (for testing or extensions)
  std::shared_ptr<DatabaseRepository> getDatabase() const { return database_; }
  std::shared_ptr<NotificationService> getNotifier() const { return notifier_; }
  std::shared_ptr<services::AlertProcessingService> getAlertService() const {
    return alertService_;
  }
  std::shared_ptr<engine::RuleEngine> getRuleEngine() const {
    return ruleEngine_;
  }
  std::unique_ptr<api::TelemetryServer>& getServer() { return httpServer_; }
  std::unique_ptr<bot::TelegramBotHandler>& getTelegramBot() {
    return telegramBot_;
  }
  std::unique_ptr<simulation::DeviceSimulator>& getSimulator() {
    return deviceSimulator_;
  }

 private:
  // Initialization methods
  void printWelcomeBanner() const;
  void loadConfiguration();
  void initializeComponents();
  void initializeDatabase();
  void initializeNotificationService();
  void initializeRuleEngine();
  void initializeHttpServer();
  void initializeTelegramBot();
  void initializeDeviceSimulator();

  // Runtime methods
  void setupSignalHandlers();
  void runMainLoop();
  void printStatusReport() const;
  void cleanup();

  // НОВЫЕ МЕТОДЫ: Управление периодической проверкой удаленной БД
  void startRemotePolling(int intervalSeconds);
  void stopRemotePolling();

  // Runtime configuration
  struct RuntimeConfig {
    // Database
    std::string dbHost;
    int dbPort;
    std::string dbName;
    std::string dbUser;
    std::string dbPassword;
    std::string dbConnectionString;
    bool runMigrations = true;

    // Server
    std::string serverHost;
    int serverPort;

    // Telegram
    bool telegramEnabled;
    std::string telegramToken;

    // Simulation
    bool simulationEnabled;
    int simulationDeviceCount;
    int simulationUpdateIntervalMs;

    // НОВОЕ: Конфигурация удаленной БД
    bool remoteDbEnabled = false;
    std::string remoteDbConnectionString;
    int remotePollingIntervalSeconds = 30;
  } runtimeConfig_;

  // Application components
  std::shared_ptr<DatabaseRepository> database_;
  std::shared_ptr<NotificationService> notifier_;
  std::shared_ptr<services::AlertProcessingService> alertService_;
  std::shared_ptr<engine::RuleEngine> ruleEngine_;
  std::unique_ptr<api::TelemetryServer> httpServer_;
  std::unique_ptr<bot::TelegramBotHandler> telegramBot_;
  std::unique_ptr<simulation::DeviceSimulator> deviceSimulator_;

  // НОВОЕ: Поток для периодической проверки удаленной БД
  std::thread pollingThread_;
  std::atomic<bool> pollingRunning_{false};

  // State
  std::atomic<bool> running_{false};
  std::atomic<bool> initialized_{false};
  std::chrono::steady_clock::time_point startTime_;

  // Statistics
  struct Statistics {
    int telemetryProcessed = 0;
    int alertsTriggered = 0;
    int errors = 0;
    int remoteChecks = 0;  // НОВОЕ: количество проверок удаленной БД
  } stats_;
  mutable std::mutex statsMutex_;
};

}  // namespace iot_core::core