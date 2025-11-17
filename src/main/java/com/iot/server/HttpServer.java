package com.iot.server;

import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.handler.codec.http.HttpObjectAggregator;
import io.netty.handler.codec.http.HttpServerCodec;

/**
 * Главный класс приложения. Запускает HTTP-сервер на указанном порту.
 */
public class HttpServer {
  private final int port;

  /**
   * Конструктор.
   * @param port Порт, на котором будет работать сервер.
   */
  public HttpServer(int port) {
    this.port = port;
  }

  /**
   * Запускает сервер.
   * @throws Exception если произошла ошибка при запуске сервера.
   */
  public void start() throws Exception {
    EventLoopGroup bossGroup = new NioEventLoopGroup();
    EventLoopGroup workerGroup = new NioEventLoopGroup();
    try {
      ServerBootstrap b = new ServerBootstrap();
      b.group(bossGroup, workerGroup)
          .channel(NioServerSocketChannel.class)
          .childHandler(new ChannelInitializer<SocketChannel>() {
            @Override
            public void initChannel(SocketChannel ch) {
              ch.pipeline()
                  .addLast(new HttpServerCodec())
                  .addLast(new HttpObjectAggregator(65536))
                  .addLast(new HttpServerHandler());
            }
          })
          .option(ChannelOption.SO_BACKLOG, 128)
          .childOption(ChannelOption.SO_KEEPALIVE, true);

      ChannelFuture f = b.bind(port).sync();
      System.out.println("🚀 Сервер запущен на http://localhost:" + port);
      f.channel().closeFuture().sync();
    } finally {
      workerGroup.shutdownGracefully();
      bossGroup.shutdownGracefully();
    }
  }

  /**
   * Точка входа в программу.
   * @param args Аргументы командной строки (не используются).
   * @throws Exception если произошла ошибка при запуске сервера.
   */
  public static void main(String[] args) throws Exception {
    new HttpServer(8081).start();
  }
}