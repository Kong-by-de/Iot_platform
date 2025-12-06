#include "Database.h"

#include <iostream>
#include <stdexcept>

namespace iot_core::core {

DatabaseRepository::DatabaseRepository(const std::string& connectionString)
    : connectionString_(connectionString) {
  std::cout << "🔧 Создание репозитория базы данных..." << std::endl;
}

DatabaseRepository::~DatabaseRepository() {
  if (connection_ && connection_->is_open()) {
    connection_->close();
    std::cout << "🔌 Соединение с локальной БД закрыто" << std::endl;
  }

  // Удаленная БД закроется автоматически в деструкторе RemoteDatabaseConnection
}

void DatabaseRepository::initialize() {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);

  std::cout << "🔌 Подключение к локальной БД..." << std::endl;
  try {
    connection_ = std::make_unique<pqxx::connection>(connectionString_);

    if (connection_->is_open()) {
      std::cout << "✅ Подключение к локальной БД установлено успешно"
                << std::endl;
    } else {
      throw std::runtime_error("Не удалось открыть соединение с локальной БД");
    }
  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка подключения к локальной БД: " << e.what()
              << std::endl;
    throw;
  }
}

bool DatabaseRepository::isConnected() const {
  return connection_ && connection_->is_open();
}

// НОВЫЕ МЕТОДЫ ДЛЯ УДАЛЕННОЙ БД

void DatabaseRepository::connectToRemoteDatabase(
    const std::string& connectionString) {
  try {
    remoteConnection_ =
        std::make_unique<RemoteDatabaseConnection>(connectionString);
    if (remoteConnection_->connect()) {
      std::cout << "✅ Подключение к удаленной БД установлено" << std::endl;
    } else {
      std::cerr << "❌ Не удалось подключиться к удаленной БД" << std::endl;
      remoteConnection_.reset();
    }
  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка при создании подключения к удаленной БД: "
              << e.what() << std::endl;
    remoteConnection_.reset();
  }
}

bool DatabaseRepository::isRemoteConnected() const {
  return remoteConnection_ && remoteConnection_->isConnected();
}

std::vector<models::IoTData> DatabaseRepository::getRemoteTelemetry(
    const std::string& deviceId, int limit) {
  if (!isRemoteConnected()) {
    std::cerr << "❌ Нет подключения к удаленной БД" << std::endl;
    return {};
  }

  return remoteConnection_->getTelemetryData(deviceId, limit);
}

std::vector<models::IoTData>
DatabaseRepository::getLatestRemoteTelemetryForAllDevices() {
  if (!isRemoteConnected()) {
    std::cerr << "❌ Нет подключения к удаленной БД" << std::endl;
    return {};
  }

  return remoteConnection_->getLatestTelemetryForAllDevices();
}

// НОВЫЙ МЕТОД: получение всех устройств с подписчиками
std::vector<std::string> DatabaseRepository::getAllSubscribedDevices() {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  std::vector<std::string> devices;

  try {
    pqxx::work transaction(getConnection());

    auto result = transaction.exec(
        "SELECT DISTINCT device_id FROM user_devices ORDER BY device_id");

    for (const auto& row : result) {
      devices.push_back(row["device_id"].as<std::string>());
    }

    std::cout << "📱 Найдено " << devices.size() << " устройств с подписчиками"
              << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка получения списка устройств: " << e.what()
              << std::endl;
  }

  return devices;
}

// УДАЛЕН МЕТОД saveTelemetryData - больше не сохраняем телеметрию локально

// ВСЕ ОСТАЛЬНЫЕ МЕТОДЫ ОСТАЮТСЯ БЕЗ ИЗМЕНЕНИЙ (копируем из существующего файла)

pqxx::connection& DatabaseRepository::getConnection() {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);

  if (!connection_ || !connection_->is_open()) {
    std::cout << "🔌 Переподключение к локальной БД..." << std::endl;
    connection_ = std::make_unique<pqxx::connection>(connectionString_);
  }
  return *connection_;
}

void DatabaseRepository::reconnectIfNeeded() {
  if (!isConnected()) {
    std::cout
        << "⚠️  Соединение с локальной БД потеряно, пытаюсь переподключиться..."
        << std::endl;
    initialize();
  }
}

