package com.iot.server;

import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.handler.codec.http.*;
import io.netty.util.CharsetUtil;

public class HttpServerHandler extends SimpleChannelInboundHandler<FullHttpRequest> { //наследование для автоматического вызова метода channelRead0

  @Override
  protected void channelRead0(ChannelHandlerContext ctx, FullHttpRequest request) { //ctx - контекст канала (через него отправляем ответ); request — сам HTTP-запрос (с URI, методом, заголовками, телом)
    String uri = request.uri(); //request.uri() — возвращает путь запроса, например: /users/123
    HttpMethod method = request.method(); //request.method() — возвращает HTTP-метод: GET, POST, PUT, DELETE

    System.out.println("Запрос: " + method + " " + uri);

    FullHttpResponse response; //response — объект, который будет отправлен клиенту (Postman, браузер и т.д.)
    String responseBody = ""; //responseBody — строка с JSON-данными, которые станут телом ответа

    //users
    if (uri.startsWith("/users")) {
      if (method == HttpMethod.POST && uri.equals("/users")) {
        responseBody = "{\"id\": 1, \"name\": \"Новый пользователь\", \"status\": \"created\"}";
        response = createResponse(HttpResponseStatus.CREATED, responseBody);
      } else if (method == HttpMethod.GET && uri.matches("/users/\\d+")) {
        responseBody = "{\"id\": " + extractId(uri) + ", \"name\": \"Пользователь\", \"email\": \"user@example.com\"}"; //extractId(uri) - получить ID из URI
        response = createResponse(HttpResponseStatus.OK, responseBody);
      } else if (method == HttpMethod.PUT && uri.matches("/users/\\d+")) {
        responseBody = "{\"id\": " + extractId(uri) + ", \"status\": \"updated\"}";
        response = createResponse(HttpResponseStatus.OK, responseBody);
      } else if (method == HttpMethod.DELETE && uri.matches("/users/\\d+")) {
        responseBody = "{\"status\": \"deleted\"}";
        response = createResponse(HttpResponseStatus.OK, responseBody);
      } else {
        response = createResponse(HttpResponseStatus.BAD_REQUEST, "{\"error\": \"Неверный запрос к /users\"}");
      }
    }
    //надо будет аналогично сделать для devices
    else if (uri.equals("/test") && method == HttpMethod.GET) {
      response = createResponse(HttpResponseStatus.OK, "{\"message\": \"Тест работает\"}");
    }
    else {
      response = createResponse(HttpResponseStatus.NOT_FOUND, "{\"error\": \"404\"}");
    }

    ctx.writeAndFlush(response).addListener(ChannelFutureListener.CLOSE);
  }

  private FullHttpResponse createResponse(HttpResponseStatus status, String body) {
    FullHttpResponse response = new DefaultFullHttpResponse(
        HttpVersion.HTTP_1_1,
        status,
        Unpooled.copiedBuffer(body, CharsetUtil.UTF_8) //оборачивает строку body в байтовый буфер с кодировкой UTF-8
    );
    response.headers().set(HttpHeaderNames.CONTENT_TYPE, "application/json; charset=UTF-8"); //клиент поймёт, что это JSON
    response.headers().set(HttpHeaderNames.CONTENT_LENGTH, response.content().readableBytes());
    return response;
  }

  private String extractId(String uri) {
    return uri.substring(uri.lastIndexOf('/') + 1); //извлечение ID из URI
  }

  @Override
  public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) {
    cause.printStackTrace();
    ctx.close();
  }
}