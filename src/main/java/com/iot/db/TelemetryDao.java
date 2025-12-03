package com.iot.db;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.SQLException;

/**
 * DAO-класс для операций с таблицей telemetry.
 * Отвечает за сохранение телеметрии в базу данных.
 */
public class TelemetryDao {

  /**
   * Сохраняет телеметрию в таблицу telemetry.
   * @param device_id Идентификатор устройства.
   * @param temperature Значение температуры.
   * @param humidity Значение влажности.
   * @return true, если запись успешно сохранена; выброс exception в случае ошибки.
   */
  public boolean saveTelemetry(String device_id, double temperature, double humidity) {
    String sql = "INSERT INTO telemetry (device_id, temperature, humidity) VALUES (?, ?, ?)";
    try (Connection conn = DatabaseConnection.getConnection();
         PreparedStatement pstmt = conn.prepareStatement(sql)) {
      pstmt.setString(1, device_id);
      pstmt.setDouble(2, temperature);
      pstmt.setDouble(3, humidity);
      pstmt.executeUpdate();
      return true;
    } catch (SQLException e) {
      throw new RuntimeException("Не удалось сохранить телеметрию в базу данных. " +
          "Устройство: " + device_id + ", Температура: " + temperature + ", Влажность: " + humidity, e);
    }
  }
}