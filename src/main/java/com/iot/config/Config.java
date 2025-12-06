package com.iot.config;

import java.io.IOException;
import java.io.InputStream;
import java.util.Properties;

/**
 * Утилитарный класс для загрузки конфигурации из application.properties.
 * <p>
 * Все параметры читаются из classpath-файла "application.properties".
 * Поддерживает обязательные и опциональные параметры.
 */
public final class Config {

  private static final Properties PROPS = new Properties();

  static {
    try (InputStream input = Config.class.getClassLoader()
        .getResourceAsStream("application.properties")) {
      if (input == null) {
        throw new IllegalStateException("Файл application.properties не найден в classpath.");
      }
      PROPS.load(input);
    } catch (IOException e) {
      throw new IllegalStateException("Не удалось загрузить application.properties", e);
    }
  }

  /**
   * Возвращает значение обязательного параметра по ключу.
   * <p>
   * Если параметр отсутствует или пуст — бросает исключение.
   *
   * @param key Ключ параметра (например, "cpp.service.url").
   * @return Непустое строковое значение.
   * @throws IllegalStateException если параметр не задан или пуст.
   */
  public static String getRequiredProperty(String key) {
    // Сначала пробуем системное свойство
    String sysValue = System.getProperty(key);
    if (sysValue != null && !sysValue.trim().isEmpty()) {
      return sysValue.trim();
    }
    // Иначе — из application.properties
    String propValue = PROPS.getProperty(key);
    if (propValue == null || propValue.trim().isEmpty()) {
      throw new IllegalStateException("Обязательный параметр '" + key + "' не задан ни в системных свойствах, ни в application.properties");
    }
    return propValue.trim();
  }

  // Запрещаем создание экземпляров
  private Config() {
    throw new UnsupportedOperationException("Utility class");
  }
}