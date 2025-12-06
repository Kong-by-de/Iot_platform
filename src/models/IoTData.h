#pragma once
#include <string>

namespace iot_core::models {

struct IoTData {
    int id;
    std::string deviceId;
    double temperature;
    double humidity;
    std::string timestamp;
    
    IoTData() : id(0), temperature(0.0), humidity(0.0) {}
    
    bool isValid() const {
        return !deviceId.empty() && 
               temperature >= -50 && temperature <= 100 &&
               humidity >= 0 && humidity <= 100;
    }
};

struct UserAlert {
    double temperatureHighThreshold = 0.0;
    double temperatureLowThreshold = 0.0;
    double humidityHighThreshold = 0.0;
    double humidityLowThreshold = 0.0;
    
    bool hasAnyAlert() const {
        return temperatureHighThreshold > 0.0 ||
               temperatureLowThreshold > 0.0 ||
               humidityHighThreshold > 0.0 ||
               humidityLowThreshold > 0.0;
    }
};

struct Device {
    std::string id;
    std::string name;
    long ownerId;
    
    Device(std::string deviceId, long chatId) 
        : id(std::move(deviceId)), ownerId(chatId) {}
};

} // namespace iot_core::models
