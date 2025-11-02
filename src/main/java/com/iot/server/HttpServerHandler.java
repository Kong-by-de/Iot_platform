package com.iot.server;

import com.iot.db.DeviceDao;
import com.iot.db.UserDao;
import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.handler.codec.http.*;
import io.netty.util.CharsetUtil;

import java.util.List;

public class HttpServerHandler extends SimpleChannelInboundHandler<FullHttpRequest> { //наследование для автоматического вызова метода channelRead0

  private final UserDao userDao = new UserDao();
  private final DeviceDao deviceDao = new DeviceDao();

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
        // Здесь можно парсить JSON из request.content(), но пока — фиктивные данные
        String uniqueEmail = "test_" + System.currentTimeMillis() + "@example.com";
        boolean created = userDao.createUser("Тестовый пользователь", uniqueEmail);
        if (created) {
          response = createResponse(HttpResponseStatus.CREATED, "{\"status\":\"created\"}");
        } else {
          response = createResponse(HttpResponseStatus.INTERNAL_SERVER_ERROR, "{\"error\":\"DB error\"}");
        }
      } else if (method == HttpMethod.GET && uri.matches("/users/\\d+")) {
        int id = Integer.parseInt(extractId(uri));
        String userJson = userDao.getUserById(id);
        if (userJson != null) {
          response = createResponse(HttpResponseStatus.OK, userJson);
        } else {
          response = createResponse(HttpResponseStatus.NOT_FOUND, "{\"error\":\"User not found\"}");
        }
      } else if (method == HttpMethod.GET && uri.equals("/users")) {
        List<String> users = userDao.getAllUsers();
        String body = "[" + String.join(",", users) + "]";
        response = createResponse(HttpResponseStatus.OK, body);
      } else if (method == HttpMethod.DELETE && uri.matches("/users/\\d+")) {
        int id = Integer.parseInt(extractId(uri));
        boolean deleted = userDao.deleteUser(id);
        if (deleted) {
          response = createResponse(HttpResponseStatus.OK, "{\"status\":\"deleted\"}");
        } else {
          response = createResponse(HttpResponseStatus.NOT_FOUND, "{\"error\":\"User not found\"}");
        }
      } else {
        response = createResponse(HttpResponseStatus.NOT_IMPLEMENTED, "{\"error\":\"Not implemented yet\"}");
      }
    }
    //devices
    else if (uri.startsWith("/devices")) {
      if (method == HttpMethod.POST && uri.equals("/devices")) {
        // Здесь можно парсить JSON из request.content(), но пока — фиктивные данные
        boolean created = deviceDao.createDevice("Тестовое устройство", 10, "thermometer");
        if (created) {
          response = createResponse(HttpResponseStatus.CREATED, "{\"status\":\"created\"}");
        } else {
          response = createResponse(HttpResponseStatus.INTERNAL_SERVER_ERROR, "{\"error\":\"DB error\"}");
        }
      } else if (method == HttpMethod.GET && uri.matches("/devices/\\d+")) {
        int id = Integer.parseInt(extractId(uri));
        String deviceJson = deviceDao.getDeviceById(id);
        if (deviceJson != null) {
          response = createResponse(HttpResponseStatus.OK, deviceJson);
        } else {
          response = createResponse(HttpResponseStatus.NOT_FOUND, "{\"error\":\"Device not found\"}");
        }
      } else if (method == HttpMethod.GET && uri.equals("/devices")) {
        List<String> devices = deviceDao.getAllDevices();
        String body = "[" + String.join(",", devices) + "]";
        response = createResponse(HttpResponseStatus.OK, body);
      } else if (method == HttpMethod.PUT && uri.matches("/devices/\\d+")) {
        int id = Integer.parseInt(extractId(uri));
        boolean updated = deviceDao.updateDevice(id, "Обновленное устройство", "humidity_sensor");
        if (updated) {
          response = createResponse(HttpResponseStatus.OK, "{\"status\":\"updated\"}");
        } else {
          response = createResponse(HttpResponseStatus.NOT_FOUND, "{\"error\":\"Device not found or not updated\"}");
        }
      } else if (method == HttpMethod.DELETE && uri.matches("/devices/\\d+")) {
        int id = Integer.parseInt(extractId(uri));
        boolean deleted = deviceDao.deleteDevice(id);
        if (deleted) {
          response = createResponse(HttpResponseStatus.OK, "{\"status\":\"deleted\"}");
        } else {
          response = createResponse(HttpResponseStatus.NOT_FOUND, "{\"error\":\"Device not found\"}");
        }
      } else {
        response = createResponse(HttpResponseStatus.NOT_IMPLEMENTED, "{\"error\":\"Not implemented yet\"}");
      }
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