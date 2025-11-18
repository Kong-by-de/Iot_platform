package com.iot.db;

import java.io.IOException;
import java.io.InputStream;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.Properties;

/**
 * Класс для управления соединением с PostgreSQL и инициализации таблиц.
 * Использует параметры из application.properties.
 */
public class DatabaseConnection {

  private static final Properties props = new Properties();

  static {
    try (InputStream input = DatabaseConnection.class.getClassLoader()
        .getResourceAsStream("application.properties")) {
      if (input == null) {
        throw new RuntimeException("Файл application.properties не найден в classpath.");
      }
      props.load(input);
    } catch (IOException e) {
      throw new RuntimeException("Не удалось загрузить файл application.properties: " + e.getMessage(), e);
    }
  }

  private static final String URL = props.getProperty("db.url");
  private static final String USER = props.getProperty("db.user");
  private static final String PASSWORD = props.getProperty("db.password");

  static {
    if (URL == null || URL.trim().isEmpty()) {
      throw new RuntimeException("Параметр 'db.url' не задан в application.properties");
    }
    if (USER == null || USER.trim().isEmpty()) {
      throw new RuntimeException("Параметр 'db.user' не задан в application.properties");
    }
    if (PASSWORD == null) {
      throw new RuntimeException("Параметр 'db.password' не задан в application.properties");
    }
  }

  /**
   * Создаёт новое соединение с базой данных.
   * @return Новое соединение с PostgreSQL.
   * @throws RuntimeException если подключение не удалось.
   */
  public static Connection getConnection() {
    try {
      Connection conn = DriverManager.getConnection(URL, USER, PASSWORD);
      System.out.println("✅ Подключение к БД успешно");
      return conn;
    } catch (SQLException e) {
      throw new RuntimeException(
          "Не удалось подключиться к базе данных по адресу: " + URL +
              ". Проверьте, что PostgreSQL запущен и параметры подключения верны.",
          e
      );
    }
  }

  /**
   * Инициализирует базу данных: создаёт таблицу telemetry, если она отсутствует.
   * Вызывается при старте приложения.
   * @throws RuntimeException если не удалось создать таблицу.
   */
  public static void initializeDatabase() {
    try (Connection conn = getConnection();
         Statement stmt = conn.createStatement()) {

      String createTelemetryTableSQL = """
                CREATE TABLE IF NOT EXISTS telemetry (
                    id BIGSERIAL PRIMARY KEY,
                    device_id VARCHAR(100) NOT NULL,
                    temperature DOUBLE PRECISION NOT NULL,
                    humidity DOUBLE PRECISION NOT NULL,
                    recorded_at TIMESTAMPTZ DEFAULT NOW()
                );
                """;
      stmt.execute(createTelemetryTableSQL);
      System.out.println("✅ Таблица 'telemetry' создана или уже существует.");

    } catch (SQLException e) {
      throw new RuntimeException(
          "Не удалось инициализировать базу данных. Ошибка при создании таблицы 'telemetry'.",
          e
      );
    }
  }
}