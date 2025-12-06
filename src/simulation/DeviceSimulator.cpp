#include "DeviceSimulator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace iot_core::simulation {

SimulatedDevice::SimulatedDevice(const DeviceConfiguration& config)
    : config_(config),
      currentState_(config.deviceId),
      randomGenerator_(randomDevice_()),
      tempDistribution_(-config.temperatureVolatility,
                        config.temperatureVolatility),
      humDistribution_(-config.humidityVolatility, config.humidityVolatility),
      failureDistribution_(0.0, 1.0) {
  // Инициализация начальных значений
  std::uniform_real_distribution<> initTemp(config.minTemperature,
                                            config.maxTemperature);
  std::uniform_real_distribution<> initHum(config.minHumidity,
                                           config.maxHumidity);

  currentState_.temperature = initTemp(randomGenerator_);
  currentState_.humidity = initHum(randomGenerator_);

  // Случайный начальный тренд
  std::uniform_real_distribution<> trendDist(-0.5, 0.5);
  temperatureTrend_ = trendDist(randomGenerator_);
  humidityTrend_ = trendDist(randomGenerator_);

  std::cout << "🎮 Создано виртуальное устройство: " << config.deviceId
            << " (T: " << currentState_.temperature
            << "°C, H: " << currentState_.humidity << "%)" << std::endl;
}

SimulatedDevice::~SimulatedDevice() { stop(); }

void SimulatedDevice::start(TelemetryCallback callback) {
  if (running_) {
    return;
  }

  running_ = true;
  simulationThread_ =
      std::thread(&SimulatedDevice::simulationLoop, this, callback);

  std::cout << "▶️  Запущено устройство: " << config_.deviceId << std::endl;
}

void SimulatedDevice::stop() {
  if (running_) {
    running_ = false;
    if (simulationThread_.joinable()) {
      simulationThread_.join();
    }

    std::cout << "⏹️  Остановлено устройство: " << config_.deviceId << std::endl;
  }
}

bool SimulatedDevice::isRunning() const { return running_; }

void SimulatedDevice::updateConfiguration(const DeviceConfiguration& config) {
  std::lock_guard<std::mutex> lock(configMutex_);
  config_ = config;

  // Обновляем распределения
  tempDistribution_ = std::uniform_real_distribution<>(
      -config.temperatureVolatility, config.temperatureVolatility);
  humDistribution_ = std::uniform_real_distribution<>(
      -config.humidityVolatility, config.humidityVolatility);

  std::cout << "⚙️  Конфигурация обновлена для: " << config.deviceId
            << std::endl;
}

DeviceConfiguration SimulatedDevice::getConfiguration() const {
  std::lock_guard<std::mutex> lock(configMutex_);
  return config_;
}

TelemetryData SimulatedDevice::getCurrentState() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  return currentState_;
}

std::vector<TelemetryData> SimulatedDevice::getHistory(int limit) const {
  std::lock_guard<std::mutex> lock(historyMutex_);

  int actualLimit = std::min(limit, static_cast<int>(history_.size()));
  return std::vector<TelemetryData>(history_.rbegin(),
                                    history_.rbegin() + actualLimit);
}

void SimulatedDevice::simulateFailure() {
  std::lock_guard<std::mutex> lock(stateMutex_);
  currentState_.isOnline = false;
  currentState_.signalStrength = 0;
  std::cout << "💥 Имитация сбоя устройства: " << config_.deviceId << std::endl;
}

void SimulatedDevice::simulateRecovery() {
  std::lock_guard<std::mutex> lock(stateMutex_);
  currentState_.isOnline = true;
  currentState_.signalStrength = 5;
  std::cout << "🔧 Имитация восстановления устройства: " << config_.deviceId
            << std::endl;
}

void SimulatedDevice::simulateSpike(double temperatureDelta,
                                    double humidityDelta) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  currentState_.temperature += temperatureDelta;
  currentState_.humidity += humidityDelta;

  // Гарантируем границы
  DeviceConfiguration config = getConfiguration();
  currentState_.temperature = std::clamp(
      currentState_.temperature, config.minTemperature, config.maxTemperature);
  currentState_.humidity = std::clamp(currentState_.humidity,
                                      config.minHumidity, config.maxHumidity);

  std::cout << "📈 Имитация скачка: " << config_.deviceId
            << " ΔT=" << temperatureDelta << " ΔH=" << humidityDelta
            << std::endl;
}

