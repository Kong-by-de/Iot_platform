package com.iot.server;

/**
 * Класс для хранения данных телеметрии, полученных от устройства.
 * Используется для парсинга JSON-запроса.
 */
public class TelemetryRequest {
  private String device_id;
  private double temperature;
  private double humidity;

  /**
   * Конструктор по умолчанию. Нужен для работы с Jackson.
   */
  public TelemetryRequest() {}

  /**
   * Получает ID устройства.
   * @return ID устройства.
   */
  public String getDevice_id() { return device_id; }

  /**
   * Устанавливает ID устройства.
   * @param device_id Новое значение ID устройства.
   */
  public void setDevice_id(String device_id) { this.device_id = device_id; }

  /**
   * Получает значение температуры.
   * @return Температура в градусах Цельсия.
   */
  public double getTemperature() { return temperature; }

  /**
   * Устанавливает значение температуры.
   * @param temperature Новое значение температуры.
   */
  public void setTemperature(double temperature) { this.temperature = temperature; }

  /**
   * Получает значение влажности.
   * @return Влажность в процентах.
   */
  public double getHumidity() { return humidity; }

  /**
   * Устанавливает значение влажности.
   * @param humidity Новое значение влажности.
   */
  public void setHumidity(double humidity) { this.humidity = humidity; }
}