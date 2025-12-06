#pragma once

#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <string>
#include <vector>

#include "../models/IoTData.h"
#include "RemoteDatabaseConnection.h"

namespace iot_core::core {

class DatabaseRepository {
 public:
  explicit DatabaseRepository(const std::string& connectionString);
  ~DatabaseRepository();

  // Подключение
  void initialize();
  bool isConnected() const;

  // Подключение к удаленной БД
  void connectToRemoteDatabase(const std::string& connectionString);
  bool isRemoteConnected() const;
  std::vector<models::IoTData> getRemoteTelemetry(
      const std::string& deviceId = "", int limit = 10);

  std::vector<models::IoTData> getLatestRemoteTelemetryForAllDevices();

  std::vector<models::IoTData> getRecentTelemetry(int limit = 10);
  std::vector<models::IoTData> getDeviceTelemetry(const std::string& deviceId,
                                                  int limit = 10);

  // Управление пользователями и устройствами
  void addUserDevice(long chatId, const std::string& deviceId);
  void removeUserDevice(long chatId, const std::string& deviceId);
  std::vector<std::string> getUserDevices(long chatId);
  std::vector<long> getDeviceSubscribers(const std::string& deviceId);

  // Новый метод: получение всех устройств с подписчиками
  std::vector<std::string> getAllSubscribedDevices();

  // Управление оповещениями
  void setUserAlert(long chatId, const models::UserAlert& alert);
  models::UserAlert getUserAlert(long chatId);
  void clearUserAlerts(long chatId);
  std::vector<std::pair<long, models::UserAlert>> getAllActiveAlerts();

  // Статистика
  int getTotalRecordsCount();
  int getActiveUsersCount();

  // Вспомогательные методы
  bool deviceExists(const std::string& deviceId);
  bool userHasDevice(long chatId, const std::string& deviceId);

 private:
  pqxx::connection& getConnection();
  void reconnectIfNeeded();

  std::string connectionString_;
  std::unique_ptr<pqxx::connection> connection_;
  std::recursive_mutex connectionMutex_;

  std::unique_ptr<RemoteDatabaseConnection> remoteConnection_;
};

}  // namespace iot_core::core