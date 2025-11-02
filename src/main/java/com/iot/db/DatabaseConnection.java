package com.iot.db;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;

public class DatabaseConnection {
  private static final String URL = "jdbc:postgresql://localhost:5432/iot_db";
  private static final String USER = "iot_user";
  private static final String PASSWORD = "iot_pass";

  private static Connection connection = null;

  public static Connection getConnection() {
    try {
      // Создаём новое соединение каждый раз
      Connection conn = DriverManager.getConnection(URL, USER, PASSWORD);
      System.out.println("✅ Успешное подключение к PostgreSQL!");
      return conn;
    } catch (SQLException e) {
      System.err.println("❌ Ошибка подключения к базе данных:");
      e.printStackTrace();
      throw new RuntimeException("Не удалось подключиться к базе данных", e);
    }
  }

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
      e.printStackTrace();
    }
  }
}