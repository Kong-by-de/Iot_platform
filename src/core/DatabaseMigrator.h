// src/core/DatabaseMigrator.h
#pragma once

#include <string>

namespace iot_core::core {

/**
 * @brief Простой класс для запуска миграций базы данных через dbmate
 *
 * Использует внешний инструмент dbmate для управления миграциями.
 */
class DatabaseMigrator {
 public:
  /**
   * @brief Конструктор
   * @param connectionString Строка подключения к PostgreSQL
   */
  explicit DatabaseMigrator(const std::string& connectionString);

  /**
   * @brief Запускает все pending миграции
   * @return true если миграции успешно применены
   */
  bool runMigrations();

 private:
  std::string connectionString_;

  /**
   * @brief Проверяет, доступна ли база данных
   * @return true если база данных доступна
   */
  bool waitForDatabase(int maxRetries = 10, int delaySeconds = 2);

  /**
   * @brief Выполняет системную команду
   * @param command Команда для выполнения
   * @return true если команда выполнена успешно
   */
  bool executeCommand(const std::string& command);

  /**
   * @brief Создает конфигурационный файл для dbmate
   * @return true если файл создан успешно
   */
  bool createDbmateConfig();

  /**
   * @brief Создает структуру директорий для миграций
   * @return true если директории созданы успешно
   */
  bool createMigrationsDirectory();
};

}  // namespace iot_core::core