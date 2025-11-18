package com.iot.server;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.iot.db.TelemetryDao;
import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.handler.codec.http.*;
import io.netty.util.CharsetUtil;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;

/**
 * Обработчик HTTP-запросов.
 * Принимает POST /telemetry, сохраняет данные в БД и пересылает их в C++-сервер (iot_core).
 */
public class HttpServerHandler extends SimpleChannelInboundHandler<FullHttpRequest> {

  private static final HttpClient HTTP_CLIENT = HttpClient.newBuilder()
      .connectTimeout(Duration.ofSeconds(5))
      .build();

  private final TelemetryDao telemetryDao = new TelemetryDao();

  /**
   * Обрабатывает входящий HTTP-запрос.
   * Поддерживается только POST /telemetry.
   * @param ctx Контекст канала Netty.
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

        // 1. Сохраняем в локальную БД
        boolean saved = telemetryDao.saveTelemetry(
            data.getDevice_id(),
            data.getTemperature(),
            data.getHumidity()
        );

        // 2. Отправляем в C++-сервер (iot_core)
        forwardToCppService(data);

        if (saved) {
          response = createJsonResponse(HttpResponseStatus.OK, "{\"status\":\"saved_and_forwarded\"}");
        } else {
          response = createJsonResponse(HttpResponseStatus.INTERNAL_SERVER_ERROR, "{\"error\":\"DB save failed\"}");
        }
      } catch (Exception e) {
        e.printStackTrace();
        response = createJsonResponse(HttpResponseStatus.BAD_REQUEST, "{\"error\":\"Invalid telemetry\"}");
      }
    } else {
      response = createJsonResponse(HttpResponseStatus.NOT_FOUND, "{\"error\":\"404\"}");
    }

    ctx.writeAndFlush(response).addListener(ChannelFutureListener.CLOSE);
  }

  /**
   * Отправляет телеметрию в C++-сервер (iot_core) по HTTP.
   * @param data Данные телеметрии.
   * @throws IOException если возникла ошибка ввода-вывода.
   * @throws InterruptedException если поток был прерван.
   */
  private void forwardToCppService(TelemetryRequest data) throws IOException, InterruptedException {
    ObjectMapper mapper = new ObjectMapper();
    String jsonBody = mapper.writeValueAsString(data);

    // ⚠️ Замените IP на реальный адрес ноутбука с iot_core
    String url = "http://192.168.1.35:8080/telemetry";

    HttpRequest request = HttpRequest.newBuilder()
        .uri(URI.create(url))
        .header("Content-Type", "application/json")
        .POST(HttpRequest.BodyPublishers.ofString(jsonBody))
        .build();

    HttpResponse<String> response = HTTP_CLIENT.send(request, HttpResponse.BodyHandlers.ofString());
    System.out.println("📨 iot_core response: " + response.statusCode());
  }

  /**
   * Создаёт HTTP-ответ с заданным статусом и телом в формате JSON.
   * @param status HTTP-статус ответа.
   * @param body Тело ответа в виде JSON-строки.
   * @return Сформированный HTTP-ответ.
   */
  private FullHttpResponse createJsonResponse(HttpResponseStatus status, String body) {
    FullHttpResponse res = new DefaultFullHttpResponse(
        HttpVersion.HTTP_1_1,
        status,
        Unpooled.copiedBuffer(body, CharsetUtil.UTF_8)
    );
    res.headers().set(HttpHeaderNames.CONTENT_TYPE, "application/json; charset=UTF-8");
    res.headers().set(HttpHeaderNames.CONTENT_LENGTH, res.content().readableBytes());
    return res;
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