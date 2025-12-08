#include "AlertService.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>

#include "../utils/Formatter.h"

namespace iot_core::services {

AlertProcessingService::AlertProcessingService(
    std::shared_ptr<core::DatabaseRepository> database,
    std::shared_ptr<core::NotificationService> notifier)
    : database_(std::move(database)), notifier_(std::move(notifier)) {
  std::cout << "🔔 Alert Service initialized" << std::endl;
}

void AlertProcessingService::processTelemetryData(const std::string& deviceId,
                                                  double temperature,
                                                  double humidity) {
  std::cout << "📊 Processing data for " << deviceId << " (T=" << temperature
            << ", H=" << humidity << ")" << std::endl;

  // Получаем всех подписчиков устройства
  auto subscribers = database_->getDeviceSubscribers(deviceId);

  // Проверяем для каждого подписчика
  for (long userId : subscribers) {
    checkUserAlerts(userId, deviceId, temperature, humidity);
  }

  // Также проверяем общие правила
  checkGlobalAlerts(deviceId, temperature, humidity);
}

// НОВЫЙ МЕТОД: Периодическая проверка всех устройств
void AlertProcessingService::checkAllSubscribedDevices() {
  if (!database_->isRemoteConnected()) {
    std::cout << "⚠️  Удаленная БД не подключена, пропускаем проверку"
              << std::endl;
    return;
  }

  // Получаем все устройства с подписчиками
  auto devices = getAllSubscribedDevices();

  if (devices.empty()) {
    std::cout << "📭 Нет устройств с подписчиками для проверки" << std::endl;
    return;
  }

  std::cout << "🔍 Проверка " << devices.size()
            << " устройств из удаленной БД..." << std::endl;

  for (const auto& deviceId : devices) {
    try {
      // Получаем последние данные из удаленной БД
      auto telemetryData = database_->getRemoteTelemetry(deviceId, 1);

      if (telemetryData.empty()) {
        std::cout << "   📭 Нет данных для устройства " << deviceId
                  << std::endl;
        continue;
      }

      const auto& data = telemetryData[0];

      // Логируем полученные данные
      std::cout << "   📊 Устройство " << deviceId << ": "
                << "T=" << std::fixed << std::setprecision(1)
                << data.temperature << "°C, "
                << "H=" << data.humidity << "%, "
                << "время: " << data.timestamp << std::endl;

      // Получаем подписчиков устройства
      auto subscribers = database_->getDeviceSubscribers(deviceId);

      if (subscribers.empty()) {
        std::cout << "   👤 Нет подписчиков для устройства " << deviceId
                  << std::endl;
        continue;
      }

      std::cout << "   👥 Подписчиков: " << subscribers.size() << std::endl;

      // Проверяем оповещения для каждого подписчика
      for (long userId : subscribers) {
        checkUserAlerts(userId, deviceId, data.temperature, data.humidity);
      }

    } catch (const std::exception& e) {
      std::cerr << "❌ Ошибка при проверке устройства " << deviceId << ": "
                << e.what() << std::endl;
    }
  }
}

// НОВЫЙ МЕТОД: Получение всех устройств с подписчиками
std::vector<std::string> AlertProcessingService::getAllSubscribedDevices() {
  return database_->getAllSubscribedDevices();
}

void AlertProcessingService::checkUserAlerts(long userId,
                                             const std::string& deviceId,
                                             double temperature,
                                             double humidity) {
  // Получаем настройки пользователя
  auto alert = database_->getUserAlert(userId);

  if (!alert.hasAnyAlert()) {
    return;  // Нет настроек
  }

  // Проверяем температуру
  if (alert.temperatureHighThreshold > 0 &&
      temperature > alert.temperatureHighThreshold) {
    if (shouldNotify(userId, deviceId, "temp_high", temperature)) {
      std::cout << "🔥 Temperature alert for user " << userId << ": "
                << temperature << " > " << alert.temperatureHighThreshold
                << " (sending Telegram + Email)" << std::endl;

      notifier_->sendTelegramAlert(userId, deviceId, temperature, "temperature",
                                   "above");

      updateStatistics("temperature");
    }
  }

  if (alert.temperatureLowThreshold > 0 &&
      temperature < alert.temperatureLowThreshold) {
    if (shouldNotify(userId, deviceId, "temp_low", temperature)) {
      std::cout << "❄️ Temperature alert for user " << userId << ": "
                << temperature << " < " << alert.temperatureLowThreshold
                << std::endl;

      notifier_->sendTelegramAlert(userId, deviceId, temperature, "temperature",
                                   "below");

      updateStatistics("temperature");
    }
  }

  // Проверяем влажность
  if (alert.humidityHighThreshold > 0 &&
      humidity > alert.humidityHighThreshold) {
    if (shouldNotify(userId, deviceId, "hum_high", humidity)) {
      std::cout << "💦 Humidity alert for user " << userId << ": " << humidity
                << " > " << alert.humidityHighThreshold << std::endl;

      notifier_->sendTelegramAlert(userId, deviceId, humidity, "humidity",
                                   "above");

      updateStatistics("humidity");
    }
  }

  if (alert.humidityLowThreshold > 0 && humidity < alert.humidityLowThreshold) {
    if (shouldNotify(userId, deviceId, "hum_low", humidity)) {
      std::cout << "🏜️ Humidity alert for user " << userId << ": " << humidity
                << " < " << alert.humidityLowThreshold << std::endl;

      notifier_->sendTelegramAlert(userId, deviceId, humidity, "humidity",
                                   "below");

      updateStatistics("humidity");
    }
  }
}

void AlertProcessingService::checkGlobalAlerts(const std::string& deviceId,
                                               double temperature,
                                               double humidity) {
  // Глобальные правила (например, для администраторов)

  // Очень высокая температура
  if (temperature > 40.0) {
    std::cout << "🚨 CRITICAL: Very high temperature: " << temperature << "°C"
              << std::endl;
    // Можно отправить email администратору
  }

  // Очень низкая температура
  if (temperature < 0.0) {
    std::cout << "⚠️ CRITICAL: Very low temperature: " << temperature << "°C"
              << std::endl;
  }

  // Экстремальная влажность
  if (humidity > 90.0 || humidity < 10.0) {
    std::cout << "⚠️ Extreme humidity: " << humidity << "%" << std::endl;
  }
}

bool AlertProcessingService::shouldNotify(long userId,
                                          const std::string& deviceId,
                                          const std::string& alertType,
                                          double value) {
  // Простая защита от спама - проверяем кэш
  std::lock_guard<std::mutex> lock(cacheMutex_);

  auto now = std::chrono::system_clock::now();
  std::string cacheKey =
      std::to_string(userId) + "_" + deviceId + "_" + alertType;

  // Удаляем старые записи
  auto it = alertCache_.begin();
  while (it != alertCache_.end()) {
    if (now - it->second > cacheDuration_) {
      it = alertCache_.erase(it);
    } else {
      ++it;
    }
  }

  // Проверяем, было ли недавно такое оповещение
  if (alertCache_.find(cacheKey) != alertCache_.end()) {
    std::cout << "⚠️ Skipping duplicate alert: " << cacheKey << std::endl;
    return false;
  }

  // Добавляем в кэш
  alertCache_[cacheKey] = now;
  return true;
}

void AlertProcessingService::updateStatistics(const std::string& alertType) {
  std::lock_guard<std::mutex> lock(statisticsMutex_);

  statistics_.totalAlerts++;
  statistics_.usersNotified++;

  if (alertType == "temperature") {
    statistics_.temperatureAlerts++;
  } else if (alertType == "humidity") {
    statistics_.humidityAlerts++;
  }
}

AlertProcessingService::AlertStatistics AlertProcessingService::getStatistics()
    const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  return statistics_;
}

void AlertProcessingService::resetStatistics() {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  statistics_ = AlertStatistics{};
}

}  // namespace iot_core::services