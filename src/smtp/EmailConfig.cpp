// src/smtp/EmailConfig.cpp
#include "EmailConfig.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace iot_core::smtp {

// Вспомогательные функции из smtp.cpp
static std::string trim(const std::string& s) {
  size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
    ++start;
  size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
    --end;
  return s.substr(start, end - start);
}

static std::map<std::string, std::string> loadConfigFile(
    const std::string& path) {
  std::map<std::string, std::string> cfg;
  std::ifstream in(path);
  if (!in.is_open()) {
    return cfg;  // Файл не найден - это нормально
  }

  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty()) continue;
    if (line[0] == '#') continue;

    auto pos = line.find('=');
    if (pos == std::string::npos) continue;

    std::string key = trim(line.substr(0, pos));
    std::string val = trim(line.substr(pos + 1));

    if (!key.empty()) {
      cfg[key] = val;
    }
  }

  return cfg;
}

EmailConfig EmailConfig::loadFromEnv() {
  EmailConfig config;

  std::cout << "🔍 Загрузка конфигурации SMTP..." << std::endl;

  // 1. Пробуем загрузить из файла smtp.conf (основной способ)
  auto m = loadConfigFile("smtp.conf");

  if (!m.empty()) {
    std::cout << "📄 Конфигурация загружена из smtp.conf" << std::endl;

    auto get = [&](const std::string& key, const std::string& def = "") {
      auto it = m.find(key);
      if (it == m.end() || it->second.empty()) return def;
      return it->second;
    };

    config.server = get("SMTP_SERVER", "smtp.gmail.com");

    // Обрабатываем URL вида "smtp://smtp.gmail.com"
    if (config.server.find("smtp://") == 0) {
      config.server = config.server.substr(7);
    }

    std::string portStr = get("SMTP_PORT", "587");
    try {
      config.port = std::stoi(portStr);
    } catch (...) {
      config.port = 587;
    }

    config.username = get("SMTP_LOGIN");
    config.password = get("SMTP_PASSWORD");
    config.fromEmail = get("SMTP_FROM", config.username);

    // Добавляем получателей из файла (можно указать через запятую)
    std::string recipientsStr = get("ALERT_RECIPIENTS", "");
    if (!recipientsStr.empty()) {
      std::istringstream iss(recipientsStr);
      std::string email;
      while (std::getline(iss, email, ',')) {
        email = trim(email);
        if (!email.empty()) {
          config.alertRecipients.push_back(email);
        }
      }
    }
  } else {
    std::cout
        << "⚠️  Файл smtp.conf не найден, проверяем переменные окружения..."
        << std::endl;
  }

  // 2. Если из файла не получили все настройки, пробуем переменные окружения
  bool needEnvFallback = config.username.empty() || config.password.empty() ||
                         config.fromEmail.empty();

  if (needEnvFallback) {
    // Базовые настройки SMTP из переменных окружения
    const char* server = std::getenv("SMTP_SERVER");
    const char* port = std::getenv("SMTP_PORT");
    const char* username = std::getenv("SMTP_USERNAME");
    const char* password = std::getenv("SMTP_PASSWORD");
    const char* fromEmail = std::getenv("SMTP_FROM_EMAIL");

    // Получатели оповещений из переменных окружения
    const char* alertEmail1 = std::getenv("ALERT_EMAIL_1");
    const char* alertEmail2 = std::getenv("ALERT_EMAIL_2");
    const char* alertEmail3 = std::getenv("ALERT_EMAIL_3");

    // Объединяем с файловыми настройками (env имеет приоритет)
    if (server && config.server == "smtp.gmail.com") {
      config.server = server;
      if (config.server.find("smtp://") == 0) {
        config.server = config.server.substr(7);
      }
    }

    if (port) {
      try {
        config.port = std::stoi(port);
      } catch (...) {
        // Оставляем текущее значение
      }
    }

    if (username && username[0] != '\0') config.username = username;
    if (password && password[0] != '\0') config.password = password;
    if (fromEmail && fromEmail[0] != '\0') config.fromEmail = fromEmail;

    // Добавляем получателей из переменных окружения
    if (alertEmail1 && alertEmail1[0] != '\0') {
      config.alertRecipients.push_back(alertEmail1);
    }
    if (alertEmail2 && alertEmail2[0] != '\0') {
      config.alertRecipients.push_back(alertEmail2);
    }
    if (alertEmail3 && alertEmail3[0] != '\0') {
      config.alertRecipients.push_back(alertEmail3);
    }
  }

  // 3. Значения по умолчанию если ничего не найдено
  if (config.server.empty()) config.server = "smtp.gmail.com";
  if (config.port == 0) config.port = 587;

  // 4. Если все еще нет настроек, выводим сообщение
  if (config.username.empty() || config.password.empty()) {
    std::cout << "❌ Настройки SMTP не найдены!" << std::endl;
    std::cout << "   Создайте файл smtp.conf в текущей директории:"
              << std::endl;
    std::cout << "   SMTP_SERVER=smtp.gmail.com" << std::endl;
    std::cout << "   SMTP_PORT=587" << std::endl;
    std::cout << "   SMTP_LOGIN=your_email@gmail.com" << std::endl;
    std::cout << "   SMTP_PASSWORD=your_app_password" << std::endl;
    std::cout << "   SMTP_FROM=your_email@gmail.com" << std::endl;
    std::cout << "   ALERT_RECIPIENTS=email1@example.com,email2@example.com"
              << std::endl;

    return config;  // Возвращаем невалидную конфигурацию
  }

  // 5. Логируем конфигурацию
  std::cout << "📧 Конфигурация SMTP:" << std::endl;
  std::cout << "   • Сервер: " << config.server << ":" << config.port
            << std::endl;
  std::cout << "   • Пользователь: " << config.username << std::endl;
  std::cout << "   • Пароль: ***" << std::endl;
  std::cout << "   • От: " << config.fromEmail << std::endl;
  std::cout << "   • Получателей: " << config.alertRecipients.size()
            << std::endl;

  if (!config.alertRecipients.empty()) {
    for (size_t i = 0; i < config.alertRecipients.size(); ++i) {
      std::cout << "      " << (i + 1) << ". " << config.alertRecipients[i]
                << std::endl;
    }
  } else {
    std::cout << "      ⚠️  Нет получателей для оповещений" << std::endl;
  }

  // 6. Предупреждение для Gmail
  if (config.server.find("gmail.com") != std::string::npos) {
    std::cout << "⚠️  Для Gmail используйте App Password, а не обычный пароль!"
              << std::endl;
    std::cout << "   Создайте здесь: https://myaccount.google.com/apppasswords"
              << std::endl;
  }

  return config;
}

}  // namespace iot_core::smtp