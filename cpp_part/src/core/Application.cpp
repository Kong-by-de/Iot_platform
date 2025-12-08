#include "Application.h"

#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "../api/Server.h"
#include "../bot/TelegramBotHandler.h"
#include "../core/DatabaseMigrator.h"
#include "../engine/RuleEngine.h"
#include "../services/AlertService.h"
#include "ConfigManager.h"
#include "Database.h"
#include "NotificationService.h"

static iot_core::core::Application* g_appInstance = nullptr;

static void signalHandler(int signal) {
  if (g_appInstance) {
    std::cout << "\n🛑 Received signal " << signal << ", shutting down..."
              << std::endl;
    g_appInstance->shutdown();
  }
}

namespace iot_core::core {

Application::Application() {
  g_appInstance = this;
  startTime_ = std::chrono::steady_clock::now();
}

Application::~Application() {
  cleanup();
  g_appInstance = nullptr;
}

bool Application::initialize() {
  try {
    printWelcomeBanner();

    loadConfiguration();

    initializeComponents();

    initialized_ = true;
    std::cout << "\n✅ All components initialized successfully!" << std::endl;

    return true;

  } catch (const std::exception& e) {
    std::cerr << "\n❌ Application initialization failed: " << e.what()
              << std::endl;
    return false;
  }
}

void Application::run() {
  if (!initialized_) {
    std::cerr << "❌ Application not initialized. Call initialize() first."
              << std::endl;
    return;
  }

  running_ = true;

  setupSignalHandlers();

  std::cout << "\n🚀 Starting IoT Platform..." << std::endl;

  if (httpServer_) {
    try {
      httpServer_->start(runtimeConfig_.serverPort);
      std::cout << "   🌐 HTTP server started on port "
                << runtimeConfig_.serverPort << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "   ❌ HTTP server failed: " << e.what() << std::endl;
    }
  }

  if (telegramBot_ && runtimeConfig_.telegramEnabled &&
      !runtimeConfig_.telegramToken.empty()) {
    try {
      telegramBot_->startPolling(runtimeConfig_.telegramToken);
      std::cout << "   🤖 Telegram bot started" << std::endl;

      setupTestUser(1067054337);

    } catch (const std::exception& e) {
      std::cerr << "   ❌ Telegram bot failed: " << e.what() << std::endl;
    }
  } else {
    std::cout << "   ⚠️  Telegram bot disabled" << std::endl;
  }
  if (runtimeConfig_.remoteDbEnabled &&
      !runtimeConfig_.remoteDbConnectionString.empty()) {
    std::cout << "   🔌 Подключение к удаленной БД сокомандника..."
              << std::endl;
    database_->connectToRemoteDatabase(runtimeConfig_.remoteDbConnectionString);

    if (database_->isRemoteConnected()) {
      startRemotePolling(runtimeConfig_.remotePollingIntervalSeconds);
      std::cout
          << "   🔄 Периодическая проверка удаленной БД запущена (интервал: "
          << runtimeConfig_.remotePollingIntervalSeconds << " сек)"
          << std::endl;
    } else {
      std::cout << "   ⚠️  Не удалось подключиться к удаленной БД" << std::endl;
    }
  } else {
    std::cout << "   ⚠️  Удаленная БД отключена в конфигурации" << std::endl;
  }

  std::cout << "\n🔄 IoT Platform is running. Press Ctrl+C to stop.\n"
            << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

  runMainLoop();
  shutdown();
}

void Application::setupTestUser(long telegramId) {
  if (!database_ || !database_->isConnected()) {
    std::cerr << "❌ Database not connected, skipping test user setup"
              << std::endl;
    return;
  }

  try {
    std::cout << "\n🔧 Setting up test user " << telegramId << "..."
              << std::endl;

    // 2. Устанавливаем правила оповещений по умолчанию
    models::UserAlert alert;
    alert.temperatureHighThreshold = 28.0;  // Оповещать если >28°C
    alert.temperatureLowThreshold = 15.0;   // Оповещать если <15°C
    alert.humidityHighThreshold = 70.0;     // Оповещать если влажность >70%
    alert.humidityLowThreshold = 30.0;      // Оповещать если влажность <30%

    database_->setUserAlert(telegramId, alert);
    std::cout << "   ⚙️ Default alerts set:" << std::endl;
    std::cout << "     • Temp > " << alert.temperatureHighThreshold << "°C"
              << std::endl;
    std::cout << "     • Temp < " << alert.temperatureLowThreshold << "°C"
              << std::endl;
    std::cout << "     • Hum > " << alert.humidityHighThreshold << "%"
              << std::endl;
    std::cout << "     • Hum < " << alert.humidityLowThreshold << "%"
              << std::endl;

    // 3. Добавляем тестовое устройство
    std::string testDeviceId = "test_device";
    try {
      database_->addUserDevice(telegramId, testDeviceId);
      std::cout << "   🧪 Test device " << testDeviceId << " added"
                << std::endl;
    } catch (...) {
      // Уже существует
    }

    std::cout << "✅ Test user setup complete for Telegram ID: " << telegramId
              << std::endl;
    std::cout
        << "   Теперь уведомления будут приходить при срабатывании правил!"
        << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "❌ Error setting up test user: " << e.what() << std::endl;
  }
}

// Запуск периодической проверки удаленной БД
void Application::startRemotePolling(int intervalSeconds) {
  if (pollingRunning_) {
    return;
  }

  pollingRunning_ = true;
  pollingThread_ = std::thread([this, intervalSeconds]() {
    std::cout << "🔄 Поток проверки удаленной БД запущен (интервал: "
              << intervalSeconds << " секунд)" << std::endl;

    int checkCount = 0;

    while (pollingRunning_ && running_) {
      try {
        checkCount++;

        std::cout << "\n🔄 Проверка #" << checkCount
                  << " данных из удаленной БД..." << std::endl;

        // Проверяем все устройства
        alertService_->checkAllSubscribedDevices();

        {
          std::lock_guard<std::mutex> lock(statsMutex_);
          stats_.remoteChecks++;
        }

        std::cout << "✅ Проверка #" << checkCount << " завершена" << std::endl;

        // Ждем перед следующей проверкой
        for (int i = 0; i < intervalSeconds && pollingRunning_; i++) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }

      } catch (const std::exception& e) {
        std::cerr << "❌ Ошибка в потоке проверки удаленной БД: " << e.what()
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
      }
    }

    std::cout << "🛑 Поток проверки удаленной БД остановлен" << std::endl;
  });
}

