// src/core/NotificationService.h
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace iot_core::smtp {
class EmailService;  // Forward declaration
}

namespace iot_core::core {

class NotificationService {
 public:
  explicit NotificationService(const std::string& botToken);
  ~NotificationService();

  // Отправка уведомлений
  void sendTelegramAlert(long chatId, const std::string& deviceId, double value,
                         const std::string& metricType,
                         const std::string& direction);

  void sendTelegramMessage(long chatId, const std::string& message);

  // Групповые уведомления
  void broadcastAlert(const std::vector<long>& chatIds,
                      const std::string& deviceId, double value,
                      const std::string& metricType,
                      const std::string& direction);

  // Проверка доступности
  bool isTelegramAvailable() const;
  bool isEmailAvailable() const;

  // Email тестирование
  bool testEmailConnection();

 private:
  // Методы форматирования
  std::string formatAlertMessage(const std::string& deviceId, double value,
                                 const std::string& metricType,
                                 const std::string& direction) const;

  std::string formatEmailAlert(const std::string& deviceId, double value,
                               const std::string& metricType,
                               const std::string& direction) const;

  std::string getMetricUnit(const std::string& metricType) const;
  std::string getMetricName(const std::string& metricType) const;

  std::string botToken_;
  bool telegramEnabled_ = false;
  std::unique_ptr<smtp::EmailService> emailService_;
};

}  // namespace iot_core::core