package com.iot.db;

import java.sql.*;
import java.util.ArrayList;
import java.util.List;

public class UserDao {

  public boolean createUser(String name, String email) {
    String sql = "INSERT INTO users(name, email) VALUES (?, ?)"; //стандартный синтаксис SQL для вставки данных
    try (Connection conn = DatabaseConnection.getConnection();
         PreparedStatement pstmt = conn.prepareStatement(sql)) { //prepareStatement реализует интерфейс AutoCloseable, поэтому в конце try вызовется .close()
      pstmt.setString(1, name); //подставляет значение переменной name вместо первого ? в SQL-запросе
      pstmt.setString(2, email); //подставляет значение переменной name вместо второго ? в SQL-запросе
      pstmt.executeUpdate(); //изменяет данные и возвращает кол-во измененных строк, либо выбрасывает ошибку
      return true;
    } catch (SQLException e) {
      e.printStackTrace(); //выводит в консоль стек вызовов — информацию об ошибке, включая место, где она произошла
      return false;
    }
  }

  public List<String> getAllUsers() {
    List<String> users = new ArrayList<>();
    String sql = "SELECT id, name, email FROM users";
    try (Connection conn = DatabaseConnection.getConnection();
         Statement stmt = conn.createStatement(); //обычный statement, потому что нет параметров в запросе
         ResultSet rs = stmt.executeQuery(sql)) { //результат выполнения SELECT
      while (rs.next()) { //возвращает true, если строка есть, и false — если достигнут конец
        users.add(String.format( //формирование JSON-подобной строки и добавление её в список
            "{\"id\":%d, \"name\":\"%s\", \"email\":\"%s\"}",
            rs.getInt("id"),
            rs.getString("name"),
            rs.getString("email")
        ));
      }
    } catch (SQLException e) {
      e.printStackTrace();
    }
    return users;
  }

  public String getUserById(int id) {
    String sql = "SELECT id, name, email FROM users WHERE id = ?";
    try (Connection conn = DatabaseConnection.getConnection();
         PreparedStatement pstmt = conn.prepareStatement(sql)) {
      pstmt.setInt(1, id);
      ResultSet rs = pstmt.executeQuery();
      if (rs.next()) {
        return String.format(
            "{\"id\":%d, \"name\":\"%s\", \"email\":\"%s\"}",
            rs.getInt("id"),
            rs.getString("name"),
            rs.getString("email")
        );
      }
    } catch (SQLException e) {
      e.printStackTrace();
    }
    return null;
  }

  public boolean updateUser(int id, String name, String email) {
    String sql = "UPDATE users SET name = ?, email = ? WHERE id = ?";
    try (Connection conn = DatabaseConnection.getConnection();
         PreparedStatement pstmt = conn.prepareStatement(sql)) {
      pstmt.setString(1, name);
      pstmt.setString(2, email);
      pstmt.setInt(3, id);
      return pstmt.executeUpdate() > 0;
    } catch (SQLException e) {
      e.printStackTrace();
      return false;
    }
  }

  public boolean deleteUser(int id) {
    String sql = "DELETE FROM users WHERE id = ?";
    try (Connection conn = DatabaseConnection.getConnection();
         PreparedStatement pstmt = conn.prepareStatement(sql)) {
      pstmt.setInt(1, id);
      return pstmt.executeUpdate() > 0;
    } catch (SQLException e) {
      e.printStackTrace();
      return false;
    }
  }
}