void SimulatedDevice::simulationLoop(TelemetryCallback callback) {
  while (running_) {
    try {
      // Генерируем новые данные
      TelemetryData data = generateTelemetry();

      // Сохраняем в историю
      addToHistory(data);

      // Обновляем текущее состояние
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        currentState_ = data;
      }

      // Вызываем callback если предоставлен
      if (callback) {
        callback(data);
      }

      // Ждем перед следующей итерацией
      int interval;
      {
        std::lock_guard<std::mutex> lock(configMutex_);
        interval = config_.updateIntervalMs;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(interval));

    } catch (const std::exception& e) {
      std::cerr << "❌ Ошибка в симуляции устройства " << config_.deviceId
                << ": " << e.what() << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

TelemetryData SimulatedDevice::generateTelemetry() {
  DeviceConfiguration config = getConfiguration();
  TelemetryData data(config.deviceId);

  // Имитация случайных колебаний
  double tempChange = tempDistribution_(randomGenerator_);
  double humChange = humDistribution_(randomGenerator_);

  // Применяем тренд
  tempChange += temperatureTrend_;
  humChange += humidityTrend_;

  // Обновляем тренд периодически
  trendCounter_++;
  if (trendCounter_ >= trendChangeInterval_) {
    std::uniform_real_distribution<> trendChange(-0.2, 0.2);
    temperatureTrend_ += trendChange(randomGenerator_);
    humidityTrend_ += trendChange(randomGenerator_);

    // Ограничиваем тренд
    temperatureTrend_ = std::clamp(temperatureTrend_, -1.0, 1.0);
    humidityTrend_ = std::clamp(humidityTrend_, -2.0, 2.0);

    trendCounter_ = 0;
  }

  // Обновляем значения с учетом границ
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    data.temperature = currentState_.temperature + tempChange;
    data.humidity = currentState_.humidity + humChange;
    data.batteryLevel = currentState_.batteryLevel;
    data.signalStrength = currentState_.signalStrength;
    data.isOnline = currentState_.isOnline;
  }

  // Гарантируем границы
  data.temperature = std::clamp(data.temperature, config.minTemperature,
                                config.maxTemperature);
  data.humidity =
      std::clamp(data.humidity, config.minHumidity, config.maxHumidity);

  // Имитация сбоя (если включена)
  if (config.failureProbability > 0.0) {
    if (failureDistribution_(randomGenerator_) < config.failureProbability) {
      data.isOnline = false;
      data.signalStrength = 0;
      data.batteryLevel -= 5.0;  // Сбой разряжает батарею
    } else if (!data.isOnline) {
      // Восстановление после сбоя
      data.isOnline = true;
      data.signalStrength = 5;
    }
  }

  // Постепенный разряд батареи
  data.batteryLevel -= 0.01;
  if (data.batteryLevel < 0) {
    data.batteryLevel = 0;
    data.isOnline = false;
  }

  // Случайные колебания уровня сигнала
  std::uniform_int_distribution<> signalChange(-1, 1);
  data.signalStrength += signalChange(randomGenerator_);
  data.signalStrength = std::clamp(data.signalStrength, 0, 5);

  // Генерация временной метки
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
  data.timestamp = ss.str();

  return data;
}

void SimulatedDevice::addToHistory(const TelemetryData& data) {
  std::lock_guard<std::mutex> lock(historyMutex_);
  history_.push_back(data);

  // Ограничиваем размер истории
  if (history_.size() > maxHistorySize_) {
    history_.erase(history_.begin());
  }
}

// ==================== DeviceSimulator ====================

DeviceSimulator::DeviceSimulator() {
  std::cout << "🎮 Инициализация симулятора устройств" << std::endl;
}

DeviceSimulator::~DeviceSimulator() { stopAll(); }

std::string DeviceSimulator::addDevice(const DeviceConfiguration& config) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  if (devices_.find(config.deviceId) != devices_.end()) {
    std::cerr << "⚠️  Устройство с ID " << config.deviceId << " уже существует"
              << std::endl;
    return "";
  }

  auto device = std::make_unique<SimulatedDevice>(config);
  devices_[config.deviceId] = std::move(device);

  std::cout << "✅ Добавлено виртуальное устройство: " << config.deviceId
            << std::endl;
  return config.deviceId;
}

bool DeviceSimulator::removeDevice(const std::string& deviceId) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto it = devices_.find(deviceId);
  if (it == devices_.end()) {
    std::cerr << "❌ Устройство " << deviceId << " не найдено" << std::endl;
    return false;
  }

  it->second->stop();
  devices_.erase(it);

  std::cout << "🗑️  Удалено устройство: " << deviceId << std::endl;
  return true;
}

