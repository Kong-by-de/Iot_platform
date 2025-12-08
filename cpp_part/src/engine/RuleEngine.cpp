#include "RuleEngine.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "../core/Database.h"
#include "../services/AlertService.h"
#include "../utils/Formatter.h"

namespace iot_core::engine {

RuleEngine::RuleEngine(
    std::shared_ptr<core::DatabaseRepository> database,
    std::shared_ptr<services::AlertProcessingService> alertService)
    : database_(std::move(database)), alertService_(std::move(alertService)) {
  if (!database_) {
    throw std::invalid_argument("Database repository cannot be null");
  }

  if (!alertService_) {
    throw std::invalid_argument("Alert service cannot be null");
  }

  std::cout << "⚙️  Rule Engine initialized" << std::endl;
}

void RuleEngine::addRule(const Rule& rule) {
  std::lock_guard<std::mutex> lock(rulesMutex_);

  // Check if rule with this name already exists
  auto it = std::find_if(rules_.begin(), rules_.end(), [&rule](const Rule& r) {
    return r.name == rule.name;
  });

  if (it != rules_.end()) {
    std::cerr << "⚠️  Rule '" << rule.name << "' already exists, replacing"
              << std::endl;
    *it = rule;
  } else {
    rules_.push_back(rule);
  }

  sortRulesByPriority();
  std::cout << "➕ Rule added: " << rule.name << " (priority: " << rule.priority
            << ")" << std::endl;
}

void RuleEngine::removeRule(const std::string& ruleName) {
  std::lock_guard<std::mutex> lock(rulesMutex_);

  auto it =
      std::remove_if(rules_.begin(), rules_.end(),
                     [&ruleName](const Rule& r) { return r.name == ruleName; });

  if (it != rules_.end()) {
    rules_.erase(it, rules_.end());
    std::cout << "➖ Rule removed: " << ruleName << std::endl;
  } else {
    std::cerr << "⚠️  Rule '" << ruleName << "' not found" << std::endl;
  }
}

void RuleEngine::enableRule(const std::string& ruleName) {
  std::lock_guard<std::mutex> lock(rulesMutex_);

  for (auto& rule : rules_) {
    if (rule.name == ruleName) {
      rule.enabled = true;
      std::cout << "✅ Rule enabled: " << ruleName << std::endl;
      return;
    }
  }

  std::cerr << "⚠️  Rule '" << ruleName << "' not found" << std::endl;
}

void RuleEngine::disableRule(const std::string& ruleName) {
  std::lock_guard<std::mutex> lock(rulesMutex_);

  for (auto& rule : rules_) {
    if (rule.name == ruleName) {
      rule.enabled = false;
      std::cout << "⛔ Rule disabled: " << ruleName << std::endl;
      return;
    }
  }

  std::cerr << "⚠️  Rule '" << ruleName << "' not found" << std::endl;
}

void RuleEngine::processData(const models::IoTData& data) {
  if (!data.isValid()) {
    std::cerr << "❌ Invalid data received, skipping processing" << std::endl;
    return;
  }

  // Update statistics
  {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.totalProcessed++;
  }
  std::cout << "📊 Data received: " << data.deviceId << " T=" << std::fixed
            << std::setprecision(1) << data.temperature << "°C"
            << " H=" << data.humidity << "%" << std::endl;

  // Get rules to apply (thread-safe copy)
  std::vector<Rule> rulesToApply;
  {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    rulesToApply = rules_;
  }

  // Apply rules in priority order
  int triggered = 0;
  for (const auto& rule : rulesToApply) {
    if (!rule.enabled) continue;

    if (rule.condition(data)) {
      executeRule(rule, data);
      triggered++;

      {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.rulesTriggered++;
        statistics_.ruleTriggerCount[rule.name]++;
      }
    }
  }

  if (triggered > 0) {
    std::cout << "🔔 " << triggered << " rules triggered for device "
              << data.deviceId << std::endl;
  }
}

void RuleEngine::processDeviceData(const std::string& deviceId,
                                   double temperature, double humidity) {
  models::IoTData data;
  data.deviceId = deviceId;
  data.temperature = temperature;
  data.humidity = humidity;

  // Генерируем временную метку
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
  data.timestamp = ss.str();

  // Логируем для отладки (но не сохраняем в локальную БД)
  std::cout << "📊 Обработка данных устройства: " << deviceId
            << " T=" << temperature << "°C"
            << " H=" << humidity << "%" << std::endl;

  // Проверяем правила
  processData(data);
}

void RuleEngine::setupDefaultRules() {
  std::cout << "📋 Setting up default rules..." << std::endl;

  // Clear existing rules
  {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    rules_.clear();
  }

  // Add default rules (in order of priority)
  addRule(createDataValidationRule());
  addRule(createTemperatureHighRule(28.0));
  addRule(createTemperatureLowRule(15.0));
  addRule(createHumidityHighRule(70.0));
  addRule(createHumidityLowRule(30.0));

  std::cout << "✅ " << rules_.size() << " default rules configured"
            << std::endl;

  // Выводим информацию о правилах
  std::cout << "📋 Rules configured:" << std::endl;
  std::cout << "   1. data_validation - Validate incoming data" << std::endl;
  std::cout << "   2. temperature_high_alert - Temp > 28.0°C" << std::endl;
  std::cout << "   3. temperature_low_alert - Temp < 15.0°C" << std::endl;
  std::cout << "   4. humidity_high_alert - Humidity > 70.0%" << std::endl;
  std::cout << "   5. humidity_low_alert - Humidity < 30.0%" << std::endl;
}

RuleEngine::Statistics RuleEngine::getStatistics() const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  return statistics_;
}

