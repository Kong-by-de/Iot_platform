#include <csignal>
#include <iostream>
#include <memory>

#include "core/Application.h"

using namespace iot_core::core;
std::unique_ptr<Application> g_application;

static void signalHandler(int signal) {
  std::cout << "\n🛑 Received signal " << signal << ", initiating shutdown..."
            << std::endl;
  if (g_application) {
    g_application->shutdown();
  }
}

int main(int argc, char* argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  try {
    g_application = std::make_unique<Application>();
    std::cout << "🚀 Starting IoT Core Platform..." << std::endl;
    if (!g_application->initialize()) {
      std::cerr << "❌ Failed to initialize application" << std::endl;
      return 1;
    }
    g_application->run();
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "\n💥 Critical error: " << e.what() << std::endl;
    if (g_application) {
      g_application->shutdown();
    }
    return 1;
  }
}