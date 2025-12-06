// src/bot/TelegramBotHandler.h
#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../core/Database.h"
#include "../core/NotificationService.h"
#include "../services/AlertService.h"

namespace iot_core::bot {

class TelegramBotHandler {
 public:
  using CommandHandler =
      std::function<void(long chatId, const std::vector<std::string>& args)>;

  TelegramBotHandler(
      std::shared_ptr<core::DatabaseRepository> database,
      std::shared_ptr<core::NotificationService> notifier,
      std::shared_ptr<services::AlertProcessingService> alertService);

  void startPolling(const std::string& botToken);
  void stop();
  bool isRunning() const;

  void sendMessage(long chatId, const std::string& text);

 private:
  void setupCommandHandlers();

  std::shared_ptr<core::DatabaseRepository> database_;
  std::shared_ptr<core::NotificationService> notifier_;
  std::shared_ptr<services::AlertProcessingService> alertService_;

  std::unordered_map<std::string, CommandHandler> commandHandlers_;
  std::thread pollingThread_;
  std::atomic<bool> running_{false};
  std::string botToken_;
};

}  // namespace iot_core::bot