void RuleEngine::resetStatistics() {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  statistics_ = Statistics{};
  std::cout << "📊 Rule Engine statistics reset" << std::endl;
}

std::vector<std::string> RuleEngine::getRuleNames() const {
  std::lock_guard<std::mutex> lock(rulesMutex_);

  std::vector<std::string> names;
  for (const auto& rule : rules_) {
    names.push_back(rule.name);
  }

  return names;
}

const Rule* RuleEngine::getRule(const std::string& name) const {
  std::lock_guard<std::mutex> lock(rulesMutex_);

  for (const auto& rule : rules_) {
    if (rule.name == name) {
      return &rule;
    }
  }

  return nullptr;
}

bool RuleEngine::ruleExists(const std::string& name) const {
  return getRule(name) != nullptr;
}

void RuleEngine::sortRulesByPriority() {
  std::sort(rules_.begin(), rules_.end(), [](const Rule& a, const Rule& b) {
    return a.priority > b.priority;
  });
}

void RuleEngine::executeRule(const Rule& rule, const models::IoTData& data) {
  try {
    std::cout << "⚡ Rule triggered: " << rule.name << " for device "
              << data.deviceId << std::endl;

    if (!rule.description.empty()) {
      std::cout << "   • " << rule.description << std::endl;
    }

    rule.action(data);

  } catch (const std::exception& e) {
    std::cerr << "❌ Error executing rule '" << rule.name << "': " << e.what()
              << std::endl;
  }
}

Rule RuleEngine::createTemperatureHighRule(double threshold) {
  return Rule(
      "temperature_high_alert",
      "Temperature above " + std::to_string(threshold) + "°C",
      [threshold](const models::IoTData& data) {
        bool triggered = data.temperature > threshold;
        if (triggered) {
          std::cout << "   🔥 Temperature " << data.temperature << " > "
                    << threshold << "°C - ALERT!" << std::endl;
        }
        return triggered;
      },
      [this](const models::IoTData& data) {
        std::cout << "   📤 Sending alert for high temperature: "
                  << data.temperature << "°C" << std::endl;
        alertService_->processTelemetryData(data.deviceId, data.temperature,
                                            data.humidity);
      },
      10, true);
}

Rule RuleEngine::createTemperatureLowRule(double threshold) {
  return Rule(
      "temperature_low_alert",
      "Temperature below " + std::to_string(threshold) + "°C",
      [threshold](const models::IoTData& data) {
        bool triggered = data.temperature < threshold;
        if (triggered) {
          std::cout << "   ❄️ Temperature " << data.temperature << " < "
                    << threshold << "°C - ALERT!" << std::endl;
        }
        return triggered;
      },
      [this](const models::IoTData& data) {
        std::cout << "   📤 Sending alert for low temperature: "
                  << data.temperature << "°C" << std::endl;
        alertService_->processTelemetryData(data.deviceId, data.temperature,
                                            data.humidity);
      },
      10, true);
}

Rule RuleEngine::createHumidityHighRule(double threshold) {
  return Rule(
      "humidity_high_alert",
      "Humidity above " + std::to_string(threshold) + "%",
      [threshold](const models::IoTData& data) {
        bool triggered = data.humidity > threshold;
        if (triggered) {
          std::cout << "   💦 Humidity " << data.humidity << " > " << threshold
                    << "% - ALERT!" << std::endl;
        }
        return triggered;
      },
      [this](const models::IoTData& data) {
        std::cout << "   📤 Sending alert for high humidity: " << data.humidity
                  << "%" << std::endl;
        alertService_->processTelemetryData(data.deviceId, data.temperature,
                                            data.humidity);
      },
      5, true);
}

Rule RuleEngine::createHumidityLowRule(double threshold) {
  return Rule(
      "humidity_low_alert", "Humidity below " + std::to_string(threshold) + "%",
      [threshold](const models::IoTData& data) {
        bool triggered = data.humidity < threshold;
        if (triggered) {
          std::cout << "   🏜️ Humidity " << data.humidity << " < " << threshold
                    << "% - ALERT!" << std::endl;
        }
        return triggered;
      },
      [this](const models::IoTData& data) {
        std::cout << "   📤 Sending alert for low humidity: " << data.humidity
                  << "%" << std::endl;
        alertService_->processTelemetryData(data.deviceId, data.temperature,
                                            data.humidity);
      },
      5, true);
}

Rule RuleEngine::createDataValidationRule() {
  return Rule(
      "data_validation", "Validate incoming telemetry data",
      [](const models::IoTData& data) { return true; },
      [](const models::IoTData& data) {
        if (!data.isValid()) {
          std::cerr << "⚠️  Invalid data from device " << data.deviceId
                    << ": T=" << data.temperature << "°C, H=" << data.humidity
                    << "%" << std::endl;
        } else {
          std::cout << "   ✓ Data validation passed for " << data.deviceId
                    << std::endl;
        }
      },
      100, true);
}

}  // namespace iot_core::engine