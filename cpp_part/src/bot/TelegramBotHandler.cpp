#include "TelegramBotHandler.h"

#include <cpr/cpr.h>
#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "../utils/Formatter.h"

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace iot_core::bot {

TelegramBotHandler::TelegramBotHandler(
    std::shared_ptr<core::DatabaseRepository> database,
    std::shared_ptr<core::NotificationService> notifier,
    std::shared_ptr<services::AlertProcessingService> alertService)
    : database_(std::move(database)),
      notifier_(std::move(notifier)),
      alertService_(std::move(alertService)) {
  setupCommandHandlers();
  std::cout << "🤖 Telegram Bot Handler инициализирован" << std::endl;
}

void TelegramBotHandler::startPolling(const std::string& botToken) {
  if (running_) {
    return;
  }

  botToken_ = botToken;

  if (botToken_.empty()) {
    std::cerr << "❌ Telegram bot token is empty!" << std::endl;
    return;
  }

  std::cout << "🤖 Starting Telegram bot polling..." << std::endl;

  running_ = true;

  // Запускаем polling loop в отдельном потоке
  pollingThread_ = std::thread([this]() {
    std::cout << "🔄 Telegram polling loop started" << std::endl;

    long lastUpdateId = 0;

    while (running_) {
      try {
        std::string url =
            "https://api.telegram.org/bot" + botToken_ + "/getUpdates";

        // Простой polling с таймаутом
        auto response = cpr::Get(
            cpr::Url{url},
            cpr::Parameters{{"offset", std::to_string(lastUpdateId + 1)},
                            {"timeout", "10"}},
            cpr::Timeout{15000});

        if (response.status_code == 200) {
          try {
            auto data = json::parse(response.text);

            if (data["ok"] == true) {
              auto updates = data["result"];

              for (const auto& update : updates) {
                lastUpdateId = update["update_id"].get<long>();

                // Обрабатываем обновление
                if (update.contains("message") &&
                    update["message"].contains("text")) {
                  std::string text = update["message"]["text"];
                  long chatId = update["message"]["chat"]["id"];

                  std::cout << "📨 Message from " << chatId << ": " << text
                            << std::endl;

                  // Обрабатываем команду
                  if (text.rfind("/", 0) == 0) {
                    // Ищем команду в обработчиках
                    std::string cmd = text;
                    size_t spacePos = cmd.find(' ');
                    if (spacePos != std::string::npos) {
                      cmd = cmd.substr(0, spacePos);
                    }

                    // Удаляем упоминание бота если есть
                    size_t atPos = cmd.find('@');
                    if (atPos != std::string::npos) {
                      cmd = cmd.substr(0, atPos);
                    }

                    auto it = commandHandlers_.find(cmd);
                    if (it != commandHandlers_.end()) {
                      // Извлекаем аргументы
                      std::vector<std::string> args;
                      std::string rest = text.substr(cmd.length());
                      std::istringstream iss(rest);
                      std::string arg;
                      while (iss >> arg) {
                        args.push_back(arg);
                      }

                      it->second(chatId, args);
                    } else {
                      sendMessage(chatId,
                                  "❓ Неизвестная команда. Используй /start "
                                  "для помощи");
                    }
                  } else {
                    sendMessage(
                        chatId,
                        "👋 Привет! Используй /start для начала работы");
                  }
                }
              }
            }
          } catch (const json::exception& e) {
            std::cerr << "❌ JSON parse error: " << e.what() << std::endl;
          }
        } else if (response.status_code !=
                   0) {  // 0 - это timeout, это нормально
          std::cerr << "❌ Telegram API error: " << response.status_code
                    << " - " << response.text << std::endl;
        }

      } catch (const std::exception& e) {
        std::cerr << "❌ Polling exception: " << e.what() << std::endl;
        std::this_thread::sleep_for(5s);
      }
    }

    std::cout << "🛑 Telegram polling loop stopped" << std::endl;
  });

  // Отделяем поток
  pollingThread_.detach();

  std::cout << "✅ Telegram Bot polling started" << std::endl;
}

void TelegramBotHandler::stop() {
  running_ = false;
  if (pollingThread_.joinable()) {
    pollingThread_.join();
  }
  std::cout << "🛑 Telegram bot stopped" << std::endl;
}

bool TelegramBotHandler::isRunning() const { return running_; }

void TelegramBotHandler::setupCommandHandlers() {
  commandHandlers_ = {
      {"/start",
       [this](long chatId, const auto& args) {
         sendMessage(chatId, utils::Formatter::createWelcomeMessage());
       }},

      {"/help",
       [this](long chatId, const auto& args) {
         sendMessage(chatId, utils::Formatter::createHelpMessage());
       }},

      {"/status",
       [this](long chatId, const auto& args) {
         bool dbConnected = database_->isConnected();
         std::string status = dbConnected ? "✅ База данных подключена"
                                          : "❌ База данных недоступна";
         sendMessage(chatId, "📊 *Статус системы:*\n\n" + status);
       }},

      {"/last",
       [this](long chatId, const auto& args) {
         try {
           auto data = database_->getRecentTelemetry(5);

           if (data.empty()) {
             sendMessage(chatId, "📭 Нет данных телеметрии");
             return;
           }

           std::string message = "📊 *Последние данные:*\n\n";
           for (const auto& item : data) {
             message += "• `" + item.deviceId +
                        "`: " + std::to_string(item.temperature).substr(0, 4) +
                        "°C, " + std::to_string(item.humidity).substr(0, 4) +
                        "%\n";
           }

           sendMessage(chatId, message);
         } catch (const std::exception& e) {
           sendMessage(chatId, "❌ Ошибка получения данных");
         }
       }},

      {"/add_device",
       [this](long chatId, const auto& args) {
         if (args.empty()) {
           sendMessage(chatId, "❌ Использование: /add_device <device_id>");
           return;
         }

         std::string deviceId = args[0];
         try {
           database_->addUserDevice(chatId, deviceId);
           sendMessage(chatId, "✅ Устройство `" + deviceId + "` добавлено");
         } catch (...) {
           sendMessage(chatId, "❌ Ошибка добавления устройства");
         }
       }},

      {"/my_devices",
       [this](long chatId, const auto& args) {
         try {
           auto devices = database_->getUserDevices(chatId);
           sendMessage(chatId, utils::Formatter::formatDeviceList(devices));
         } catch (...) {
           sendMessage(chatId, "❌ Ошибка получения устройств");
         }
       }},

      {"/alert_temp_high",
       [this](long chatId, const auto& args) {
         if (args.empty()) {
           sendMessage(chatId, "❌ Использование: /alert_temp_high <значение>");
           return;
         }

         try {
           double threshold = std::stod(args[0]);
           models::UserAlert alert = database_->getUserAlert(chatId);
           alert.temperatureHighThreshold = threshold;
           database_->setUserAlert(chatId, alert);

           sendMessage(chatId, "🔥 Установлено оповещение по температуре: >" +
                                   std::to_string(threshold).substr(0, 4) +
                                   "°C");
         } catch (...) {
           sendMessage(chatId, "❌ Ошибка установки оповещения");
         }
       }},

      {"/alert_temp_low",
       [this](long chatId, const auto& args) {
         if (args.empty()) {
           sendMessage(chatId, "❌ Использование: /alert_temp_low <значение>");
           return;
         }

         try {
           double threshold = std::stod(args[0]);
           models::UserAlert alert = database_->getUserAlert(chatId);
           alert.temperatureLowThreshold = threshold;
           database_->setUserAlert(chatId, alert);

           sendMessage(chatId, "❄️ Установлено оповещение по температуре: <" +
                                   std::to_string(threshold).substr(0, 4) +
                                   "°C");
         } catch (...) {
           sendMessage(chatId, "❌ Ошибка установки оповещения");
         }
       }},

      {"/alert_hum_high",
       [this](long chatId, const auto& args) {
         if (args.empty()) {
           sendMessage(chatId, "❌ Использование: /alert_hum_high <значение>");
           return;
         }

         try {
           double threshold = std::stod(args[0]);
           models::UserAlert alert = database_->getUserAlert(chatId);
           alert.humidityHighThreshold = threshold;
           database_->setUserAlert(chatId, alert);

           sendMessage(chatId, "💦 Установлено оповещение по влажности: >" +
                                   std::to_string(threshold).substr(0, 4) +
                                   "%");
         } catch (...) {
           sendMessage(chatId, "❌ Ошибка установки оповещения");
         }
       }},

      {"/alert_hum_low",
       [this](long chatId, const auto& args) {
         if (args.empty()) {
           sendMessage(chatId, "❌ Использование: /alert_hum_low <значение>");
           return;
         }

         try {
           double threshold = std::stod(args[0]);
           models::UserAlert alert = database_->getUserAlert(chatId);
           alert.humidityLowThreshold = threshold;
           database_->setUserAlert(chatId, alert);

           sendMessage(chatId, "🏜️ Установлено оповещение по влажности: <" +
                                   std::to_string(threshold).substr(0, 4) +
                                   "%");
         } catch (...) {
           sendMessage(chatId, "❌ Ошибка установки оповещения");
         }
       }},

      {"/show_alerts",
       [this](long chatId, const auto& args) {
         try {
           auto alert = database_->getUserAlert(chatId);
           sendMessage(chatId, utils::Formatter::formatAlertSettings(alert));
         } catch (...) {
           sendMessage(chatId, "❌ Ошибка получения настроек оповещений");
         }
       }},

      {"/clear_alerts",
       [this](long chatId, const auto& args) {
         try {
           database_->clearUserAlerts(chatId);
           sendMessage(chatId, "🗑️ Все оповещения удалены");
         } catch (...) {
           sendMessage(chatId, "❌ Ошибка удаления оповещений");
         }
       }},

      {"/test_hot",
       [this](long chatId, const auto& args) {
         // Отправляем тестовые данные с высокой температурой
         alertService_->processTelemetryData("test_device", 35.0, 50.0);
         sendMessage(chatId, "🔥 Тестовые данные отправлены (35°C)");
       }},

      {"/test_cold",
       [this](long chatId, const auto& args) {
         // Отправляем тестовые данные с низкой температурой
         alertService_->processTelemetryData("test_device", 10.0, 50.0);
         sendMessage(chatId, "❄️ Тестовые данные отправлены (10°C)");
       }},

      {"/test_humid",
       [this](long chatId, const auto& args) {
         // Отправляем тестовые данные с высокой влажностью
         alertService_->processTelemetryData("test_device", 22.0, 80.0);
         sendMessage(chatId, "💦 Тестовые данные отправлены (80% влажность)");
       }},

      {"/test_dry",
       [this](long chatId, const auto& args) {
         // Отправляем тестовые данные с низкой влажностью
         alertService_->processTelemetryData("test_device", 22.0, 20.0);
         sendMessage(chatId, "🏜️ Тестовые данные отправлены (20% влажность)");
       }},

      {"/stats",
       [this](long chatId, const auto& args) {
         try {
           int totalRecords = database_->getTotalRecordsCount();
           int activeUsers = database_->getActiveUsersCount();

           std::ostringstream oss;
           oss << "📈 *Статистика системы:*\n\n"
               << "📊 Всего записей в БД: " << totalRecords << "\n"
               << "👥 Активных пользователей: " << activeUsers << "\n";

           sendMessage(chatId, oss.str());
         } catch (...) {
           sendMessage(chatId, "❌ Ошибка получения статистики");
         }
       }},
  };
}

void TelegramBotHandler::sendMessage(long chatId, const std::string& text) {
  try {
    json payload;
    payload["chat_id"] = chatId;
    payload["text"] = text;
    payload["parse_mode"] = "Markdown";

    std::string url =
        "https://api.telegram.org/bot" + botToken_ + "/sendMessage";

    cpr::Response r = cpr::Post(
        cpr::Url{url}, cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{payload.dump()}, cpr::Timeout{10000});

    if (r.status_code == 200) {
      std::cout << "✅ Telegram message sent to " << chatId << std::endl;
    } else {
      std::cerr << "❌ Failed to send Telegram message: " << r.status_code
                << " - " << r.text << std::endl;
    }
  } catch (const std::exception& e) {
    std::cerr << "❌ Exception sending Telegram message: " << e.what()
              << std::endl;
  }
}

}  // namespace iot_core::bot