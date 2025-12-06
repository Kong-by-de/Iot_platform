// src/api/TelemetryServerImpl.h
#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <iomanip>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../core/Database.h"
#include "../core/NotificationService.h"
#include "../services/AlertService.h"
#include "httplib.h"

namespace iot_core::api {

class TelemetryServer;  // Forward declaration

class TelemetryServerImpl {
 public:
  TelemetryServerImpl(
      std::shared_ptr<core::DatabaseRepository> database,
      std::shared_ptr<services::AlertProcessingService> alertService,
      std::shared_ptr<core::NotificationService> notifier);

  void setup(TelemetryServer* owner);
  bool listen(const std::string& host, int port);
  void stop();
  bool isListening() const;

 private:
  void setupCors();
  void setupRoutes();
  std::string getCurrentTimestamp() const;

  TelemetryServer* owner_ = nullptr;
  std::shared_ptr<core::DatabaseRepository> database_;
  std::shared_ptr<services::AlertProcessingService> alertService_;
  std::shared_ptr<core::NotificationService> notifier_;
  std::unique_ptr<httplib::Server> server_;
  std::thread serverThread_;
  std::string host_;
  int port_ = 8080;
};

}  // namespace iot_core::api