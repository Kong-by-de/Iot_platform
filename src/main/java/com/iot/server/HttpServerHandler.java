package com.iot.server;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.iot.service.TelemetryService;
import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.handler.codec.http.*;
import io.netty.util.CharsetUtil;

import java.io.IOException;

/**
 * Обработчик HTTP-запросов для приема телеметрии от устройств.
 * <p>
 * Делегирует обработку данных сервису {@link com.iot.service.TelemetryService}.
 */
public class HttpServerHandler extends SimpleChannelInboundHandler<FullHttpRequest> {

  private final TelemetryService telemetryService;

  /**
   * Конструктор обработчика.
   *
   * @param telemetryService Сервис для обработки телеметрии.
   */
  public HttpServerHandler(TelemetryService telemetryService) {
    this.telemetryService = telemetryService;
  }

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

        boolean success = telemetryService.processTelemetry(data);

        if (success) {
          response = createJsonResponse(HttpResponseStatus.OK, "{\"status\":\"saved_and_forwarded\"}");
        } else {
          response = createJsonResponse(HttpResponseStatus.INTERNAL_SERVER_ERROR, "{\"error\":\"Processing failed\"}");
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

  @Override
  public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) {
    cause.printStackTrace();
    ctx.close();
  }
}