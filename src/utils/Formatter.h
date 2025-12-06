// src/utils/Formatter.h
#pragma once
#include <string>
#include <vector>

#include "../models/IoTData.h"

namespace iot_core::utils {

class Formatter {
 public:
  static std::string formatTemperature(double value);
  static std::string formatHumidity(double value);
  static std::string formatTelemetryMessage(const models::IoTData& data);
  static std::string formatAlertMessage(const std::string& deviceId,
                                        double value,
                                        const std::string& metricType,
                                        const std::string& direction);

  // Специальные методы для конкретных типов оповещений
  static std::string formatTemperatureAlert(const std::string& deviceId,
                                            double temperature,
                                            const std::string& direction);
  static std::string formatHumidityAlert(const std::string& deviceId,
                                         double humidity,
                                         const std::string& direction);

  static std::string createWelcomeMessage();
  static std::string createHelpMessage();
  static std::string formatDeviceList(const std::vector<std::string>& devices);
  static std::string formatAlertSettings(const models::UserAlert& alert);
};

}  // namespace iot_core::utils