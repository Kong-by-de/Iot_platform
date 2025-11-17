package com.iot.server;

import com.fasterxml.jackson.databind.ObjectMapper;
import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.handler.codec.http.*;
import io.netty.util.CharsetUtil;

import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.util.Properties;

/**
 * Обработчик HTTP-запросов. Принимает POST-запросы на /telemetry и отправляет данные в Telegram Bot API.
 */
public class HttpServerHandler extends SimpleChannelInboundHandler<FullHttpRequest> {

  // Создаём HTTP-клиент
  private static final HttpClient HTTP_CLIENT = HttpClient.newBuilder()
      .connectTimeout(Duration.ofSeconds(5))
      .build();

  // Токен Telegram-бота
  private static final String TELEGRAM_BOT_TOKEN = loadTelegramToken();

  /**
   * Обрабатывает входящий HTTP-запрос.
   * @param ctx Контекст канала.
   * @param request Полный HTTP-запрос.
   */
  @Override
  protected void channelRead0(ChannelHandlerContext ctx, FullHttpRequest request) {
    String uri = request.uri();
    HttpMethod method = request.method();

    System.out.println("📥 " + method + " " + uri);

    FullHttpResponse response;

    if (method == HttpMethod.POST && "/telemetry".equals(uri)) {
      try {
        String body = request.content().toString(CharsetUtil.UTF_8);
        TelemetryRequest data = new ObjectMapper().readValue(body, TelemetryRequest.class);

        sendToCppService(data);
        response = createJsonResponse(HttpResponseStatus.OK, "{\"status\":\"forwarded\"}");

      } catch (Exception e) {
        e.printStackTrace(); // ОК: локальная отладка
        response = createJsonResponse(HttpResponseStatus.BAD_REQUEST, "{\"error\":\"Invalid telemetry\"}");
      }
    } else {
      response = createJsonResponse(HttpResponseStatus.NOT_FOUND, "{\"error\":\"404\"}");
    }

    ctx.writeAndFlush(response).addListener(ChannelFutureListener.CLOSE);
  }

  /**
   * Отправляет данные телеметрии в Telegram Bot API.
   * @param data Данные телеметрии.
   * @throws IOException если возникла ошибка ввода-вывода.
   * @throws InterruptedException если поток был прерван.
   */
  private void sendToCppService(TelemetryRequest data) throws IOException, InterruptedException {
    // Используем %s, потому что device_id — это String
    String message = String.format(
        "🌡️ Новые данные:\nУстройство: %s\nТемпература: %.1f°C\nВлажность: %.1f%%",
        data.getDevice_id(),      // ← %s для String
        data.getTemperature(),    // ← %.1f для double
        data.getHumidity()        // ← %.1f для double
    );

    String url = "http://192.168.1.32:8080/send-notification";
    String json = String.format("{\"text\": \"%s\"}", message.replace("\"", "\\\""));

    HttpRequest req = HttpRequest.newBuilder()
        .uri(URI.create(url))
        .header("Content-Type", "application/json")
        .POST(HttpRequest.BodyPublishers.ofString(json))
        .build();

    HttpResponse<String> res = HTTP_CLIENT.send(req, HttpResponse.BodyHandlers.ofString());
    System.out.println("📤 Telegram response: " + res.statusCode());
  }

  /**
   * Создаёт HTTP-ответ с указанным статусом и телом.
   * @param status Статус ответа.
   * @param body Тело ответа в формате JSON.
   * @return HTTP-ответ.
   */
  private FullHttpResponse createJsonResponse(HttpResponseStatus status, String body) {
    FullHttpResponse res = new DefaultFullHttpResponse(
        HttpVersion.HTTP_1_1, status,
        Unpooled.copiedBuffer(body, CharsetUtil.UTF_8)
    );
    res.headers().set(HttpHeaderNames.CONTENT_TYPE, "application/json; charset=UTF-8");
    res.headers().set(HttpHeaderNames.CONTENT_LENGTH, res.content().readableBytes());
    return res;
  }

  /**
   * Загружает токен Telegram-бота из файла application.properties.
   * @return Токен бота.
   * @throws RuntimeException если файл properties не найден или токен не задан.
   */
  private static String loadTelegramToken() {
    try (InputStream is = HttpServerHandler.class.getClassLoader()
        .getResourceAsStream("application.properties")) {
      Properties props = new Properties();
      props.load(is);
      String token = props.getProperty("telegram.bot.token");
      if (token == null || token.trim().isEmpty()) {
        throw new RuntimeException("Параметр 'telegram.bot.token' не задан в application.properties");
      }
      return token;
    } catch (Exception e) {
      throw new RuntimeException("Не удалось загрузить telegram.bot.token", e);
    }
  }

  /**
   * Обрабатывает исключения, возникшие при обработке запроса.
   * @param ctx Контекст канала.
   * @param cause Причина исключения.
   */
  @Override
  public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) {
    cause.printStackTrace();
    ctx.close();
  }
}