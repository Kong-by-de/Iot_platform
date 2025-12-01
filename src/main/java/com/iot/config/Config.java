package com.iot.config;

import java.io.IOException;
import java.io.InputStream;
import java.util.Properties;

/**
 * Утилитарный класс для загрузки конфигурационных параметров из файла application.properties.
 * <p>
 * Все параметры загружаются при старте приложения и доступны через статические методы.
 */
public class Config {

  /**
   * Статический объект Properties, содержащий все настройки из application.properties.
   */
  private static final Properties props = new Properties();

  static {
    try (InputStream input = Config.class.getClassLoader()
        .getResourceAsStream("application.properties")) {
      if (input == null) {
        throw new RuntimeException("Файл application.properties не найден в classpath.");
      }
      props.load(input);
    } catch (IOException e) {
      throw new RuntimeException("Не удалось загрузить application.properties", e);
    }
  }

  /**
   * Получает URL C++-сервера (iot_core) из конфигурации.
   *
   * @return URL сервера в виде строки, например "http://localhost:8080/telemetry".
   * @throws RuntimeException если параметр 'cpp.service.url' не задан или пуст.
   */
  public static String getCppServiceUrl() {
    String url = props.getProperty("cpp.service.url");
    if (url == null || url.trim().isEmpty()) {
      throw new RuntimeException("Не задан параметр 'cpp.service.url' в application.properties");
    }
    return url.trim();
  }
}