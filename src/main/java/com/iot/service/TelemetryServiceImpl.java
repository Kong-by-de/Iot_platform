package com.iot.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.iot.config.Config;
import com.iot.db.TelemetryDao;
import com.iot.server.TelemetryRequest;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;

/**
 * Реализация сервиса обработки телеметрии.
 */
public class TelemetryServiceImpl implements TelemetryService {

  private static final Logger logger = LoggerFactory.getLogger(TelemetryServiceImpl.class);

  private final TelemetryDao telemetryDao;
  private final HttpClient httpClient;
  private final ObjectMapper objectMapper;

  /**
   * Конструктор сервиса.
   *
   * @param telemetryDao DAO для работы с БД.
   * @param httpClient HTTP-клиент для отправки данных в C++-сервер.
   */
  public TelemetryServiceImpl(TelemetryDao telemetryDao, HttpClient httpClient) {
    this.telemetryDao = telemetryDao;
    this.httpClient = httpClient;
    this.objectMapper = new ObjectMapper();
  }

  @Override
  public boolean processTelemetry(TelemetryRequest data) {
    if (data == null) {
      throw new IllegalArgumentException("Telemetry data cannot be null");
    }

    // 1. Сохраняем в БД
    boolean saved = telemetryDao.saveTelemetry(
        data.getDevice_id(),
        data.getTemperature(),
        data.getHumidity()
    );

    if (!saved) {
      logger.error("❌ Не удалось сохранить телеметрию в БД. Устройство: {}", data.getDevice_id());
      return false;
    }

    // 2. Отправляем в C++-сервер
    try {
      forwardToCppService(data);
      logger.info("✅ Телеметрия от {} успешно обработана", data.getDevice_id());
      return true;
    } catch (Exception e) {
      logger.error("❌ Ошибка при отправке телеметрии в C++-сервер. Устройство: {}", data.getDevice_id(), e);
      return false;
    }
  }

  private void forwardToCppService(TelemetryRequest data) throws IOException, InterruptedException {
    String jsonBody = objectMapper.writeValueAsString(data);
    String url = Config.getRequiredProperty("cpp.service.url");

    HttpRequest request = HttpRequest.newBuilder()
        .uri(URI.create(url))
        .header("Content-Type", "application/json")
        .POST(HttpRequest.BodyPublishers.ofString(jsonBody))
        .timeout(Duration.ofSeconds(10))
        .build();

    HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());

    if (response.statusCode() >= 400) {
      throw new RuntimeException("C++-сервер вернул ошибку: " + response.statusCode());
    }
  }
}