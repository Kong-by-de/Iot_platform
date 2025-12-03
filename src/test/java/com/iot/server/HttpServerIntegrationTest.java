package com.iot.server;

import com.iot.db.DatabaseConnection;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.buffer.Unpooled;
import io.netty.channel.*;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.handler.codec.http.*;
import io.netty.util.CharsetUtil;
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
import java.util.concurrent.atomic.AtomicReference;

import static org.assertj.core.api.Assertions.assertThat;

@Testcontainers
class HttpServerIntegrationTest {

  @Container
  private static final PostgreSQLContainer<?> postgres =
      new PostgreSQLContainer<>("postgres:15")
          .withDatabaseName("iot_db")
          .withUsername("iot_user")
          .withPassword("iot_pass");

  private static Thread javaServerThread;
  private static int javaServerPort;
  private static Channel cppMockChannel;
  private static EventLoopGroup bossGroup;
  private static EventLoopGroup workerGroup;
  private static int cppPort;
  private static final AtomicReference<String> lastCppRequest = new AtomicReference<>();

  private static int freePort() throws IOException {
    try (var socket = new java.net.ServerSocket(0)) {
      return socket.getLocalPort();
    }
  }

  @BeforeAll
  static void setUp() throws Exception {
    // Настройка БД через системные свойства
    System.setProperty("db.url", postgres.getJdbcUrl());
    System.setProperty("db.user", postgres.getUsername());
    System.setProperty("db.password", postgres.getPassword());
    DatabaseConnection.initializeDatabase();

    // Запуск мока C++-сервера
    cppPort = freePort();
    startCppMockServer();

    // Говорим приложению: "отправляй на мок"
    System.setProperty("cpp.service.url", "http://127.0.0.1:" + cppPort + "/telemetry");

    // Запуск основного сервера на случайном порту
    javaServerPort = freePort();
    javaServerThread = new Thread(() -> {
      try {
        new HttpServer(javaServerPort).start();
      } catch (Exception e) {
        e.printStackTrace();
      }
    });
    javaServerThread.setDaemon(true);
    javaServerThread.start();
    Thread.sleep(1200); // ждём запуска сервера
  }

  private static void startCppMockServer() throws Exception {
    bossGroup = new NioEventLoopGroup(1);
    workerGroup = new NioEventLoopGroup();
    var bootstrap = new ServerBootstrap();
    bootstrap.group(bossGroup, workerGroup)
        .channel(NioServerSocketChannel.class)
        .childHandler(new ChannelInitializer<SocketChannel>() {
          @Override
          protected void initChannel(SocketChannel ch) {
            ch.pipeline().addLast(
                new HttpServerCodec(),
                new HttpObjectAggregator(65536),
                new SimpleChannelInboundHandler<FullHttpRequest>() {
                  @Override
                  protected void channelRead0(ChannelHandlerContext ctx, FullHttpRequest req) {
                    if (req.method() == HttpMethod.POST && req.uri().equals("/telemetry")) {
                      lastCppRequest.set(req.content().toString(CharsetUtil.UTF_8));
                      FullHttpResponse resp = new DefaultFullHttpResponse(
                          HttpVersion.HTTP_1_1,
                          HttpResponseStatus.OK,
                          Unpooled.copiedBuffer("{\"status\":\"ok\"}", CharsetUtil.UTF_8)
                      );
                      resp.headers().set(HttpHeaderNames.CONTENT_TYPE, "application/json");
                      resp.headers().set(HttpHeaderNames.CONTENT_LENGTH, resp.content().readableBytes());
                      ctx.writeAndFlush(resp);
                    }
                  }
                }
            );
          }
        });
    cppMockChannel = bootstrap.bind(cppPort).sync().channel();
    System.out.println("✅ Mock C++ server running at port " + cppPort);
  }

  @AfterAll
  static void tearDown() {
    if (cppMockChannel != null) cppMockChannel.close();
    if (bossGroup != null) bossGroup.shutdownGracefully();
    if (workerGroup != null) workerGroup.shutdownGracefully();
  }

  @Test
  @DisplayName("Сохранение в БД при недоступности C++-сервера")
  void shouldSaveToDbWhenCppUnavailable() throws Exception {
    // Сохраняем текущий URL
    String originalUrl = System.getProperty("cpp.service.url");
    try {
      // Подменяем на недостижимый адрес → эмулируем "C++ недоступен"
      System.setProperty("cpp.service.url", "http://127.0.0.1:1/telemetry");

      String url = "http://localhost:" + javaServerPort + "/telemetry";
      String json = "{\"device_id\":\"dev_offline\",\"temperature\":22.0,\"humidity\":11.0}";
      var req = HttpRequest.newBuilder()
          .uri(URI.create(url))
          .header("Content-Type", "application/json")
          .POST(HttpRequest.BodyPublishers.ofString(json))
          .build();
      var resp = HttpClient.newHttpClient().send(req, HttpResponse.BodyHandlers.ofString());

      // Сервер вернёт 500, потому что C++ недоступен
      assertThat(resp.statusCode()).isEqualTo(500);

      // Но данные ДОЛЖНЫ быть в БД
      try (Connection conn = DatabaseConnection.getConnection();
           Statement stmt = conn.createStatement()) {
        ResultSet rs = stmt.executeQuery("SELECT * FROM telemetry WHERE device_id = 'dev_offline'");
        assertThat(rs.next()).isTrue();
      }
    } finally {
      // Восстанавливаем URL мока
      System.setProperty("cpp.service.url", originalUrl);
    }
  }

  @Test
  @DisplayName("Успешная отправка телеметрии в C++-сервер и сохранение в БД")
  void shouldForwardTelemetryToCppAndSave() throws Exception {
    Thread.sleep(500); // гарантируем, что всё готово

    String url = "http://localhost:" + javaServerPort + "/telemetry";
    String json = "{\"device_id\":\"dev_online\",\"temperature\":33.0,\"humidity\":44.0}";
    var req = HttpRequest.newBuilder()
        .uri(URI.create(url))
        .header("Content-Type", "application/json")
        .POST(HttpRequest.BodyPublishers.ofString(json))
        .build();
    var resp = HttpClient.newHttpClient().send(req, HttpResponse.BodyHandlers.ofString());

    // Успешный ответ
    assertThat(resp.statusCode()).isEqualTo(200);
    assertThat(resp.body()).contains("saved_and_forwarded");

    // Проверяем, что запрос дошёл до C++-мока
    assertThat(lastCppRequest.get()).contains("dev_online");

    // Проверяем, что данные в БД
    try (Connection conn = DatabaseConnection.getConnection();
         Statement stmt = conn.createStatement()) {
      ResultSet rs = stmt.executeQuery("SELECT * FROM telemetry WHERE device_id = 'dev_online'");
      assertThat(rs.next()).isTrue();
    }
  }
}