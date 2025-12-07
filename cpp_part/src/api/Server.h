// src/api/Server.h
#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../core/Database.h"
#include "../core/NotificationService.h"
#include "../services/AlertService.h"

namespace iot_core::api {

// Forward declaration приватной реализации
class TelemetryServerImpl;

class TelemetryServer {
 public:
  TelemetryServer(
      std::shared_ptr<core::DatabaseRepository> database,
      std::shared_ptr<services::AlertProcessingService> alertService,
      std::shared_ptr<core::NotificationService> notifier);

  ~TelemetryServer();

  void start(int port = 8080);
  void stop();
  bool isRunning() const;

  // API endpoints information
  struct EndpointInfo {
    std::string method;
    std::string path;
    std::string description;
  };

  std::vector<EndpointInfo> getAvailableEndpoints() const;

  // Методы для логирования
  void logRequest(const std::string& method, const std::string& path) const;
  void logResponse(int statusCode, const std::string& path) const;

 private:
  std::shared_ptr<core::DatabaseRepository> database_;
  std::shared_ptr<services::AlertProcessingService> alertService_;
  std::shared_ptr<core::NotificationService> notifier_;

  std::unique_ptr<TelemetryServerImpl> serverImpl_;

  std::atomic<bool> running_{false};
  int port_ = 8080;

  // Статистика сервера
  struct ServerStatistics {
    int totalRequests = 0;
    int successfulRequests = 0;
    int failedRequests = 0;
    std::chrono::steady_clock::time_point startTime;
  };

  mutable ServerStatistics statistics_;
  mutable std::mutex statisticsMutex_;
};

}  // namespace iot_core::api