// Остановка периодической проверки
void Application::stopRemotePolling() {
  pollingRunning_ = false;
  if (pollingThread_.joinable()) {
    pollingThread_.join();
  }
}

void Application::shutdown() {
  if (!running_) return;

  running_ = false;

  std::cout << "\n🛑 Shutting down IoT Platform..." << std::endl;

  // Останавливаем периодическую проверку удаленной БД
  stopRemotePolling();
  if (telegramBot_) {
    telegramBot_->stop();
    std::cout << "   • Telegram bot stopped" << std::endl;
  }

  if (httpServer_) {
    httpServer_->stop();
    std::cout << "   • HTTP server stopped" << std::endl;
  }

  std::cout << "\n👋 IoT Platform shutdown complete.\n" << std::endl;
}

void Application::printWelcomeBanner() const {
  std::cout << R"(
╔══════════════════════════════════════════════════════╗
║                 IoT CORE PLATFORM                    ║
║      Temperature & Humidity Monitoring System        ║
╚══════════════════════════════════════════════════════╝
)" << std::endl;
}

void Application::loadConfiguration() {
  std::cout << "🔧 Loading configuration..." << std::endl;

  auto& configMgr = ConfigManager::instance();
  configMgr.load();

  auto dbConfig = configMgr.getDatabaseConfig();
  runtimeConfig_.dbHost = dbConfig.host;
  runtimeConfig_.dbPort = dbConfig.port;
  runtimeConfig_.dbName = dbConfig.name;
  runtimeConfig_.dbUser = dbConfig.user;
  runtimeConfig_.dbPassword = dbConfig.password;
  runtimeConfig_.dbConnectionString = dbConfig.connectionString;

  auto serverConfig = configMgr.getServerConfig();
  runtimeConfig_.serverHost = serverConfig.host;
  runtimeConfig_.serverPort = serverConfig.port;

  auto telegramConfig = configMgr.getTelegramConfig();
  runtimeConfig_.telegramEnabled = telegramConfig.enabled;
  runtimeConfig_.telegramToken = telegramConfig.token;

  auto remoteConfig = configMgr.getRemoteDatabaseConfig();
  runtimeConfig_.remoteDbEnabled = remoteConfig.enabled;
  runtimeConfig_.remoteDbConnectionString = remoteConfig.connectionString;
  runtimeConfig_.remotePollingIntervalSeconds =
      remoteConfig.pollingIntervalSeconds;

  runtimeConfig_.runMigrations = configMgr.getBool("RUN_MIGRATIONS", true);

  std::cout << "   📊 Configuration loaded" << std::endl;
  std::cout << "   • Локальная БД: " << runtimeConfig_.dbHost << ":"
            << runtimeConfig_.dbPort << "/" << runtimeConfig_.dbName
            << std::endl;
  std::cout << "   • Сервер: " << runtimeConfig_.serverHost << ":"
            << runtimeConfig_.serverPort << std::endl;
  std::cout << "   • Telegram: "
            << (runtimeConfig_.telegramEnabled ? "enabled" : "disabled")
            << std::endl;
  std::cout << "   • Run Migrations: "
            << (runtimeConfig_.runMigrations ? "yes" : "no") << std::endl;
  std::cout << "   • Удаленная БД: "
            << (runtimeConfig_.remoteDbEnabled ? "enabled" : "disabled")
            << " (интервал: " << runtimeConfig_.remotePollingIntervalSeconds
            << " сек)" << std::endl;
}

