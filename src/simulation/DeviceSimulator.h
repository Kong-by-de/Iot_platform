#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace iot_core::simulation {

struct DeviceConfiguration {
  std::string deviceId;
  std::string deviceName;

  // Диапазоны значений
  double minTemperature = 15.0;
  double maxTemperature = 30.0;
  double minHumidity = 30.0;
  double maxHumidity = 70.0;

  // Волатильность (насколько значения могут меняться за один шаг)
  double temperatureVolatility = 2.0;
  double humidityVolatility = 5.0;

  // Интервал обновления (миллисекунды)
  int updateIntervalMs = 10000;  // 10 секунд

  // Вероятность сбоя (0.0 - 1.0)
  double failureProbability = 0.01;

  // Сезонные колебания
  bool enableSeasonalEffects = true;
  double seasonalTemperatureAdjustment = 0.0;

  DeviceConfiguration(std::string id, std::string name = "")
      : deviceId(std::move(id)), deviceName(name.empty() ? deviceId : name) {}
};

struct TelemetryData {
  std::string deviceId;
  double temperature;
  double humidity;
  double batteryLevel;  // 0.0 - 100.0
  int signalStrength;   // 0-5
  bool isOnline;
  std::string timestamp;

  TelemetryData(std::string id)
      : deviceId(std::move(id)),
        temperature(0.0),
        humidity(0.0),
        batteryLevel(100.0),
        signalStrength(5),
        isOnline(true) {}
};

using TelemetryCallback = std::function<void(const TelemetryData& data)>;

class SimulatedDevice {
 public:
  SimulatedDevice(const DeviceConfiguration& config);
  ~SimulatedDevice();

  void start(TelemetryCallback callback);
  void stop();
  bool isRunning() const;

  void updateConfiguration(const DeviceConfiguration& config);
  DeviceConfiguration getConfiguration() const;

  TelemetryData getCurrentState() const;
  std::vector<TelemetryData> getHistory(int limit = 10) const;

  // Имитация событий
  void simulateFailure();
  void simulateRecovery();
  void simulateSpike(double temperatureDelta, double humidityDelta);

 private:
  void simulationLoop(TelemetryCallback callback);
  TelemetryData generateTelemetry();
  void addToHistory(const TelemetryData& data);

  DeviceConfiguration config_;
  mutable std::mutex configMutex_;

  std::atomic<bool> running_{false};
  std::thread simulationThread_;

  TelemetryData currentState_;
  mutable std::mutex stateMutex_;

  std::vector<TelemetryData> history_;
  mutable std::mutex historyMutex_;
  const size_t maxHistorySize_ = 100;

  // Генераторы случайных чисел
  std::random_device randomDevice_;
  std::mt19937 randomGenerator_;
  std::uniform_real_distribution<> tempDistribution_;
  std::uniform_real_distribution<> humDistribution_;
  std::uniform_real_distribution<> failureDistribution_;

  // Для имитации трендов
  double temperatureTrend_ = 0.0;
  double humidityTrend_ = 0.0;
  int trendCounter_ = 0;
  const int trendChangeInterval_ = 10;  // изменяем тренд каждые 10 циклов
};

class DeviceSimulator {
 public:
  DeviceSimulator();
  ~DeviceSimulator();

  // Управление устройствами
  std::string addDevice(const DeviceConfiguration& config);
  bool removeDevice(const std::string& deviceId);
  void updateDevice(const std::string& deviceId,
                    const DeviceConfiguration& config);

  // Запуск/остановка
  void startAll(TelemetryCallback callback);
  void stopAll();
  void startDevice(const std::string& deviceId, TelemetryCallback callback);
  void stopDevice(const std::string& deviceId);

  // Получение информации
  std::vector<std::string> getDeviceIds() const;
  DeviceConfiguration getDeviceConfig(const std::string& deviceId) const;
  TelemetryData getDeviceState(const std::string& deviceId) const;

  // Управляемые события
  void simulateDeviceFailure(const std::string& deviceId);
  void simulateDeviceRecovery(const std::string& deviceId);
  void simulateTemperatureSpike(const std::string& deviceId, double delta);
  void simulateHumiditySpike(const std::string& deviceId, double delta);

  // Статистика
  int getActiveDeviceCount() const;
  int getTotalDeviceCount() const;
  double getAverageTemperature() const;
  double getAverageHumidity() const;

 private:
  std::unordered_map<std::string, std::unique_ptr<SimulatedDevice>> devices_;
  mutable std::mutex devicesMutex_;

  TelemetryCallback globalCallback_;
};

}  // namespace iot_core::simulation
