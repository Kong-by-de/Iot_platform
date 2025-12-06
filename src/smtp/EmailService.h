// src/smtp/EmailService.h
#pragma once
#include <string>
#include <vector>

#include "EmailConfig.h"

namespace iot_core::smtp {

class EmailService {
 public:
  EmailService();
  ~EmailService();

  // Основной метод отправки email
  bool sendEmail(const std::vector<std::string>& recipients,
                 const std::string& subject, const std::string& body,
                 bool isHtml = false);

  // Метод для отправки оповещений (HTML формат)
  bool sendAlertEmail(const std::string& deviceId, double value,
                      const std::string& metricType,
                      const std::string& direction);

  // Проверка конфигурации
  bool isConfigured() const { return configured_; }

  // Получение получателей оповещений
  std::vector<std::string> getAlertRecipients() const {
    return config_.alertRecipients;
  }

  // Тестирование подключения
  bool testConnection();

 private:
  struct UploadData {
    std::string payload;
    size_t sent = 0;
  };

  static size_t payloadSource(void* ptr, size_t size, size_t nmemb,
                              void* userp);

  EmailConfig config_;
  bool configured_ = false;

  // Форматирование email оповещения (HTML)
  std::string formatAlertBody(const std::string& deviceId, double value,
                              const std::string& metricType,
                              const std::string& direction) const;

  // Вспомогательные методы для форматирования
  static std::string formatDoubleNice(double value);
  static std::string humanMetricName(const std::string& type);
  static std::string humanDirection(const std::string& direction);
  static std::string getEmoji(const std::string& type,
                              const std::string& direction);
};

}  // namespace iot_core::smtp