void Application::initializeComponents() {
  std::cout << "\n🔧 Initializing components..." << std::endl;

  std::cout << "   1. 📁 Database... ";
  initializeDatabase();
  std::cout << "✅" << std::endl;

  std::cout << "   2. 🔔 Notification Service... ";
  initializeNotificationService();
  std::cout << "✅" << std::endl;

  std::cout << "   3. ⚙️  Rule Engine & Alerts... ";
  initializeRuleEngine();
  std::cout << "✅" << std::endl;

  std::cout << "   4. 🌐 HTTP Server... ";
  initializeHttpServer();
  std::cout << "✅" << std::endl;

  std::cout << "   5. 🤖 Telegram Bot... ";
  initializeTelegramBot();
  std::cout << "✅" << std::endl;
}

void Application::initializeDatabase() {
  // Используем connection string если она есть, иначе строим из параметров
  std::string connStr = runtimeConfig_.dbConnectionString;

  if (connStr.empty()) {
    connStr = "host=" + runtimeConfig_.dbHost +
              " port=" + std::to_string(runtimeConfig_.dbPort) +
              " dbname=" + runtimeConfig_.dbName +
              " user=" + runtimeConfig_.dbUser +
              " password=" + runtimeConfig_.dbPassword;
  }

  // Сначала запускаем миграции, если включено
  if (runtimeConfig_.runMigrations) {
    std::cout << "\n   1.1 📋 Проверка миграций базы данных..." << std::endl;
    try {
      DatabaseMigrator migrator(connStr);
      if (!migrator.runMigrations()) {
        std::cout << "   ⚠️  Предупреждение: возможны проблемы с миграциями БД"
                  << std::endl;
        std::cout << "   ℹ️  Проверьте вручную: DATABASE_URL=\"" << connStr
                  << "\" dbmate status" << std::endl;
      }
    } catch (const std::exception& e) {
      std::cerr << "   ❌ Ошибка миграций: " << e.what() << std::endl;
      std::cerr << "   ℹ️  Продолжаю без миграций..." << std::endl;
    }
  }

  // Затем инициализируем репозиторий
  database_ = std::make_shared<DatabaseRepository>(connStr);
  database_->initialize();
}

