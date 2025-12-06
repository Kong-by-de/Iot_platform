// TelemetryServerImpl.cpp
#include "TelemetryServerImpl.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "../utils/Formatter.h"
#include "Server.h"

using json = nlohmann::json;

namespace iot_core::api {

// ==================== TelemetryServerImpl ====================

TelemetryServerImpl::TelemetryServerImpl(
    std::shared_ptr<core::DatabaseRepository> database,
    std::shared_ptr<services::AlertProcessingService> alertService,
    std::shared_ptr<core::NotificationService> notifier)
    : database_(std::move(database)),
      alertService_(std::move(alertService)),
      notifier_(std::move(notifier)),
      server_(std::make_unique<httplib::Server>()) {
  setupCors();
  setupRoutes();
}

void TelemetryServerImpl::setup(TelemetryServer* owner) { owner_ = owner; }

bool TelemetryServerImpl::listen(const std::string& host, int port) {
  try {
    host_ = host;
    port_ = port;

    std::cout << "🌐 Starting HTTP server on " << host << ":" << port << "..."
              << std::endl;

    // Запускаем сервер в отдельном потоке
    serverThread_ = std::thread([this, host, port]() {
      try {
        std::cout << "🚀 HTTP server thread started" << std::endl;

        // Запускаем прослушивание
        std::cout << "📡 Listening on " << host << ":" << port << "..."
                  << std::endl;
        server_->listen(host.c_str(), port);

      } catch (const std::exception& e) {
        std::cerr << "❌ HTTP server thread exception: " << e.what()
                  << std::endl;
      }
      std::cout << "🛑 HTTP server thread stopped" << std::endl;
    });

    // Даем время на запуск
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    return true;

  } catch (const std::exception& e) {
    std::cerr << "❌ Failed to start HTTP server: " << e.what() << std::endl;
    return false;
  }
}

void TelemetryServerImpl::stop() {
  if (server_) {
    server_->stop();
  }

  if (serverThread_.joinable()) {
    serverThread_.join();
  }

  std::cout << "✅ HTTP server stopped" << std::endl;
}

bool TelemetryServerImpl::isListening() const {
  return true;  // Упрощенная проверка
}

// ==================== Приватные методы ====================

void TelemetryServerImpl::setupCors() {
  server_->set_pre_routing_handler(
      [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods",
                       "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers",
                       "Content-Type, Authorization");
        res.set_header("Access-Control-Max-Age", "86400");

        if (req.method == "OPTIONS") {
          res.status = 200;
          return httplib::Server::HandlerResponse::Handled;
        }

        // Логирование запроса
        if (owner_) {
          owner_->logRequest(req.method, req.path);
        }

        return httplib::Server::HandlerResponse::Unhandled;
      });

  server_->set_post_routing_handler(
      [this](const httplib::Request& req, httplib::Response& res) {
        if (owner_) {
          owner_->logResponse(res.status, req.path);
        }
      });
}

void TelemetryServerImpl::setupRoutes() {
  // Health check
  server_->Get("/health", [this](const httplib::Request& req,
                                 httplib::Response& res) {
    json response = {
        {"status", "healthy"},
        {"service", "iot_core"},
        {"version", "1.0.0"},
        {"timestamp", getCurrentTimestamp()},
        {"database", database_->isConnected() ? "connected" : "disconnected"}};

    res.set_content(response.dump(), "application/json");
  });

  // System info
  server_->Get(
      "/info", [this](const httplib::Request& req, httplib::Response& res) {
        json response = {{"system", "IoT Core Platform"},
                         {"version", "1.0.0"},
                         {"endpoints",
                          {{"GET /health", "Health check"},
                           {"GET /info", "System information"},
                           {"GET /telemetry", "Get telemetry data"},
                           {"POST /telemetry", "Submit telemetry data"},
                           {"GET /stats", "System statistics"}}}};

        res.set_content(response.dump(), "application/json");
      });

  // Submit telemetry data
  server_->Post("/telemetry", [this](const httplib::Request& req,
                                     httplib::Response& res) {
    try {
      auto data = json::parse(req.body);

      // Валидация
      if (!data.contains("device_id") || !data.contains("temperature") ||
          !data.contains("humidity")) {
        res.status = 400;
        res.set_content("Missing required fields", "text/plain");
        return;
      }

      std::string deviceId = data["device_id"];
      double temperature = data["temperature"];
      double humidity = data["humidity"];

      // Обработка данных
      alertService_->processTelemetryData(deviceId, temperature, humidity);

      json response = {
          {"status", "success"},   {"message", "Telemetry data processed"},
          {"device_id", deviceId}, {"temperature", temperature},
          {"humidity", humidity},  {"timestamp", getCurrentTimestamp()}};

      res.status = 200;
      res.set_content(response.dump(), "application/json");

    } catch (const json::parse_error& e) {
      res.status = 400;
      res.set_content("Invalid JSON", "text/plain");
    } catch (const std::exception& e) {
      res.status = 500;
      res.set_content("Internal server error", "text/plain");
    }
  });

  // Get recent telemetry
  server_->Get("/telemetry",
               [this](const httplib::Request& req, httplib::Response& res) {
                 try {
                   int limit = 10;
                   if (req.has_param("limit")) {
                     limit = std::stoi(req.get_param_value("limit"));
                     limit = std::clamp(limit, 1, 100);
                   }

                   auto data = database_->getRecentTelemetry(limit);
                   json response = json::array();

                   for (const auto& item : data) {
                     response.push_back({{"id", item.id},
                                         {"device_id", item.deviceId},
                                         {"temperature", item.temperature},
                                         {"humidity", item.humidity},
                                         {"timestamp", item.timestamp}});
                   }

                   res.set_content(response.dump(), "application/json");

                 } catch (const std::exception& e) {
                   res.status = 500;
                   res.set_content("Error", "text/plain");
                 }
               });

  // Statistics endpoint
  server_->Get("/stats", [this](const httplib::Request& req,
                                httplib::Response& res) {
    auto stats = alertService_->getStatistics();

    json response = {{"system_statistics",
                      {{"database_records", database_->getTotalRecordsCount()},
                       {"active_users", database_->getActiveUsersCount()}}},
                     {"alert_statistics",
                      {{"total_alerts", stats.totalAlerts},
                       {"temperature_alerts", stats.temperatureAlerts},
                       {"humidity_alerts", stats.humidityAlerts},
                       {"users_notified", stats.usersNotified}}},
                     {"timestamp", getCurrentTimestamp()}};

    res.set_content(response.dump(), "application/json");
  });

  // Test endpoint
  server_->Post("/test/alert", [this](const httplib::Request& req,
                                      httplib::Response& res) {
    try {
      auto data = json::parse(req.body);
      std::string deviceId = data.value("device_id", "test_device");
      double temperature = data.value("temperature", 35.0);
      double humidity = data.value("humidity", 80.0);

      // Имитируем оповещение
      alertService_->processTelemetryData(deviceId, temperature, humidity);

      json response = {{"status", "success"},
                       {"message", "Test alert sent"},
                       {"device_id", deviceId},
                       {"temperature", temperature},
                       {"humidity", humidity}};

      res.set_content(response.dump(), "application/json");

    } catch (const std::exception& e) {
      res.status = 400;
      res.set_content("Error", "text/plain");
    }
  });
}

std::string TelemetryServerImpl::getCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

}  // namespace iot_core::api