std::vector<models::IoTData> DatabaseRepository::getRecentTelemetry(int limit) {
  // Теперь этот метод возвращает данные из удаленной БД
  return getRemoteTelemetry("", limit);
}

std::vector<models::IoTData> DatabaseRepository::getDeviceTelemetry(
    const std::string& deviceId, int limit) {
  // Теперь этот метод возвращает данные из удаленной БД
  return getRemoteTelemetry(deviceId, limit);
}

void DatabaseRepository::addUserDevice(long chatId,
                                       const std::string& deviceId) {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  try {
    pqxx::work transaction(getConnection());

    transaction.exec_params(
        "INSERT INTO user_devices (chat_id, device_id) VALUES ($1, $2) "
        "ON CONFLICT (chat_id, device_id) DO NOTHING",
        chatId, deviceId);

    transaction.commit();
    std::cout << "📱 Устройство " << deviceId << " привязано к пользователю "
              << chatId << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка привязки устройства: " << e.what() << std::endl;
    throw;
  }
}

void DatabaseRepository::removeUserDevice(long chatId,
                                          const std::string& deviceId) {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  try {
    pqxx::work transaction(getConnection());

    transaction.exec_params(
        "DELETE FROM user_devices WHERE chat_id = $1 AND device_id = $2",
        chatId, deviceId);

    transaction.commit();
    std::cout << "📱 Устройство " << deviceId << " отвязано от пользователя "
              << chatId << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка отвязки устройства: " << e.what() << std::endl;
    throw;
  }
}

