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
 * Модульные тесты для класса {@link HttpServerHandler}.
 * <p>
 * Проверяют обработку HTTP-запросов с использованием мока сервиса {@link TelemetryService}.
 * Внешние зависимости полностью изолированы.
 */
class HttpServerHandlerTest {

  /**
   * Мок сервиса обработки телеметрии.
   */
  @Mock
  private TelemetryService telemetryService;

  /**
   * Мок контекста канала Netty.
   */
  @Mock
  private ChannelHandlerContext ctx;

  /**
   * Мок промиса канала.
   */
  @Mock
  private ChannelPromise channelPromise;

  /**
   * Тестируемый обработчик.
   */
  private HttpServerHandler handler;

  /**
   * ObjectMapper для подготовки JSON-тела.
   */
  private final ObjectMapper mapper = new ObjectMapper();

  /**
   * Инициализация моков перед каждым тестом.
   */
  @BeforeEach
  void setUp() {
    MockitoAnnotations.openMocks(this);
    handler = new HttpServerHandler(telemetryService);

    when(ctx.writeAndFlush(any())).thenReturn(channelPromise);
    when(channelPromise.addListener(any())).thenReturn(channelPromise);
  }

  /**
   * Проверяет успешную обработку телеметрии → 200 OK.
   */
  @Test
  @DisplayName("Успешная обработка телеметрии → 200 OK")
  void shouldReturnOkWhenProcessingSucceeds() {
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

  /**
   * Проверяет, что ошибка в сервисе → 500.
   */
  @Test
  @DisplayName("Ошибка в сервисе → 500 Internal Server Error")
  void shouldReturnInternalServerErrorWhenProcessingFails() {
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

  /**
   * Проверяет обработку некорректного JSON → 400.
   */
  @Test
  @DisplayName("Некорректный JSON → 400 Bad Request")
  void shouldReturnBadRequestWhenInvalidJson() {
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

  /**
   * Проверяет неизвестный путь → 404.
   */
  @Test
  @DisplayName("Неизвестный путь → 404 Not Found")
  void shouldReturnNotFoundForUnknownPath() {
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