package com.iot.db;

import java.io.IOException;
import java.io.InputStream;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.Properties;

/**
 * Класс для управления соединением с базой данных PostgreSQL.
 * На текущем этапе используется только для инициализации таблиц.
 */
public class DatabaseConnection {
  private static final Properties props = new Properties();

  static {
    try (InputStream input = DatabaseConnection.class.getClassLoader().getResourceAsStream("application.properties")) {
      if (input == null) {
        throw new RuntimeException("Unable to find application.properties");
      }
      props.load(input);
    } catch (IOException e) {
      throw new RuntimeException("Error loading properties", e);
    }
  }

  private static final String URL = props.getProperty("db.url");
  private static final String USER = props.getProperty("db.user");
  private static final String PASSWORD = props.getProperty("db.password");

  /**
   * Получает новое соединение с базой данных.
   * @return Соединение с базой данных.
   * @throws RuntimeException если подключение не удалось.
   */
  public static Connection getConnection() {
    try {
      Connection conn = DriverManager.getConnection(URL, USER, PASSWORD);
      System.out.println("✅ Успешное подключение к PostgreSQL!");
      return conn;
    } catch (SQLException e) {
      System.err.println("❌ Ошибка подключения к базе данных:");
      throw new RuntimeException("Не удалось подключиться к базе данных", e);
    }
  }

  /**
   * Инициализирует базу данных, создавая таблицы users и devices, если они не существуют.
   */
  public static void initializeDatabase() {
    try (Connection conn = getConnection();
         Statement stmt = conn.createStatement()) {

      // 1. Создаём таблицу users
      String createUsersTableSQL = """
                CREATE TABLE IF NOT EXISTS users (
                    id SERIAL PRIMARY KEY,
                    name VARCHAR(100) NOT NULL,
                    email VARCHAR(100) UNIQUE NOT NULL
                );
                """;
      stmt.execute(createUsersTableSQL);
      System.out.println("✅ Таблица 'users' создана или уже существует.");

      // 2. Создаём таблицу devices
      String createDevicesTableSQL = """
                CREATE TABLE IF NOT EXISTS devices (
                    id SERIAL PRIMARY KEY,
                    name VARCHAR(100) NOT NULL,
                    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
                    type VARCHAR(50) NOT NULL
                );
                """; /* ON DELETE CASCADE - Если удалится пользователь через DELETE FROM users WHERE id = ?
                 все его устройства автоматически удалятся из таблицы devices */
      stmt.execute(createDevicesTableSQL);
      System.out.println("✅ Таблица 'devices' создана или уже существует.");

    } catch (SQLException e) {
      System.err.println("❌ Ошибка при создании таблиц:");
      throw new RuntimeException("Описание ошибки", e);
    }
  }
}

/*
docker run --name iot-postgres -e POSTGRES_DB=iot_db -e POSTGRES_USER=iot_user -e POSTGRES_PASSWORD=iot_pass -p 5432:5432 -d postgres:16
*/