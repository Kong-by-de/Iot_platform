package com.iot.db;

import java.sql.*;
import java.util.ArrayList;
import java.util.List;

public class DeviceDao {

  public boolean createDevice(String name, int userId, String type) {
    String sql = "INSERT INTO devices(name, user_id, type) VALUES (?, ?, ?)";
    try (Connection conn = DatabaseConnection.getConnection();
         PreparedStatement pstmt = conn.prepareStatement(sql)) {
      pstmt.setString(1, name);
      pstmt.setInt(2, userId);
      pstmt.setString(3, type);
      pstmt.executeUpdate();
      return true;
    } catch (SQLException e) {
      e.printStackTrace();
      return false;
    }
  }

  public List<String> getAllDevices() {
    List<String> devices = new ArrayList<>();
    String sql = "SELECT d.id, d.name, d.type, u.name as user_name FROM devices d JOIN users u ON d.user_id = u.id";
    try (Connection conn = DatabaseConnection.getConnection();
         Statement stmt = conn.createStatement();
         ResultSet rs = stmt.executeQuery(sql)) {
      while (rs.next()) {
        devices.add(String.format(
            "{\"id\":%d, \"name\":\"%s\", \"type\":\"%s\", \"user_name\":\"%s\"}",
            rs.getInt("id"),
            rs.getString("name"),
            rs.getString("type"),
            rs.getString("user_name")
        ));
      }
    } catch (SQLException e) {
      e.printStackTrace();
    }
    return devices;
  }

  public String getDeviceById(int id) {
    String sql = "SELECT d.id, d.name, d.type, u.name as user_name FROM devices d JOIN users u ON d.user_id = u.id WHERE d.id = ?";
    try (Connection conn = DatabaseConnection.getConnection();
         PreparedStatement pstmt = conn.prepareStatement(sql)) {
      pstmt.setInt(1, id);
      ResultSet rs = pstmt.executeQuery();
      if (rs.next()) {
        return String.format(
            "{\"id\":%d, \"name\":\"%s\", \"type\":\"%s\", \"user_name\":\"%s\"}",
            rs.getInt("id"),
            rs.getString("name"),
            rs.getString("type"),
            rs.getString("user_name")
        );
      }
    } catch (SQLException e) {
      e.printStackTrace();
    }
    return null;
  }

  public boolean updateDevice(int id, String name, String type) {
    String sql = "UPDATE devices SET name = ?, type = ? WHERE id = ?";
    try (Connection conn = DatabaseConnection.getConnection();
         PreparedStatement pstmt = conn.prepareStatement(sql)) {
      pstmt.setString(1, name);
      pstmt.setString(2, type);
      pstmt.setInt(3, id);
      return pstmt.executeUpdate() > 0;
    } catch (SQLException e) {
      e.printStackTrace();
      return false;
    }
  }

  public boolean deleteDevice(int id) {
    String sql = "DELETE FROM devices WHERE id = ?";
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