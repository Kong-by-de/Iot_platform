// src/smtp/EmailConfig.h
#pragma once
#include <string>
#include <vector>

namespace iot_core::smtp {

struct EmailConfig {
  std::string server;
  int port;
  std::string username;
  std::string password;
  std::string fromEmail;
  std::vector<std::string> alertRecipients;

  // Загрузка конфигурации из файла smtp.conf и переменных окружения
  static EmailConfig loadFromEnv();

  bool isValid() const {
    return !server.empty() && port > 0 && port <= 65535 && !username.empty() &&
           !password.empty() && !fromEmail.empty();
  }

  bool hasRecipients() const { return !alertRecipients.empty(); }
};

}  // namespace iot_core::smtp