package com.iot.server;

import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.handler.codec.http.*;
import io.netty.util.CharsetUtil;

public class HttpServerHandler extends SimpleChannelInboundHandler<FullHttpRequest> {

  @Override
  protected void channelRead0(ChannelHandlerContext ctx, FullHttpRequest request) {
    System.out.println("Получен запрос: " + request.uri() + " метод: " + request.method());

    // Создаём ответ
    FullHttpResponse response;
    String responseBody;

    // Простая маршрутизация по URI и методу
    if ("/test".equals(request.uri()) && request.method() == HttpMethod.GET) {
      responseBody = "{\"message\": \"Привет из Netty!\"}";
      response = new DefaultFullHttpResponse(
          HttpVersion.HTTP_1_1,
          HttpResponseStatus.OK,
          Unpooled.copiedBuffer(responseBody, CharsetUtil.UTF_8)
      );
    } else {
      responseBody = "{\"error\": \"404 Not Found\"}";
      response = new DefaultFullHttpResponse(
          HttpVersion.HTTP_1_1,
          HttpResponseStatus.NOT_FOUND,
          Unpooled.copiedBuffer(responseBody, CharsetUtil.UTF_8)
      );
    }

    response.headers().set(HttpHeaderNames.CONTENT_TYPE, "application/json; charset=UTF-8");
    response.headers().set(HttpHeaderNames.CONTENT_LENGTH, response.content().readableBytes());

    ctx.writeAndFlush(response).addListener(ChannelFutureListener.CLOSE);
  }

  @Override
  public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) {
    cause.printStackTrace();
    ctx.close();
  }
}