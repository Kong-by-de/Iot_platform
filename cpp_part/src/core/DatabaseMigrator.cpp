// src/core/DatabaseMigrator.cpp
#include "DatabaseMigrator.h"

#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace iot_core::core {

namespace fs = std::filesystem;

DatabaseMigrator::DatabaseMigrator(const std::string& connectionString)
    : connectionString_(connectionString) {
  std::cout << "   🔧 Инициализация DatabaseMigrator" << std::endl;

  // Создаем конфигурационный файл для dbmate
  createDbmateConfig();

  // Создаем директорию для миграций если её нет
  createMigrationsDirectory();
}

bool DatabaseMigrator::executeCommand(const std::string& command) {
  std::cout << "   🚀 Выполнение: " << command << std::endl;

  int result = system(command.c_str());

  if (result == 0) {
    std::cout << "   ✅ Команда выполнена успешно" << std::endl;
    return true;
  } else {
    std::cerr << "   ❌ Команда завершилась с кодом " << result << std::endl;
    return false;
  }
}

bool DatabaseMigrator::waitForDatabase(int maxRetries, int delaySeconds) {
  std::cout << "   ⏳ Ожидание доступности базы данных..." << std::endl;

  // Используем простую команду psql для проверки
  std::string testCommand =
      "psql \"" + connectionString_ + "\" -c \"SELECT 1\" > /dev/null 2>&1";

  for (int i = 0; i < maxRetries; ++i) {
    int result = system(testCommand.c_str());

    if (result == 0) {
      std::cout << "   ✅ База данных доступна" << std::endl;
      return true;
    }

    if (i < maxRetries - 1) {
      std::cout << "     • Попытка " << (i + 1) << "/" << maxRetries
                << ": База данных недоступна, жду " << delaySeconds << " сек..."
                << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(delaySeconds));
    }
  }

  std::cerr << "   ❌ Не удалось подключиться к базе данных" << std::endl;
  return false;
}

bool DatabaseMigrator::createDbmateConfig() {
  try {
    // Определяем путь к корню проекта
    std::string projectRoot;

    // Попробуем несколько способов найти корень проекта
    const char* envPwd = std::getenv("PWD");
    if (envPwd && std::strlen(envPwd) > 0) {
      projectRoot = envPwd;
    } else {
      // Получаем текущую директорию
      char buffer[1024];
      if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        projectRoot = buffer;
      } else {
        projectRoot = ".";
      }
    }

    // Проверяем, что мы в правильной директории
    // Если есть папка src или db, считаем что это корень проекта
    if (!fs::exists(projectRoot + "/src") && !fs::exists(projectRoot + "/db")) {
      // Пробуем найти проект на уровень выше
      projectRoot = projectRoot + "/..";
    }

    // Создаем конфигурационный файл для dbmate
    std::ofstream configFile(projectRoot + "/.dbmate");
    if (!configFile.is_open()) {
      std::cerr << "   ⚠️  Не удалось создать конфиг файл для dbmate"
                << std::endl;
      return false;
    }

    configFile << "DATABASE_URL=\"" << connectionString_ << "\"\n";
    configFile << "MIGRATIONS_DIR=\"" << projectRoot << "/db/migrations\"\n";
    configFile.close();

    std::cout << "   📄 Создан конфигурационный файл .dbmate" << std::endl;
    std::cout << "   📁 Путь к миграциям: " << projectRoot << "/db/migrations"
              << std::endl;

    return true;

  } catch (const std::exception& e) {
    std::cerr << "   ❌ Ошибка создания конфига: " << e.what() << std::endl;
    return false;
  }
}

bool DatabaseMigrator::createMigrationsDirectory() {
  try {
    // Определяем путь к корню проекта (на уровень выше build)
    std::string projectRoot;
    char buffer[1024];

    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
      std::string currentDir = buffer;
      // Если мы в build директории, поднимаемся на уровень выше
      if (currentDir.find("/build") != std::string::npos) {
        size_t pos = currentDir.find("/build");
        projectRoot = currentDir.substr(0, pos);
      } else {
        projectRoot = currentDir;
      }
    } else {
      projectRoot = "..";  // По умолчанию на уровень выше
    }

    std::string migrationsDir = projectRoot + "/db/migrations";

    // Создаем директорию
    std::string mkdirCommand = "mkdir -p \"" + migrationsDir + "\"";
    system(mkdirCommand.c_str());

    // Копируем файл миграции если существует
    std::string sourceFile = projectRoot + "/20251203110925_initial_schema.sql";
    std::string destFile = migrationsDir + "/20251203110925_initial_schema.sql";

    std::string copyCommand =
        "cp -n \"" + sourceFile + "\" \"" + destFile + "\" 2>/dev/null || true";
    system(copyCommand.c_str());

    std::cout << "   📁 Путь к миграциям: " << migrationsDir << std::endl;

    // Обновляем конфиг файл с правильным путем
    std::ofstream configFile(".dbmate");
    if (configFile.is_open()) {
      configFile << "DATABASE_URL=\"" << connectionString_ << "\"\n";
      configFile << "MIGRATIONS_DIR=\"" << migrationsDir << "\"\n";
      configFile.close();
    }

    return true;

  } catch (const std::exception& e) {
    std::cerr << "   ⚠️  Ошибка: " << e.what() << std::endl;
    return false;
  }
}

bool DatabaseMigrator::runMigrations() {
  std::cout << "   📋 Запуск миграций базы данных..." << std::endl;

  if (!waitForDatabase()) {
    return false;
  }

  // Простая команда - запускаем скрипт из корня проекта
  std::string command = "cd ~/cpp/iot_project && bash run_migrations.sh";

  return executeCommand(command);
}

}  // namespace iot_core::core