// Server.cpp
#include "Server.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>

#include "../utils/Formatter.h"
#include "TelemetryServerImpl.h"

using json = nlohmann::json;

namespace iot_core::api {

TelemetryServer::TelemetryServer(
    std::shared_ptr<core::DatabaseRepository> database,
    std::shared_ptr<services::AlertProcessingService> alertService,
    std::shared_ptr<core::NotificationService> notifier)
    : database_(std::move(database)),
      alertService_(std::move(alertService)),
      notifier_(std::move(notifier)),
      serverImpl_(std::make_unique<TelemetryServerImpl>(
          database_, alertService_, notifier_)) {
  serverImpl_->setup(this);
  statistics_.startTime = std::chrono::steady_clock::now();

  std::cout << "🌐 HTTP сервер инициализирован" << std::endl;
}

TelemetryServer::~TelemetryServer() { stop(); }

void TelemetryServer::start(int port) {
  if (running_) {
    std::cout << "⚠️  Сервер уже запущен" << std::endl;
    return;
  }

  port_ = port;

  std::cout << "🚀 Запуск HTTP сервера на порту " << port_ << "..."
            << std::endl;

  // Запускаем сервер
  if (serverImpl_->listen("0.0.0.0", port_)) {
    running_ = true;
    std::cout << "✅ HTTP сервер запущен" << std::endl;
    std::cout << "📡 Доступен по адресу: http://localhost:" << port_
              << std::endl;
  } else {
    std::cerr << "❌ Не удалось запустить HTTP сервер" << std::endl;
    running_ = false;
  }
}

void TelemetryServer::stop() {
  if (running_) {
    serverImpl_->stop();
    running_ = false;
    std::cout << "🛑 HTTP сервер остановлен" << std::endl;
  }
}

bool TelemetryServer::isRunning() const { return running_; }

std::vector<TelemetryServer::EndpointInfo>
TelemetryServer::getAvailableEndpoints() const {
  return {{"GET", "/health", "Health check"},
          {"GET", "/info", "System information"},
          {"GET", "/telemetry", "Get telemetry data"},
          {"POST", "/telemetry", "Submit telemetry data"},
          {"GET", "/stats", "System statistics"},
          {"POST", "/test/alert", "Send test alert"}};
}

void TelemetryServer::logRequest(const std::string& method,
                                 const std::string& path) const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  statistics_.totalRequests++;

  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);

  if (path != "/health") {  // Не логируем health checks
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S");
    std::cout << "📥 [" << ss.str() << "] " << method << " " << path
              << std::endl;
  }
}

void TelemetryServer::logResponse(int statusCode,
                                  const std::string& path) const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);

  if (statusCode >= 200 && statusCode < 300) {
    statistics_.successfulRequests++;
  } else {
    statistics_.failedRequests++;
  }
}

}  // namespace iot_core::api