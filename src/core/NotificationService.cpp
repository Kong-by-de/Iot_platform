// src/core/NotificationService.cpp
#include "NotificationService.h"

#include <cpr/cpr.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "../smtp/EmailService.h"
#include "../utils/Formatter.h"

namespace iot_core::core {

NotificationService::NotificationService(const std::string& botToken)
    : botToken_(botToken) {
  telegramEnabled_ = !botToken.empty();

  // Инициализация email службы
  try {
    emailService_ = std::make_unique<smtp::EmailService>();

    if (emailService_->isConfigured()) {
      std::cout << "📧 Email notifications enabled" << std::endl;

      // Логируем получателей
      auto recipients = emailService_->getAlertRecipients();
      if (!recipients.empty()) {
        std::cout << "   📬 Alert recipients:" << std::endl;
        for (size_t i = 0; i < recipients.size(); ++i) {
          std::cout << "      " << (i + 1) << ". " << recipients[i]
                    << std::endl;
        }
      }
    } else {
      std::cout << "⚠️  Email notifications disabled (not configured)"
                << std::endl;
    }
  } catch (const std::exception& e) {
    std::cerr << "❌ Failed to initialize EmailService: " << e.what()
              << std::endl;
    emailService_.reset();
  }

  if (telegramEnabled_) {
    std::cout << "🤖 Telegram notifications enabled" << std::endl;
  } else {
    std::cout << "⚠️  Telegram notifications disabled (no token)" << std::endl;
  }
}

NotificationService::~NotificationService() {
  // Деструктор для cleanup
}

bool NotificationService::isTelegramAvailable() const {
  return telegramEnabled_;
}

bool NotificationService::isEmailAvailable() const {
  return emailService_ && emailService_->isConfigured();
}

void NotificationService::sendTelegramAlert(long chatId,
                                            const std::string& deviceId,
                                            double value,
                                            const std::string& metricType,
                                            const std::string& direction) {
  // Отправляем Telegram оповещение
  if (telegramEnabled_) {
    std::cout << "🔔 Sending Telegram alert to " << chatId << " for "
              << deviceId << std::endl;

    std::string message = utils::Formatter::formatAlertMessage(
        deviceId, value, metricType, direction);
    sendTelegramMessage(chatId, message);
  }

  // Отправляем email оповещение
  if (isEmailAvailable()) {
    std::cout << "📧 Sending email alert for device " << deviceId << std::endl;

    try {
      bool emailSent =
          emailService_->sendAlertEmail(deviceId, value, metricType, direction);
      if (emailSent) {
        std::cout << "✅ Email alert sent successfully" << std::endl;
      } else {
        std::cout << "⚠️  Email alert failed to send" << std::endl;
      }
    } catch (const std::exception& e) {
      std::cerr << "❌ Error sending email alert: " << e.what() << std::endl;
    }
  }
}

void NotificationService::sendTelegramMessage(long chatId,
                                              const std::string& message) {
  if (!telegramEnabled_ || message.empty()) {
    return;
  }

  try {
    nlohmann::json payload;
    payload["chat_id"] = chatId;
    payload["text"] = message;
    payload["parse_mode"] = "HTML";

    std::string url =
        "https://api.telegram.org/bot" + botToken_ + "/sendMessage";

    cpr::Response response = cpr::Post(
        cpr::Url{url}, cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{payload.dump()}, cpr::Timeout{5000});

    if (response.status_code == 200) {
      std::cout << "✅ Telegram message sent successfully" << std::endl;
    } else {
      std::cerr << "❌ Failed to send Telegram: " << response.status_code
                << " - " << response.text << std::endl;
    }
  } catch (const std::exception& e) {
    std::cerr << "❌ Telegram send exception: " << e.what() << std::endl;
  }
}

void NotificationService::broadcastAlert(const std::vector<long>& chatIds,
                                         const std::string& deviceId,
                                         double value,
                                         const std::string& metricType,
                                         const std::string& direction) {
  // Telegram broadcast
  if (telegramEnabled_) {
    std::string message = utils::Formatter::formatAlertMessage(
        deviceId, value, metricType, direction);

    for (long chatId : chatIds) {
      sendTelegramMessage(chatId, message);
    }
  }

  // Email broadcast (если доступен)
  if (isEmailAvailable()) {
    std::cout << "📧 Sending broadcast email alert for device " << deviceId
              << std::endl;

    try {
      bool emailSent =
          emailService_->sendAlertEmail(deviceId, value, metricType, direction);
      if (emailSent) {
        std::cout << "✅ Broadcast email sent successfully" << std::endl;
      }
    } catch (const std::exception& e) {
      std::cerr << "❌ Error sending broadcast email: " << e.what()
                << std::endl;
    }
  }
}

std::string NotificationService::formatAlertMessage(
    const std::string& deviceId, double value, const std::string& metricType,
    const std::string& direction) const {
  // Используем Formatter для единого стиля
  return utils::Formatter::formatAlertMessage(deviceId, value, metricType,
                                              direction);
}

std::string NotificationService::formatEmailAlert(
    const std::string& deviceId, double value, const std::string& metricType,
    const std::string& direction) const {
  std::ostringstream oss;
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);

  oss << "IoT Platform Alert\n"
      << "==================\n\n"
      << "Device ID: " << deviceId << "\n"
      << "Metric: " << getMetricName(metricType) << "\n"
      << "Value: " << std::fixed << std::setprecision(1) << value
      << getMetricUnit(metricType) << "\n"
      << "Condition: "
      << (direction == "above" ? "Above threshold" : "Below threshold") << "\n"
      << "Time: " << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
      << "\n\n"
      << "---\n"
      << "This is an automated alert from IoT Platform.\n";

  return oss.str();
}

std::string NotificationService::getMetricUnit(
    const std::string& metricType) const {
  return (metricType == "temperature") ? "°C" : "%";
}

std::string NotificationService::getMetricName(
    const std::string& metricType) const {
  if (metricType == "temperature") {
    return "Temperature";
  } else if (metricType == "humidity") {
    return "Humidity";
  }
  return "Unknown";
}

bool NotificationService::testEmailConnection() {
  if (!isEmailAvailable()) {
    std::cout << "❌ Email service not configured" << std::endl;
    return false;
  }

  std::cout << "🔍 Testing email connection..." << std::endl;
  try {
    return emailService_->testConnection();
  } catch (const std::exception& e) {
    std::cerr << "❌ Email connection test failed: " << e.what() << std::endl;
    return false;
  }
}

}  // namespace iot_core::core