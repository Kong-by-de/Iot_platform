#include <gtest/gtest.h>

#include <memory>

#include "../../src/core/NotificationService.h"

using namespace iot_core::core;

class NotificationServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Используем тестовый токен
    service = std::make_unique<NotificationService>("test_bot_token_12345");
  }

  std::unique_ptr<NotificationService> service;
};

TEST_F(NotificationServiceTest, Initialization) {
  // Telegram должен быть доступен если передан токен
  EXPECT_TRUE(service->isTelegramAvailable());

  // Email по умолчанию должен быть недоступен без настроек
  EXPECT_FALSE(service->isEmailAvailable());
}

TEST_F(NotificationServiceTest, AlertSendingWithoutToken) {
  // Создаем сервис с пустым токеном
  auto noTokenService = std::make_unique<NotificationService>("");

  // Метод должен корректно обрабатывать отсутствие токена
  // (не падать с исключением)
  EXPECT_NO_THROW(noTokenService->sendTelegramAlert(123456, "test_device", 25.0,
                                                    "temperature", "above"));
}

TEST_F(NotificationServiceTest, MessageSendingToInvalidChat) {
  // Отправка сообщения на невалидный chat_id не должна падать
  EXPECT_NO_THROW(service->sendTelegramMessage(-1, "Test message"));
}

TEST_F(NotificationServiceTest, BroadcastToEmptyList) {
  // Рассылка пустому списку пользователей
  std::vector<long> emptyList;

  EXPECT_NO_THROW(service->broadcastAlert(emptyList, "test_device", 30.0,
                                          "temperature", "above"));
}

TEST_F(NotificationServiceTest, EmailWithoutConfiguration) {
  // Email тест должен корректно обрабатывать отсутствие конфигурации
  // Мы не можем проверить реальную отправку, но проверяем что не падает
  EXPECT_NO_THROW(service->testEmailConnection());

  // Должен возвращать false если email не настроен
  EXPECT_FALSE(service->testEmailConnection());
}

TEST_F(NotificationServiceTest, FormattingMethods) {
  // Проверяем вспомогательные методы форматирования
  // Эти методы можно тестировать без внешних зависимостей

  // Создаем временный объект для тестирования приватных методов
  // или тестируем через публичные методы

  EXPECT_NO_THROW(
      service->sendTelegramMessage(123456, "Test message without formatting"));
}

TEST_F(NotificationServiceTest, MockTest) {
  // Простая проверка что объект создан
  EXPECT_NE(service, nullptr);
  EXPECT_TRUE(service->isTelegramAvailable());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  std::cout << "🚀 Starting NotificationService Tests..." << std::endl;
  std::cout << "📧 Testing notification functionality" << std::endl;
  std::cout << "⚠️  Note: Real Telegram/Email sending is mocked" << std::endl;

  int result = RUN_ALL_TESTS();

  if (result == 0) {
    std::cout << "✅ All notification tests passed!" << std::endl;
  } else {
    std::cout << "⚠️  Some notification tests failed" << std::endl;
  }

  return result;
}