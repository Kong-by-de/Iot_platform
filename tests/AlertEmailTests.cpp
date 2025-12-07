// tests/AlertEmailTests.cpp

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#define private public
#include "../src/services/AlertService.h"
#include "../src/smtp/EmailService.h"
#undef private

using iot_core::services::AlertProcessingService;
using iot_core::smtp::EmailService;

namespace {

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

}  // namespace

// ================== EmailService: HTML-письмо =======================

TEST(EmailServiceTests, TemperatureAbove_BuildsCorrectHtml) {
  EmailService service;  // конфиг из env, но мы НЕ шлём реальное письмо

  std::string device_id = "device-42";
  double value = 100.0;
  std::string metric = "temperature";
  std::string direction = "above";

  std::string body =
      service.formatAlertBody(device_id, value, metric, direction);

  EXPECT_TRUE(contains(body, "IoT Platform Alert"));
  EXPECT_TRUE(contains(body, "Device Name"));
  EXPECT_TRUE(contains(body, device_id));
  EXPECT_TRUE(contains(body, "Metric"));
  EXPECT_TRUE(contains(body, "Temperature"));
  EXPECT_TRUE(contains(body, "Status"));
  EXPECT_TRUE(contains(body, "too high"));
  EXPECT_TRUE(contains(body, "Current Value"));

  EXPECT_TRUE(contains(body, "100"));
  EXPECT_TRUE(contains(body, "°C"));

  EXPECT_TRUE(contains(body, "🔥"));
}

TEST(EmailServiceTests, HumidityBelow_BuildsCorrectHtml) {
  EmailService service;

  std::string device_id = "sensor-7";
  double value = 55.5;
  std::string metric = "humidity";
  std::string direction = "below";

  std::string body =
      service.formatAlertBody(device_id, value, metric, direction);

  EXPECT_TRUE(contains(body, "Device Name"));
  EXPECT_TRUE(contains(body, device_id));
  EXPECT_TRUE(contains(body, "Metric"));
  EXPECT_TRUE(contains(body, "Humidity"));
  EXPECT_TRUE(contains(body, "Status"));
  EXPECT_TRUE(contains(body, "too low"));
  EXPECT_TRUE(contains(body, "Current Value"));

  EXPECT_TRUE(contains(body, "55.5"));
  EXPECT_TRUE(contains(body, "%"));

  EXPECT_TRUE(contains(body, "🏜️"));
}

TEST(EmailServiceTests, FormatDoubleNice_TrimsZeros) {
  EmailService service;

  EXPECT_EQ(service.formatDoubleNice(100.0), "100");
  EXPECT_EQ(service.formatDoubleNice(55.50), "55.5");

  std::string pi = service.formatDoubleNice(3.14159);
  EXPECT_EQ(pi.rfind("3.14", 0), 0u);  // начинается с "3.14"
}

TEST(EmailServiceTests, HumanHelpers_WorkAsExpected) {
  EmailService service;

  EXPECT_EQ(service.humanMetricName("temperature"), "Temperature");
  EXPECT_EQ(service.humanMetricName("humidity"), "Humidity");

  EXPECT_EQ(service.humanDirection("above"), "too high");
  EXPECT_EQ(service.humanDirection("below"), "too low");

  EXPECT_EQ(service.getEmoji("temperature", "above"), "🔥");
  EXPECT_EQ(service.getEmoji("temperature", "below"), "❄️");
  EXPECT_EQ(service.getEmoji("humidity", "above"), "💧");
  EXPECT_EQ(service.getEmoji("humidity", "below"), "🏜️");
}

// ================== AlertProcessingService: shouldNotify ============

TEST(AlertProcessingServiceTests, ShouldNotify_PreventsDuplicates) {
  std::shared_ptr<iot_core::core::DatabaseRepository> db;         // nullptr
  std::shared_ptr<iot_core::core::NotificationService> notifier;  // nullptr

  AlertProcessingService service(db, notifier);

  long userId = 123;
  std::string deviceId = "dev-A";

  // первый алерт должен пройти
  bool first = service.shouldNotify(userId, deviceId, "temp_high", 42.0);
  EXPECT_TRUE(first);

  // такой же алерт сразу же – должен быть заблокирован
  bool second = service.shouldNotify(userId, deviceId, "temp_high", 43.0);
  EXPECT_FALSE(second);

  // другой тип алерта должен пройти
  bool different = service.shouldNotify(userId, deviceId, "hum_low", 10.0);
  EXPECT_TRUE(different);
}
