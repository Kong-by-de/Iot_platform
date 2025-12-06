#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../models/IoTData.h"

// Forward declarations
namespace iot_core {
namespace core {
class DatabaseRepository;
}
namespace services {
class AlertProcessingService;
}
}  // namespace iot_core

namespace iot_core::engine {

struct Rule {
  std::string name;
  std::string description;
  std::function<bool(const models::IoTData&)> condition;
  std::function<void(const models::IoTData&)> action;
  int priority = 0;
  bool enabled = true;

  Rule(std::string n, std::string desc,
       std::function<bool(const models::IoTData&)> cond,
       std::function<void(const models::IoTData&)> act, int prio = 0,
       bool en = true)
      : name(std::move(n)),
        description(std::move(desc)),
        condition(std::move(cond)),
        action(std::move(act)),
        priority(prio),
        enabled(en) {}
};

class RuleEngine {
 public:
  RuleEngine(std::shared_ptr<core::DatabaseRepository> database,
             std::shared_ptr<services::AlertProcessingService> alertService);

  // Rule management
  void addRule(const Rule& rule);
  void removeRule(const std::string& ruleName);
  void enableRule(const std::string& ruleName);
  void disableRule(const std::string& ruleName);

  // Data processing
  void processData(const models::IoTData& data);
  void processDeviceData(const std::string& deviceId, double temperature,
                         double humidity);

  // Default rules setup
  void setupDefaultRules();

  // Statistics
  struct Statistics {
    int totalProcessed = 0;
    int rulesTriggered = 0;
    std::unordered_map<std::string, int> ruleTriggerCount;
  };

  Statistics getStatistics() const;
  void resetStatistics();

  // Information
  std::vector<std::string> getRuleNames() const;
  const Rule* getRule(const std::string& name) const;
  bool ruleExists(const std::string& name) const;

 private:
  std::vector<Rule> rules_;
  std::shared_ptr<core::DatabaseRepository> database_;
  std::shared_ptr<services::AlertProcessingService> alertService_;

  mutable Statistics statistics_;
  mutable std::mutex statisticsMutex_;
  mutable std::mutex rulesMutex_;

  void sortRulesByPriority();
  void executeRule(const Rule& rule, const models::IoTData& data);

  // Default rule factories
  Rule createTemperatureHighRule(double threshold = 30.0);
  Rule createTemperatureLowRule(double threshold = 15.0);
  Rule createHumidityHighRule(double threshold = 70.0);
  Rule createHumidityLowRule(double threshold = 30.0);
  Rule createDataValidationRule();
  Rule createDeviceOfflineRule();
  Rule createBatteryLowRule(double threshold = 20.0);
};

}  // namespace iot_core::engine