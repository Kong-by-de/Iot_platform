#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/Database.h"
#include "../core/NotificationService.h"

namespace iot_core::services {

class AlertProcessingService {
 public:
  AlertProcessingService(std::shared_ptr<core::DatabaseRepository> database,
                         std::shared_ptr<core::NotificationService> notifier);

  void processTelemetryData(const std::string& deviceId, double temperature,
                            double humidity);

  void checkAllSubscribedDevices();

  // Получение статистики
  struct AlertStatistics {
    int totalAlerts = 0;
    int temperatureAlerts = 0;
    int humidityAlerts = 0;
    int usersNotified = 0;
  };

  AlertStatistics getStatistics() const;
  void resetStatistics();

 private:
  // Вспомогательные методы
  void checkUserAlerts(long userId, const std::string& deviceId,
                       double temperature, double humidity);

  void checkGlobalAlerts(const std::string& deviceId, double temperature,
                         double humidity);

  bool shouldNotify(long userId, const std::string& deviceId,
                    const std::string& alertType, double value);

  void updateStatistics(const std::string& alertType);

  std::vector<std::string> getAllSubscribedDevices();

  std::shared_ptr<core::DatabaseRepository> database_;
  std::shared_ptr<core::NotificationService> notifier_;

  AlertStatistics statistics_;
  mutable std::mutex statisticsMutex_;

  // Кэш для предотвращения спама
  std::unordered_map<std::string, std::chrono::system_clock::time_point>
      alertCache_;
  std::chrono::seconds cacheDuration_ = std::chrono::seconds(300);
  mutable std::mutex cacheMutex_;
};

}  // namespace iot_core::services