void DeviceSimulator::updateDevice(const std::string& deviceId,
                                   const DeviceConfiguration& config) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto it = devices_.find(deviceId);
  if (it == devices_.end()) {
    throw std::runtime_error("Устройство " + deviceId + " не найдено");
  }

  it->second->updateConfiguration(config);
}

void DeviceSimulator::startAll(TelemetryCallback callback) {
  std::lock_guard<std::mutex> lock(devicesMutex_);
  globalCallback_ = callback;

  for (auto& [deviceId, device] : devices_) {
    device->start([this, callback](const TelemetryData& data) {
      if (callback) {
        callback(data);
      }
    });
  }

  std::cout << "▶️  Запущены все устройства (" << devices_.size() << " шт.)"
            << std::endl;
}

void DeviceSimulator::stopAll() {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  for (auto& [deviceId, device] : devices_) {
    device->stop();
  }

  std::cout << "⏹️  Остановлены все устройства" << std::endl;
}

void DeviceSimulator::startDevice(const std::string& deviceId,
                                  TelemetryCallback callback) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto it = devices_.find(deviceId);
  if (it == devices_.end()) {
    throw std::runtime_error("Устройство " + deviceId + " не найдено");
  }

  it->second->start(callback);
}

void DeviceSimulator::stopDevice(const std::string& deviceId) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto it = devices_.find(deviceId);
  if (it == devices_.end()) {
    throw std::runtime_error("Устройство " + deviceId + " не найдено");
  }

  it->second->stop();
}

std::vector<std::string> DeviceSimulator::getDeviceIds() const {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  std::vector<std::string> ids;
  for (const auto& [id, device] : devices_) {
    ids.push_back(id);
  }

  return ids;
}

DeviceConfiguration DeviceSimulator::getDeviceConfig(
    const std::string& deviceId) const {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto it = devices_.find(deviceId);
  if (it == devices_.end()) {
    throw std::runtime_error("Устройство " + deviceId + " не найдено");
  }

  return it->second->getConfiguration();
}

TelemetryData DeviceSimulator::getDeviceState(
    const std::string& deviceId) const {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto it = devices_.find(deviceId);
  if (it == devices_.end()) {
    throw std::runtime_error("Устройство " + deviceId + " не найдено");
  }

  return it->second->getCurrentState();
}

void DeviceSimulator::simulateDeviceFailure(const std::string& deviceId) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto it = devices_.find(deviceId);
  if (it == devices_.end()) {
    throw std::runtime_error("Устройство " + deviceId + " не найдено");
  }

  it->second->simulateFailure();
}

void DeviceSimulator::simulateDeviceRecovery(const std::string& deviceId) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto it = devices_.find(deviceId);
  if (it == devices_.end()) {
    throw std::runtime_error("Устройство " + deviceId + " не найдено");
  }

  it->second->simulateRecovery();
}

void DeviceSimulator::simulateTemperatureSpike(const std::string& deviceId,
                                               double delta) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto it = devices_.find(deviceId);
  if (it == devices_.end()) {
    throw std::runtime_error("Устройство " + deviceId + " не найдено");
  }

  it->second->simulateSpike(delta, 0.0);
}

void DeviceSimulator::simulateHumiditySpike(const std::string& deviceId,
                                            double delta) {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  auto it = devices_.find(deviceId);
  if (it == devices_.end()) {
    throw std::runtime_error("Устройство " + deviceId + " не найдено");
  }

  it->second->simulateSpike(0.0, delta);
}

int DeviceSimulator::getActiveDeviceCount() const {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  int count = 0;
  for (const auto& [id, device] : devices_) {
    if (device->isRunning()) {
      count++;
    }
  }

  return count;
}

int DeviceSimulator::getTotalDeviceCount() const {
  std::lock_guard<std::mutex> lock(devicesMutex_);
  return static_cast<int>(devices_.size());
}

double DeviceSimulator::getAverageTemperature() const {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  if (devices_.empty()) {
    return 0.0;
  }

  double sum = 0.0;
  int count = 0;

  for (const auto& [id, device] : devices_) {
    auto state = device->getCurrentState();
    if (state.isOnline) {
      sum += state.temperature;
      count++;
    }
  }

  return count > 0 ? sum / count : 0.0;
}

double DeviceSimulator::getAverageHumidity() const {
  std::lock_guard<std::mutex> lock(devicesMutex_);

  if (devices_.empty()) {
    return 0.0;
  }

  double sum = 0.0;
  int count = 0;

  for (const auto& [id, device] : devices_) {
    auto state = device->getCurrentState();
    if (state.isOnline) {
      sum += state.humidity;
      count++;
    }
  }

  return count > 0 ? sum / count : 0.0;
}

}  // namespace iot_core::simulation
