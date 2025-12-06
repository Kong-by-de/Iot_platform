package com.iot.server;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.iot.service.TelemetryService;
import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelPromise;
import io.netty.handler.codec.http.*;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.nio.charset.StandardCharsets;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.*;

/**
 * Модульные тесты для HttpServerHandler.
 * <p>
 * Проверяют HTTP-уровень: парсинг запроса, валидацию, формирование ответа.
 * Бизнес-логика (TelemetryService) замокана, но проверяется,
 * что в неё передаются корректные данные.
 */
class HttpServerHandlerTest {

  @Mock
  private TelemetryService telemetryService;

  @Mock
  private ChannelHandlerContext ctx;

  @Mock
  private ChannelPromise channelPromise;

  private HttpServerHandler handler;

  @BeforeEach
  void setUp() {
    MockitoAnnotations.openMocks(this);
    handler = new HttpServerHandler(telemetryService);

    // Настраиваем моки так, чтобы ctx.writeAndFlush не падал
    when(ctx.writeAndFlush(any())).thenReturn(channelPromise);
    when(channelPromise.addListener(any())).thenReturn(channelPromise);
  }

  @Test
  @DisplayName("Валидный POST /telemetry → вызывает service с корректными данными")
  void shouldParseValidJsonAndPassToService() throws Exception {
    // Given
    String jsonBody = "{\"device_id\":\"sensor_01\",\"temperature\":25.5,\"humidity\":60.0}";
    FullHttpRequest request = new DefaultFullHttpRequest(
        HttpVersion.HTTP_1_1,
        HttpMethod.POST,
        "/telemetry",
        Unpooled.wrappedBuffer(jsonBody.getBytes(StandardCharsets.UTF_8))
    );
    request.headers().set(HttpHeaderNames.CONTENT_LENGTH, jsonBody.length());
    request.headers().set(HttpHeaderNames.CONTENT_TYPE, "application/json");

    // When
    handler.channelRead0(ctx, request);

    // Then: проверяем, что service вызван с правильным объектом
    ArgumentCaptor<TelemetryRequest> captor = ArgumentCaptor.forClass(TelemetryRequest.class);
    verify(telemetryService).processTelemetry(captor.capture());

    TelemetryRequest data = captor.getValue();
    assertThat(data.getDevice_id()).isEqualTo("sensor_01");
    assertThat(data.getTemperature()).isEqualTo(25.5);
    assertThat(data.getHumidity()).isEqualTo(60.0);
  }

  @Test
  @DisplayName("Валидный запрос + service вернул true → 200 OK")
  void shouldReturn200WhenServiceReturnsTrue() throws Exception {
    // Given
    String jsonBody = "{\"device_id\":\"sensor_01\",\"temperature\":25.5,\"humidity\":60.0}";
    FullHttpRequest request = new DefaultFullHttpRequest(
        HttpVersion.HTTP_1_1,
        HttpMethod.POST,
        "/telemetry",
        Unpooled.wrappedBuffer(jsonBody.getBytes(StandardCharsets.UTF_8))
    );
    request.headers().set(HttpHeaderNames.CONTENT_LENGTH, jsonBody.length());
    request.headers().set(HttpHeaderNames.CONTENT_TYPE, "application/json");

    when(telemetryService.processTelemetry(any(TelemetryRequest.class))).thenReturn(true);

    // When
    handler.channelRead0(ctx, request);

    // Then
    ArgumentCaptor<FullHttpResponse> responseCaptor = ArgumentCaptor.forClass(FullHttpResponse.class);
    verify(ctx).writeAndFlush(responseCaptor.capture());

    FullHttpResponse response = responseCaptor.getValue();
    assertThat(response.status()).isEqualTo(HttpResponseStatus.OK);
    String content = response.content().toString(StandardCharsets.UTF_8);
    assertThat(content).contains("\"status\":\"saved_and_forwarded\"");
  }

  @Test
  @DisplayName("Валидный запрос + service вернул false → 500 Internal Server Error")
  void shouldReturn500WhenServiceReturnsFalse() throws Exception {
    // Given
    String jsonBody = "{\"device_id\":\"sensor_01\",\"temperature\":25.5,\"humidity\":60.0}";
    FullHttpRequest request = new DefaultFullHttpRequest(
        HttpVersion.HTTP_1_1,
        HttpMethod.POST,
        "/telemetry",
        Unpooled.wrappedBuffer(jsonBody.getBytes(StandardCharsets.UTF_8))
    );
    request.headers().set(HttpHeaderNames.CONTENT_LENGTH, jsonBody.length());
    request.headers().set(HttpHeaderNames.CONTENT_TYPE, "application/json");

    when(telemetryService.processTelemetry(any(TelemetryRequest.class))).thenReturn(false);

    // When
    handler.channelRead0(ctx, request);

    // Then
    ArgumentCaptor<FullHttpResponse> responseCaptor = ArgumentCaptor.forClass(FullHttpResponse.class);
    verify(ctx).writeAndFlush(responseCaptor.capture());

    FullHttpResponse response = responseCaptor.getValue();
    assertThat(response.status()).isEqualTo(HttpResponseStatus.INTERNAL_SERVER_ERROR);
    String content = response.content().toString(StandardCharsets.UTF_8);
    assertThat(content).contains("\"error\":\"Processing failed\"");
  }

  @Test
  @DisplayName("Невалидный JSON → 400 Bad Request")
  void shouldReturn400WhenJsonIsInvalid() {
    // Given
    String invalidJson = "{invalid";
    FullHttpRequest request = new DefaultFullHttpRequest(
        HttpVersion.HTTP_1_1,
        HttpMethod.POST,
        "/telemetry",
        Unpooled.wrappedBuffer(invalidJson.getBytes(StandardCharsets.UTF_8))
    );

    // When
    handler.channelRead0(ctx, request);

    // Then
    ArgumentCaptor<FullHttpResponse> responseCaptor = ArgumentCaptor.forClass(FullHttpResponse.class);
    verify(ctx).writeAndFlush(responseCaptor.capture());

    FullHttpResponse response = responseCaptor.getValue();
    assertThat(response.status()).isEqualTo(HttpResponseStatus.BAD_REQUEST);
    String content = response.content().toString(StandardCharsets.UTF_8);
    assertThat(content).contains("\"error\":\"Invalid telemetry\"");
  }

  @Test
  @DisplayName("Неверный URI → 404 Not Found")
  void shouldReturn404ForUnknownPath() {
    // Given
    FullHttpRequest request = new DefaultFullHttpRequest(
        HttpVersion.HTTP_1_1,
        HttpMethod.POST,
        "/unknown",
        Unpooled.EMPTY_BUFFER
    );

    // When
    handler.channelRead0(ctx, request);

    // Then
    ArgumentCaptor<FullHttpResponse> responseCaptor = ArgumentCaptor.forClass(FullHttpResponse.class);
    verify(ctx).writeAndFlush(responseCaptor.capture());

    FullHttpResponse response = responseCaptor.getValue();
    assertThat(response.status()).isEqualTo(HttpResponseStatus.NOT_FOUND);
    String content = response.content().toString(StandardCharsets.UTF_8);
    assertThat(content).contains("\"error\":\"404\"");
  }
}