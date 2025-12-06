#include "Formatter.h"

#include <iomanip>
#include <sstream>
#include <vector>

namespace iot_core::utils {

std::string Formatter::formatTemperature(double value) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << value << "°C";
  return oss.str();
}

std::string Formatter::formatHumidity(double value) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << value << "%";
  return oss.str();
}

std::string Formatter::formatTelemetryMessage(const models::IoTData& data) {
  std::ostringstream oss;

  // Добавляем эмодзи в зависимости от температуры
  std::string tempEmoji;
  if (data.temperature < 15)
    tempEmoji = "❄️";
  else if (data.temperature > 28)
    tempEmoji = "🔥";
  else
    tempEmoji = "🌡️";

  // Добавляем эмодзи для влажности
  std::string humEmoji;
  if (data.humidity < 30)
    humEmoji = "🏜️";
  else if (data.humidity > 70)
    humEmoji = "💦";
  else
    humEmoji = "💧";

  oss << "📊 *Показания устройства*\n\n"
      << "📟 ID: `" << data.deviceId << "`\n"
      << tempEmoji << " Температура: *" << formatTemperature(data.temperature)
      << "*\n"
      << humEmoji << " Влажность: *" << formatHumidity(data.humidity) << "*\n"
      << "⏰ Время: " << data.timestamp << "\n";

  return oss.str();
}

std::string Formatter::formatAlertMessage(const std::string& deviceId,
                                          double value,
                                          const std::string& metricType,
                                          const std::string& direction) {
  std::ostringstream oss;

  std::string emoji;
  std::string unit;
  std::string metricName;

  if (metricType == "temperature") {
    emoji = (direction == "above") ? "🔥" : "❄️";
    unit = "°C";
    metricName = "Температура";
  } else {
    emoji = (direction == "above") ? "💦" : "🏜️";
    unit = "%";
    metricName = "Влажность";
  }

  std::string formattedValue;
  {
    std::ostringstream val;
    val << std::fixed << std::setprecision(1) << value;
    formattedValue = val.str();
  }

  oss << emoji << " *СРАБОТАЛО ОПОВЕЩЕНИЕ!*\n\n"
      << "📟 Устройство: `" << deviceId << "`\n"
      << "📊 Показание: *" << formattedValue << unit << "*\n"
      << "⚠️  Условие: " << metricName << " "
      << (direction == "above" ? "выше порога" : "ниже порога") << "\n";

  return oss.str();
}

// Дополнительные методы для специфичных типов оповещений
std::string Formatter::formatTemperatureAlert(const std::string& deviceId,
                                              double temperature,
                                              const std::string& direction) {
  return formatAlertMessage(deviceId, temperature, "temperature", direction);
}

std::string Formatter::formatHumidityAlert(const std::string& deviceId,
                                           double humidity,
                                           const std::string& direction) {
  return formatAlertMessage(deviceId, humidity, "humidity", direction);
}

std::string Formatter::createWelcomeMessage() {
  return R"(🚀 *Добро пожаловать в IoT Core System!* 🌡️💧

Я помогу вам отслеживать показания ваших IoT-устройств 
и настраивать умные оповещения.

📋 *Основные команды:*
/start - Показать это сообщение
/help - Помощь по командам
/status - Проверить состояние системы

📊 *Работа с данными:*
/last - Последние показания
/history - История данных
/stats - Статистика

⚙️ *Настройка оповещений:*
/alert_temp_high 25.0 - Уведомлять если >25°C
/alert_temp_low 15.0 - Уведомлять если <15°C
/alert_hum_high 60.0 - Уведомлять если влажность >60%
/alert_hum_low 30.0 - Уведомлять если влажность <30%
/show_alerts - Показать текущие настройки
/clear_alerts - Удалить все оповещения

🔗 *Управление устройствами:*
/add_device sensor_01 - Добавить устройство
/my_devices - Мои устройства
/remove_device sensor_01 - Удалить устройство

🎮 *Тестирование:*
/test_hot - Тест высокой температуры
/test_cold - Тест низкой температуры
/test_humid - Тест высокой влажности
/test_dry - Тест низкой влажности

💡 *Совет:* Начните с добавления устройства командой /add_device
)";
}

std::string Formatter::createHelpMessage() {
  return R"(🆘 *Помощь по IoT Core System*

📞 *Поддержка:* 
Если возникли проблемы, проверьте:
1. Сервер доступен? (/status)
2. Устройство привязано? (/my_devices)
3. Оповещения настроены? (/show_alerts)

📚 *Примеры использования:*
1. Добавить устройство и настроить оповещение:
   /add_device sensor_01
   /alert_temp_high 30.0
   /alert_hum_high 70.0

2. Проверить текущие данные:
   /last
   /stats

3. Протестировать систему:
   /test_hot
   /test_cold

🛠️ *Техническая информация:*
• Система поддерживает до 10 устройств на пользователя
• Данные хранятся 30 дней
• Оповещения приходят в Telegram и на email
• API доступен по адресу: http://localhost:8080
)";
}

std::string Formatter::formatDeviceList(
    const std::vector<std::string>& devices) {
  if (devices.empty()) {
    return "📭 *У вас нет привязанных устройств*\n\n"
           "Используйте /add_device <id> чтобы добавить устройство";
  }

  std::ostringstream oss;
  oss << "📱 *Ваши устройства:*\n\n";

  for (size_t i = 0; i < devices.size(); ++i) {
    oss << (i + 1) << ". `" << devices[i] << "`\n";
  }

  oss << "\nВсего: " << devices.size() << " устройств";
  return oss.str();
}

std::string Formatter::formatAlertSettings(const models::UserAlert& alert) {
  std::ostringstream oss;
  oss << "⚙️ *Текущие настройки оповещений:*\n\n";

  bool hasSettings = false;

  if (alert.temperatureHighThreshold > 0.0) {
    oss << "🔥 Температура > "
        << formatTemperature(alert.temperatureHighThreshold) << "\n";
    hasSettings = true;
  }

  if (alert.temperatureLowThreshold > 0.0) {
    oss << "❄️ Температура < "
        << formatTemperature(alert.temperatureLowThreshold) << "\n";
    hasSettings = true;
  }

  if (alert.humidityHighThreshold > 0.0) {
    oss << "💦 Влажность > " << formatHumidity(alert.humidityHighThreshold)
        << "\n";
    hasSettings = true;
  }

  if (alert.humidityLowThreshold > 0.0) {
    oss << "🏜️ Влажность < " << formatHumidity(alert.humidityLowThreshold)
        << "\n";
    hasSettings = true;
  }

  if (!hasSettings) {
    oss << "ℹ️ *Оповещения не настроены*\n\n"
        << "Используйте команды /alert_temp_high, /alert_temp_low, "
        << "/alert_hum_high, /alert_hum_low для настройки";
  }

  return oss.str();
}

}  // namespace iot_core::utils