std::vector<std::string> DatabaseRepository::getUserDevices(long chatId) {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  std::vector<std::string> devices;

  try {
    pqxx::work transaction(getConnection());

    auto result = transaction.exec_params(
        "SELECT device_id FROM user_devices WHERE chat_id = $1 ORDER BY "
        "created_at DESC",
        chatId);

    for (const auto& row : result) {
      devices.push_back(row["device_id"].as<std::string>());
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка получения устройств пользователя: " << e.what()
              << std::endl;
  }

  return devices;
}

std::vector<long> DatabaseRepository::getDeviceSubscribers(
    const std::string& deviceId) {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  std::vector<long> subscribers;

  try {
    pqxx::work transaction(getConnection());

    auto result = transaction.exec_params(
        "SELECT chat_id FROM user_devices WHERE device_id = $1", deviceId);

    for (const auto& row : result) {
      subscribers.push_back(row["chat_id"].as<long>());
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка получения подписчиков устройства: " << e.what()
              << std::endl;
  }

  return subscribers;
}

void DatabaseRepository::setUserAlert(long chatId,
                                      const models::UserAlert& alert) {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  try {
    pqxx::work transaction(getConnection());

    transaction.exec_params(
        "INSERT INTO user_alerts (chat_id, temp_high_threshold, "
        "temp_low_threshold, "
        "hum_high_threshold, hum_low_threshold, updated_at) "
        "VALUES ($1, $2, $3, $4, $5, CURRENT_TIMESTAMP) "
        "ON CONFLICT (chat_id) DO UPDATE SET "
        "temp_high_threshold = $2, "
        "temp_low_threshold = $3, "
        "hum_high_threshold = $4, "
        "hum_low_threshold = $5, "
        "updated_at = CURRENT_TIMESTAMP",
        chatId, alert.temperatureHighThreshold, alert.temperatureLowThreshold,
        alert.humidityHighThreshold, alert.humidityLowThreshold);

    transaction.commit();
    std::cout << "⚙️  Настройки оповещений обновлены для пользователя " << chatId
              << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка сохранения настроек оповещений: " << e.what()
              << std::endl;
    throw;
  }
}

models::UserAlert DatabaseRepository::getUserAlert(long chatId) {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  models::UserAlert alert;

  try {
    pqxx::work transaction(getConnection());

    auto result = transaction.exec_params(
        "SELECT temp_high_threshold, temp_low_threshold, "
        "hum_high_threshold, hum_low_threshold "
        "FROM user_alerts WHERE chat_id = $1",
        chatId);

    if (!result.empty()) {
      const auto& row = result[0];

      if (!row["temp_high_threshold"].is_null()) {
        alert.temperatureHighThreshold =
            row["temp_high_threshold"].as<double>();
      }

      if (!row["temp_low_threshold"].is_null()) {
        alert.temperatureLowThreshold = row["temp_low_threshold"].as<double>();
      }

      if (!row["hum_high_threshold"].is_null()) {
        alert.humidityHighThreshold = row["hum_high_threshold"].as<double>();
      }

      if (!row["hum_low_threshold"].is_null()) {
        alert.humidityLowThreshold = row["hum_low_threshold"].as<double>();
      }
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка получения настроек оповещений: " << e.what()
              << std::endl;
  }

  return alert;
}

void DatabaseRepository::clearUserAlerts(long chatId) {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  try {
    pqxx::work transaction(getConnection());

    transaction.exec_params("DELETE FROM user_alerts WHERE chat_id = $1",
                            chatId);

    transaction.commit();
    std::cout << "🗑️  Настройки оповещений удалены для пользователя " << chatId
              << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка удаления настроек оповещений: " << e.what()
              << std::endl;
    throw;
  }
}

std::vector<std::pair<long, models::UserAlert>>
DatabaseRepository::getAllActiveAlerts() {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  std::vector<std::pair<long, models::UserAlert>> alerts;

  try {
    pqxx::work transaction(getConnection());

    auto result = transaction.exec(
        "SELECT chat_id, temp_high_threshold, temp_low_threshold, "
        "hum_high_threshold, hum_low_threshold "
        "FROM user_alerts "
        "WHERE temp_high_threshold > 0 OR temp_low_threshold > 0 OR "
        "hum_high_threshold > 0 OR hum_low_threshold > 0");

    for (const auto& row : result) {
      models::UserAlert alert;
      long chatId = row["chat_id"].as<long>();

      if (!row["temp_high_threshold"].is_null()) {
        alert.temperatureHighThreshold =
            row["temp_high_threshold"].as<double>();
      }

      if (!row["temp_low_threshold"].is_null()) {
        alert.temperatureLowThreshold = row["temp_low_threshold"].as<double>();
      }

      if (!row["hum_high_threshold"].is_null()) {
        alert.humidityHighThreshold = row["hum_high_threshold"].as<double>();
      }

      if (!row["hum_low_threshold"].is_null()) {
        alert.humidityLowThreshold = row["hum_low_threshold"].as<double>();
      }

      alerts.emplace_back(chatId, alert);
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка получения активных оповещений: " << e.what()
              << std::endl;
  }

  return alerts;
}

int DatabaseRepository::getTotalRecordsCount() {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  try {
    pqxx::work transaction(getConnection());

    auto result =
        transaction.exec("SELECT COUNT(*) as count FROM user_devices");

    if (!result.empty()) {
      return result[0]["count"].as<int>();
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка получения количества записей: " << e.what()
              << std::endl;
  }

  return 0;
}

int DatabaseRepository::getActiveUsersCount() {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  try {
    pqxx::work transaction(getConnection());

    auto result = transaction.exec(
        "SELECT COUNT(DISTINCT chat_id) as count FROM user_devices");

    if (!result.empty()) {
      return result[0]["count"].as<int>();
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка получения количества пользователей: " << e.what()
              << std::endl;
  }

  return 0;
}

bool DatabaseRepository::deviceExists(const std::string& deviceId) {
  // Теперь проверяем существование устройства в удаленной БД
  if (isRemoteConnected()) {
    auto data = getRemoteTelemetry(deviceId, 1);
    return !data.empty();
  }

  return false;
}

bool DatabaseRepository::userHasDevice(long chatId,
                                       const std::string& deviceId) {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  reconnectIfNeeded();

  try {
    pqxx::work transaction(getConnection());

    auto result = transaction.exec_params(
        "SELECT COUNT(*) as count FROM user_devices WHERE chat_id = $1 AND "
        "device_id = $2",
        chatId, deviceId);

    if (!result.empty()) {
      return result[0]["count"].as<int>() > 0;
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка проверки привязки устройства: " << e.what()
              << std::endl;
  }

  return false;
}

}  // namespace iot_core::core