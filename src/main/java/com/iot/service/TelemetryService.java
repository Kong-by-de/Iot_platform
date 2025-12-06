package com.iot.service;

import com.iot.server.TelemetryRequest;

/**
 * Сервис для обработки телеметрии от устройств.
 * <p>
 * Отвечает за сохранение данных в БД и пересылку их на C++-сервер (iot_core).
 */
public interface TelemetryService {

  /**
   * Обрабатывает входящую телеметрию: сохраняет в БД и отправляет в C++-сервер.
   *
   * @param data Объект с данными телеметрии.
   * @return true, если оба этапа (сохранение и отправка) завершились успешно.
   * @throws RuntimeException если произошла ошибка при обработке.
   */
  boolean processTelemetry(TelemetryRequest data);
}