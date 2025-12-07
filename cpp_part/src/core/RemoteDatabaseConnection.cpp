// src/core/RemoteDatabaseConnection.cpp
#include "RemoteDatabaseConnection.h"

#include <iostream>
#include <sstream>

namespace iot_core::core {

RemoteDatabaseConnection::RemoteDatabaseConnection(
    const std::string& connectionString)
    : connectionString_(connectionString) {
  std::cout << "🔌 Создание подключения к удаленной БД..." << std::endl;
}

RemoteDatabaseConnection::~RemoteDatabaseConnection() { disconnect(); }

bool RemoteDatabaseConnection::connect() {
  std::lock_guard<std::mutex> lock(connectionMutex_);

  try {
    // Если уже подключены, возвращаем true
    if (connection_ && connection_->is_open()) {
      return true;
    }

    std::cout << "🌐 Подключение к удаленной БД: "
              << connectionString_.substr(0,
                                          connectionString_.find("password="))
              << "password=***" << std::endl;

    // Создаем новое подключение
    connection_ = std::make_unique<pqxx::connection>(connectionString_);

    if (connection_->is_open()) {
      std::cout << "✅ Удаленная БД подключена" << std::endl;

      // Проверяем структуру при первом подключении
      validateSchema();

      return true;
    }

    std::cerr << "❌ Не удалось открыть соединение с удаленной БД" << std::endl;
    return false;

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка подключения к удаленной БД: " << e.what()
              << std::endl;
    connection_.reset();
    return false;
  }
}

bool RemoteDatabaseConnection::isConnected() const {
  std::lock_guard<std::mutex> lock(connectionMutex_);
  return connection_ && connection_->is_open();
}

void RemoteDatabaseConnection::disconnect() {
  std::lock_guard<std::mutex> lock(connectionMutex_);
  if (connection_ && connection_->is_open()) {
    connection_->close();
    std::cout << "🔌 Отключено от удаленной БД" << std::endl;
  }
  connection_.reset();
}

pqxx::connection& RemoteDatabaseConnection::getConnection() {
  std::lock_guard<std::mutex> lock(connectionMutex_);

  if (!connection_ || !connection_->is_open()) {
    throw std::runtime_error("Нет подключения к удаленной БД");
  }

  return *connection_;
}

void RemoteDatabaseConnection::reconnectIfNeeded() {
  if (!isConnected()) {
    std::cout << "⚠️  Переподключение к удаленной БД..." << std::endl;
    connect();
  }
}

std::vector<models::IoTData> RemoteDatabaseConnection::getTelemetryData(
    const std::string& deviceId, int limit, const std::string& timeFrom) {
  reconnectIfNeeded();
  std::vector<models::IoTData> results;

  try {
    pqxx::work transaction(getConnection());

    std::string query;
    pqxx::result result;

    if (deviceId.empty()) {
      // Получаем данные для всех устройств
      query =
          "SELECT id, device_id, temperature, humidity, "
          "to_char(timestamp, 'YYYY-MM-DD HH24:MI:SS') as ts "
          "FROM telemetry_data ";

      if (!timeFrom.empty()) {
        query += "WHERE timestamp >= '" + timeFrom + "' ";
      }

      query += "ORDER BY timestamp DESC LIMIT " + std::to_string(limit);

      result = transaction.exec(query);
    } else {
      // Получаем данные для конкретного устройства
      query =
          "SELECT id, device_id, temperature, humidity, "
          "to_char(timestamp, 'YYYY-MM-DD HH24:MI:SS') as ts "
          "FROM telemetry_data WHERE device_id = $1 ";

      if (!timeFrom.empty()) {
        query += "AND timestamp >= '" + timeFrom + "' ";
      }

      query += "ORDER BY timestamp DESC LIMIT " + std::to_string(limit);

      result = transaction.exec_params(query, deviceId);
    }

    for (const auto& row : result) {
      models::IoTData data;
      data.id = row["id"].as<int>();
      data.deviceId = row["device_id"].as<std::string>();
      data.temperature = row["temperature"].as<double>();
      data.humidity = row["humidity"].as<double>();
      data.timestamp = row["ts"].as<std::string>();

      results.push_back(data);
    }

    if (!results.empty()) {
      std::cout << "📥 Получено " << results.size()
                << " записей из удаленной БД"
                << (deviceId.empty() ? "" : " для устройства " + deviceId)
                << std::endl;
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка получения телеметрии из удаленной БД: " << e.what()
              << std::endl;
  }

  return results;
}

std::vector<models::IoTData>
RemoteDatabaseConnection::getLatestTelemetryForAllDevices() {
  reconnectIfNeeded();
  std::vector<models::IoTData> results;

  try {
    pqxx::work transaction(getConnection());

    // Получаем последние данные для каждого устройства
    // Используем DISTINCT ON для получения последней записи каждого устройства
    auto result = transaction.exec(
        "SELECT DISTINCT ON (device_id) id, device_id, temperature, humidity, "
        "to_char(timestamp, 'YYYY-MM-DD HH24:MI:SS') as ts "
        "FROM telemetry_data "
        "ORDER BY device_id, timestamp DESC");

    for (const auto& row : result) {
      models::IoTData data;
      data.id = row["id"].as<int>();
      data.deviceId = row["device_id"].as<std::string>();
      data.temperature = row["temperature"].as<double>();
      data.humidity = row["humidity"].as<double>();
      data.timestamp = row["ts"].as<std::string>();

      results.push_back(data);
    }

    if (!results.empty()) {
      std::cout << "📥 Получены последние данные для " << results.size()
                << " устройств" << std::endl;
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка получения последних данных: " << e.what()
              << std::endl;
  }

  return results;
}

std::vector<models::IoTData> RemoteDatabaseConnection::getDeviceTelemetry(
    const std::string& deviceId, int limit) {
  return getTelemetryData(deviceId, limit);
}

bool RemoteDatabaseConnection::validateSchema() {
  reconnectIfNeeded();

  try {
    pqxx::work transaction(getConnection());

    // Проверяем наличие таблицы telemetry_data
    auto result = transaction.exec(
        "SELECT EXISTS ("
        "SELECT FROM information_schema.tables "
        "WHERE table_schema = 'public' "
        "AND table_name = 'telemetry_data')");

    bool tableExists = result[0][0].as<bool>();

    if (!tableExists) {
      std::cerr << "❌ Таблица telemetry_data не найдена в удаленной БД"
                << std::endl;
      return false;
    }

    // Проверяем структуру таблицы
    auto columns = transaction.exec(
        "SELECT column_name, data_type "
        "FROM information_schema.columns "
        "WHERE table_name = 'telemetry_data' "
        "ORDER BY ordinal_position");

    std::cout << "📊 Структура таблицы telemetry_data в удаленной БД:"
              << std::endl;
    bool hasRequiredColumns = false;
    int columnCount = 0;

    for (const auto& row : columns) {
      std::string columnName = row["column_name"].as<std::string>();
      std::string dataType = row["data_type"].as<std::string>();

      std::cout << "   • " << columnName << " : " << dataType << std::endl;

      // Проверяем наличие обязательных колонок
      if (columnName == "device_id" || columnName == "temperature" ||
          columnName == "humidity" || columnName == "timestamp") {
        hasRequiredColumns = true;
      }

      columnCount++;
    }

    if (columnCount >= 4 && hasRequiredColumns) {
      std::cout << "✅ Структура таблицы корректна" << std::endl;
      return true;
    } else {
      std::cerr << "❌ Неправильная структура таблицы telemetry_data"
                << std::endl;
      return false;
    }

  } catch (const std::exception& e) {
    std::cerr << "❌ Ошибка проверки схемы удаленной БД: " << e.what()
              << std::endl;
    return false;
  }
}

}  // namespace iot_core::core