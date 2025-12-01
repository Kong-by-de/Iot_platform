package com.iot.server;

import com.iot.config.Config;
import com.iot.db.DatabaseConnection;
import com.iot.db.TelemetryDao;
import com.iot.service.TelemetryService;
import com.iot.service.TelemetryServiceImpl;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.Channel;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.handler.codec.http.HttpObjectAggregator;
import io.netty.handler.codec.http.HttpServerCodec;
import org.junit.jupiter.api.*;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.Statement;

import static org.assertj.core.api.Assertions.assertThat;

/**
 * Интеграционные тесты для HTTP-сервера.
 * <p>
 * Запускает реальный Netty-сервер и PostgreSQL в Docker-контейнере.
 * Проверяет сквозной сценарий: HTTP-запрос → сохранение в БД.
 * <p>
 * C++-сервер мокается через подмену URL в конфигурации.
 */
@Testcontainers
class HttpServerIntegrationTest {

  /**
   * Запускает PostgreSQL в Docker-контейнере.
   */
  @Container
  private static final PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:15")
      .withDatabaseName("iot_db")
      .withUsername("iot_user")
      .withPassword("iot_pass");

  /**
   * Порт, на котором запускается тестовый Netty-сервер.
   */
  private static int serverPort;

  /**
   * Ссылка на запущенный серверный канал (для корректного закрытия).
   */
  private static Channel serverChannel;

  /**
   * Группы потоков Netty.
   */
  private static EventLoopGroup bossGroup;
  private static EventLoopGroup workerGroup;

  /**
   * Настройка окружения перед всеми тестами.
   */
  @BeforeAll
  static void setUpAll() throws Exception {
    // Перенаправляем конфигурацию БД на контейнер
    System.setProperty("db.url", postgres.getJdbcUrl());
    System.setProperty("db.user", postgres.getUsername());
    System.setProperty("db.password", postgres.getPassword());

    // Явно запускаем контейнер и ждём его готовности
    postgres.start();
    postgres.waitingFor(
        org.testcontainers.containers.wait.strategy.Wait.forListeningPort()
            .withStartupTimeout(java.time.Duration.ofSeconds(60)) // Увеличили таймаут до 60 секунд
    );

    // Логируем URL для отладки
    System.out.println("✅ PostgreSQL JDBC URL: " + postgres.getJdbcUrl());

    // Инициализируем БД
    DatabaseConnection.initializeDatabase();

    // Запускаем сервер на случайном порту
    bossGroup = new NioEventLoopGroup();
    workerGroup = new NioEventLoopGroup();
    ServerBootstrap bootstrap = new ServerBootstrap();
    bootstrap.group(bossGroup, workerGroup)
        .channel(NioServerSocketChannel.class)
        .childHandler(new ChannelInitializer<SocketChannel>() {
          @Override
          protected void initChannel(SocketChannel ch) {
            TelemetryDao telemetryDao = new TelemetryDao();
            HttpClient httpClient = HttpClient.newBuilder()
                .connectTimeout(java.time.Duration.ofSeconds(5))
                .build();
            TelemetryService telemetryService = new TelemetryServiceImpl(telemetryDao, httpClient);
            ch.pipeline()
                .addLast(new HttpServerCodec())
                .addLast(new HttpObjectAggregator(65536))
                .addLast(new HttpServerHandler(telemetryService));
          }
        })
        .option(ChannelOption.SO_BACKLOG, 128)
        .childOption(ChannelOption.SO_KEEPALIVE, true);

    serverChannel = bootstrap.bind(0).sync().channel();
    serverPort = ((java.net.InetSocketAddress) serverChannel.localAddress()).getPort();
    System.out.println("✅ Интеграционный сервер запущен на порту " + serverPort);
  }

  /**
   * Корректное завершение после всех тестов.
   */
  @AfterAll
  static void tearDownAll() {
    if (serverChannel != null) {
      serverChannel.close().syncUninterruptibly();
    }
    if (workerGroup != null) {
      workerGroup.shutdownGracefully();
    }
    if (bossGroup != null) {
      bossGroup.shutdownGracefully();
    }
  }

  /**
   * Проверяет, что телеметрия сохраняется в БД при успешном запросе.
   * <p>
   * Примечание: отправка в C++ завершится ошибкой (т.к. сервера нет),
   * поэтому ожидаем статус 500, но данные должны быть в БД.
   */
  @Test
  @DisplayName("Телеметрия сохраняется в БД даже если C++-сервер недоступен")
  void shouldSaveTelemetryToDatabaseDespiteCppFailure() throws Exception {
    // Given
    String jsonBody = "{\"device_id\":\"sensor_test\",\"temperature\":22.0,\"humidity\":45.0}";
    String url = "http://localhost:" + serverPort + "/telemetry";

    HttpRequest request = HttpRequest.newBuilder()
        .uri(URI.create(url))
        .header("Content-Type", "application/json")
        .POST(HttpRequest.BodyPublishers.ofString(jsonBody))
        .build();

    // When
    HttpClient client = HttpClient.newHttpClient();
    HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());

    // Then
    // Ожидаем 500, потому что C++-сервер недоступен
    assertThat(response.statusCode()).isEqualTo(500);

    // Но данные ДОЛЖНЫ быть в БД
    try (Connection conn = DatabaseConnection.getConnection();
         Statement stmt = conn.createStatement()) {

      ResultSet rs = stmt.executeQuery("SELECT * FROM telemetry WHERE device_id = 'sensor_test'");
      assertThat(rs.next()).isTrue();
      assertThat(rs.getString("device_id")).isEqualTo("sensor_test");
      assertThat(rs.getDouble("temperature")).isEqualTo(22.0);
      assertThat(rs.getDouble("humidity")).isEqualTo(45.0);
    }
  }

  /**
   * Проверяет, что валидация JSON работает на реальном сервере.
   */
  @Test
  @DisplayName("Некорректный JSON → 400 Bad Request (интеграционный)")
  void shouldReturnBadRequestOnInvalidJsonIntegration() throws Exception {
    // Given
    String invalidJson = "{invalid";
    String url = "http://localhost:" + serverPort + "/telemetry";

    HttpRequest request = HttpRequest.newBuilder()
        .uri(URI.create(url))
        .header("Content-Type", "application/json")
        .POST(HttpRequest.BodyPublishers.ofString(invalidJson))
        .build();

    // When
    HttpClient client = HttpClient.newHttpClient();
    HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());

    // Then
    assertThat(response.statusCode()).isEqualTo(400);
  }
}