void Application::initializeNotificationService() {
  notifier_ =
      std::make_shared<NotificationService>(runtimeConfig_.telegramToken);
  if (notifier_->isEmailAvailable()) {
    std::cout << "   📧 Testing email connection..." << std::endl;
    bool emailOk = notifier_->testEmailConnection();
    std::cout << "   " << (emailOk ? "✅" : "❌") << " Email connection "
              << (emailOk ? "successful" : "failed") << std::endl;
  }
}

void Application::initializeRuleEngine() {
  alertService_ =
      std::make_shared<services::AlertProcessingService>(database_, notifier_);

  ruleEngine_ = std::make_shared<engine::RuleEngine>(database_, alertService_);
  ruleEngine_->setupDefaultRules();
}

void Application::initializeHttpServer() {
  httpServer_ = std::make_unique<api::TelemetryServer>(database_, alertService_,
                                                       notifier_);
}

void Application::initializeTelegramBot() {
  if (runtimeConfig_.telegramEnabled && !runtimeConfig_.telegramToken.empty()) {
    telegramBot_ = std::make_unique<bot::TelegramBotHandler>(
        database_, notifier_, alertService_);
  }
}

void Application::setupSignalHandlers() {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
}

void Application::runMainLoop() {
  auto lastStatusTime = std::chrono::steady_clock::now();
  const auto statusInterval = std::chrono::seconds(30);

  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (httpServer_ && !httpServer_->isRunning()) {
      std::cout << "⚠️  HTTP server not running, attempting restart..."
                << std::endl;
      try {
        httpServer_->start(runtimeConfig_.serverPort);
      } catch (...) {
      }
    }

    auto now = std::chrono::steady_clock::now();
    if (now - lastStatusTime >= statusInterval) {
      printStatusReport();
      lastStatusTime = now;
    }
  }
}

void Application::printStatusReport() const {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - startTime_);

  std::cout << "\n📈 System Status ("
            << std::put_time(std::localtime(&time), "%H:%M:%S") << ")\n"
            << "   • Uptime: " << uptime.count() << " seconds\n";

  std::cout << "   • Локальная БД: "
            << (database_ && database_->isConnected() ? "✅ connected"
                                                      : "❌ disconnected")
            << "\n";

  std::cout << "   • Удаленная БД: "
            << (database_ && database_->isRemoteConnected() ? "✅ connected"
                                                            : "❌ disconnected")
            << "\n";

  std::cout << "   • HTTP Server: "
            << (httpServer_ && httpServer_->isRunning() ? "✅ running"
                                                        : "❌ stopped")
            << "\n";

  if (telegramBot_) {
    std::cout << "   • Telegram Bot: "
              << (telegramBot_->isRunning() ? "✅ active" : "❌ inactive")
              << "\n";
  }

  {
    std::lock_guard<std::mutex> lock(statsMutex_);
    std::cout << "   • Telemetry Processed: " << stats_.telemetryProcessed
              << "\n";
    std::cout << "   • Remote DB Checks: " << stats_.remoteChecks << "\n";

    if (alertService_) {
      auto alertStats = alertService_->getStatistics();
      std::cout << "   • Alerts Sent: " << alertStats.totalAlerts << "\n";
    }
  }

  // Правила
  if (ruleEngine_) {
    auto ruleStats = ruleEngine_->getStatistics();
    std::cout << "   • Rules Triggered: " << ruleStats.rulesTriggered << "\n";
  }
}

void Application::cleanup() { shutdown(); }
}  // namespace iot_core::core