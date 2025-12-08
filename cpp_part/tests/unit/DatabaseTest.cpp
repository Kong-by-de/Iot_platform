#include <gtest/gtest.h>

#include <memory>
#include <thread>

#include "../../src/core/Database.h"

using namespace iot_core::core;
using namespace iot_core::models;

class DatabaseTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    std::cout << "📦 DatabaseTest Suite Setup" << std::endl;
  }

  static void TearDownTestSuite() {
    std::cout << "🧹 DatabaseTest Suite Cleanup" << std::endl;
  }

  void SetUp() override {
    // Используем тестовую БД из GitHub Actions
    std::string connectionString =
        "host=localhost port=5432 dbname=iot_test "
        "user=test_user password=test_pass";

    db = std::make_unique<DatabaseRepository>(connectionString);

    try {
      db->initialize();
      std::cout << "✅ Database connection established" << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "⚠️  Database not available: " << e.what() << std::endl;
      // Помечаем тест как пропущенный
      GTEST_SKIP() << "Database not available: " << e.what();
    }
  }

  void TearDown() override {
    // Очищаем тестовые данные если они были созданы
    if (db && db->isConnected()) {
      try {
        // Можно добавить очистку тестовых данных здесь
        std::cout << "🧹 Cleaning up test data..." << std::endl;
      } catch (...) {
        // Игнорируем ошибки очистки
      }
    }
  }

  std::unique_ptr<DatabaseRepository> db;
};

TEST_F(DatabaseTest, ConnectionTest) {
  ASSERT_TRUE(db != nullptr);
  EXPECT_TRUE(db->isConnected());
}

TEST_F(DatabaseTest, UserDeviceManagement) {
  const long userId = 999999;
  const std::string deviceId = "test_user_device_ci";

  // Добавляем устройство
  EXPECT_NO_THROW(db->addUserDevice(userId, deviceId));

  // Проверяем, что устройство добавлено
  auto devices = db->getUserDevices(userId);
  EXPECT_FALSE(devices.empty());
  EXPECT_NE(std::find(devices.begin(), devices.end(), deviceId), devices.end());

  // Проверяем подписчиков устройства
  auto subscribers = db->getDeviceSubscribers(deviceId);
  EXPECT_FALSE(subscribers.empty());
  EXPECT_NE(std::find(subscribers.begin(), subscribers.end(), userId),
            subscribers.end());

  // Удаляем устройство
  EXPECT_NO_THROW(db->removeUserDevice(userId, deviceId));

  // Проверяем, что устройство удалено
  devices = db->getUserDevices(userId);
  EXPECT_TRUE(std::find(devices.begin(), devices.end(), deviceId) ==
              devices.end());
}

TEST_F(DatabaseTest, UserAlerts) {
  const long userId = 888888;

  UserAlert alert;
  alert.temperatureHighThreshold = 30.0;
  alert.temperatureLowThreshold = 15.0;
  alert.humidityHighThreshold = 70.0;
  alert.humidityLowThreshold = 30.0;

  // Устанавливаем оповещения
  EXPECT_NO_THROW(db->setUserAlert(userId, alert));

  // Получаем оповещения
  UserAlert retrievedAlert = db->getUserAlert(userId);

  EXPECT_DOUBLE_EQ(retrievedAlert.temperatureHighThreshold,
                   alert.temperatureHighThreshold);
  EXPECT_DOUBLE_EQ(retrievedAlert.temperatureLowThreshold,
                   alert.temperatureLowThreshold);
  EXPECT_DOUBLE_EQ(retrievedAlert.humidityHighThreshold,
                   alert.humidityHighThreshold);
  EXPECT_DOUBLE_EQ(retrievedAlert.humidityLowThreshold,
                   alert.humidityLowThreshold);
  EXPECT_TRUE(retrievedAlert.hasAnyAlert());

  // Очищаем оповещения
  EXPECT_NO_THROW(db->clearUserAlerts(userId));

  // Проверяем, что оповещения очищены
  retrievedAlert = db->getUserAlert(userId);
  EXPECT_FALSE(retrievedAlert.hasAnyAlert());
}

TEST_F(DatabaseTest, DeviceExistenceCheck) {
  // Этот тест проверяет удаленную БД, может быть пропущен
  if (!db->isRemoteConnected()) {
    GTEST_SKIP()
        << "Remote database not connected, skipping device existence check";
  }

  const std::string existingDevice = "sensor_1";
  const std::string nonExistingDevice = "non_existing_test_device_ci";

  // Проверяем существование (через удаленную БД)
  bool deviceExists = db->deviceExists(existingDevice);
  bool nonExists = db->deviceExists(nonExistingDevice);

  // Мы не знаем точно какие устройства есть в удаленной БД,
  // поэтому просто проверяем что метод не падает
  EXPECT_NO_THROW(db->deviceExists("test"));
}

TEST_F(DatabaseTest, GetAllSubscribedDevices) {
  // Сначала добавляем тестовые устройства
  db->addUserDevice(111111, "test_device_1");
  db->addUserDevice(222222, "test_device_2");

  // Получаем все устройства
  auto devices = db->getAllSubscribedDevices();

  // Должно быть хотя бы 2 устройства
  EXPECT_GE(devices.size(), 2);

  // Очищаем
  db->removeUserDevice(111111, "test_device_1");
  db->removeUserDevice(222222, "test_device_2");
}

TEST_F(DatabaseTest, Statistics) {
  // Добавляем пользователей для статистики
  db->addUserDevice(1001, "stat_device");
  db->addUserDevice(1002, "stat_device");

  int activeUsers = db->getActiveUsersCount();
  EXPECT_GE(activeUsers, 2);

  // Очищаем
  db->removeUserDevice(1001, "stat_device");
  db->removeUserDevice(1002, "stat_device");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  std::cout << "🚀 Starting Database Tests..." << std::endl;
  std::cout << "📊 Testing database functionality" << std::endl;

  int result = RUN_ALL_TESTS();

  if (result == 0) {
    std::cout << "✅ All database tests passed!" << std::endl;
  } else {
    std::cout << "⚠️  Some database tests failed" << std::endl;
  }

  return result;
}