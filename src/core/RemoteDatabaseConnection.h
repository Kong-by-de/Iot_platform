#pragma once

#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <string>
#include <vector>

#include "../models/IoTData.h"

namespace iot_core::core {
class RemoteDatabaseConnection {
 public:
  explicit RemoteDatabaseConnection(const std::string& connectionString);
  ~RemoteDatabaseConnection();
  bool connect();
  bool isConnected() const;
  void disconnect();
  std::vector<models::IoTData> getTelemetryData(
      const std::string& deviceId = "", int limit = 10,
      const std::string& timeFrom = "");
  std::vector<models::IoTData> getLatestTelemetryForAllDevices();
  std::vector<models::IoTData> getDeviceTelemetry(const std::string& deviceId,
                                                  int limit = 10);
  bool validateSchema();

 private:
  std::string connectionString_;
  std::unique_ptr<pqxx::connection> connection_;
  mutable std::mutex connectionMutex_;
  void reconnectIfNeeded();
  pqxx::connection& getConnection();
};

}  // namespace iot_core::core