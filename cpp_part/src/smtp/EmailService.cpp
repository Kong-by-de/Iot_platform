#include "EmailService.h"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace iot_core::smtp {

EmailService::EmailService() {
  config_ = EmailConfig::loadFromEnv();
  configured_ = config_.isValid();

  if (configured_) {
    std::cout << "✅ EmailService инициализирован" << std::endl;
  } else {
    std::cout << "❌ EmailService не настроен" << std::endl;
  }
}

EmailService::~EmailService() {
  // Освобождение ресурсов CURL
}

bool EmailService::sendEmail(const std::vector<std::string>& recipients,
                             const std::string& subject,
                             const std::string& body, bool isHtml) {
  if (!configured_) {
    std::cout << "❌ EmailService не настроен, пропускаем отправку"
              << std::endl;
    return false;
  }

  if (recipients.empty()) {
    std::cout << "⚠️  Нет получателей для отправки email" << std::endl;
    return false;
  }

  CURL* curl = curl_easy_init();
  if (!curl) {
    std::cerr << "❌ Не удалось инициализировать CURL" << std::endl;
    return false;
  }

  UploadData upload;

  // Формируем email с MIME заголовками
  std::string emailData =
      "From: " + config_.fromEmail + "\r\n" + "To: " + recipients.front() +
      "\r\n" + "Subject: " + subject + "\r\n" + "MIME-Version: 1.0\r\n" +
      (isHtml ? "Content-Type: text/html; charset=UTF-8\r\n"
              : "Content-Type: text/plain; charset=utf-8\r\n") +
      "\r\n" + body + "\r\n";

  upload.payload = emailData;

  // Настраиваем CURL
  std::string url =
      "smtp://" + config_.server + ":" + std::to_string(config_.port);

  std::cout << "📧 Отправка email через " << config_.server << ":"
            << config_.port << std::endl;
  std::cout << "   • От: " << config_.fromEmail << std::endl;
  std::cout << "   • Тема: " << subject << std::endl;
  std::cout << "   • Формат: " << (isHtml ? "HTML" : "Plain text") << std::endl;
  std::cout << "   • Получателей: " << recipients.size() << std::endl;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_MAIL_FROM, config_.fromEmail.c_str());
  curl_easy_setopt(curl, CURLOPT_USERNAME, config_.username.c_str());
  curl_easy_setopt(curl, CURLOPT_PASSWORD, config_.password.c_str());
  curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,
                   0L);  // Отключаем проверку SSL для тестов

  // Таймауты
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

  // Отладка
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  // Получатели
  curl_slist* recipientsList = nullptr;
  for (const auto& recipient : recipients) {
    recipientsList = curl_slist_append(recipientsList, recipient.c_str());
    std::cout << "   • Кому: " << recipient << std::endl;
  }
  curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipientsList);

  // Источник данных
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, payloadSource);
  curl_easy_setopt(curl, CURLOPT_READDATA, &upload);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

  // Отправляем
  CURLcode res = curl_easy_perform(curl);

  bool success = false;
  if (res != CURLE_OK) {
    std::cerr << "❌ Ошибка отправки email: " << curl_easy_strerror(res)
              << std::endl;
  } else {
    long smtp_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &smtp_code);
    std::cout << "✅ Email успешно отправлен (SMTP код: " << smtp_code << ")"
              << std::endl;
    success = true;
  }

  // Очистка
  curl_slist_free_all(recipientsList);
  curl_easy_cleanup(curl);

  return success;
}

bool EmailService::sendAlertEmail(const std::string& deviceId, double value,
                                  const std::string& metricType,
                                  const std::string& direction) {
  if (!configured_) {
    std::cout << "⚠️  EmailService не настроен, пропускаем оповещение"
              << std::endl;
    return false;
  }

  if (!config_.hasRecipients()) {
    std::cout << "⚠️  Нет получателей для оповещений" << std::endl;
    return false;
  }

  std::string metricName = humanMetricName(metricType);
  std::string subject = "IoT Alert: " + metricName + " " +
                        humanDirection(direction) + " on " + deviceId;

  std::string body = formatAlertBody(deviceId, value, metricType, direction);

  return sendEmail(config_.alertRecipients, subject, body,
                   true);  // HTML формат
}

bool EmailService::testConnection() {
  if (!configured_) {
    return false;
  }

  std::cout << "🔍 Тестирование подключения к SMTP..." << std::endl;

  // Пробуем отправить тестовый email самому себе
  std::vector<std::string> testRecipients = {config_.fromEmail};
  std::string subject = "SMTP Connection Test - IoT Platform";
  std::string body = R"(<h1>SMTP Connection Test</h1>
<p>If you receive this email, SMTP configuration is working correctly.</p>
<p>Time: )" + []() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
  }() + R"(</p>)";

  bool success = sendEmail(testRecipients, subject, body, true);

  if (success) {
    std::cout << "✅ SMTP подключение работает" << std::endl;
  } else {
    std::cout << "❌ SMTP подключение не работает" << std::endl;
  }

  return success;
}

size_t EmailService::payloadSource(void* ptr, size_t size, size_t nmemb,
                                   void* userp) {
  UploadData* upload = static_cast<UploadData*>(userp);
  size_t maxSize = size * nmemb;

  if (upload->sent >= upload->payload.size()) {
    return 0;
  }

  size_t remaining = upload->payload.size() - upload->sent;
  size_t toCopy = std::min(maxSize, remaining);

  memcpy(ptr, upload->payload.data() + upload->sent, toCopy);
  upload->sent += toCopy;

  return toCopy;
}

std::string EmailService::formatAlertBody(const std::string& deviceId,
                                          double value,
                                          const std::string& metricType,
                                          const std::string& direction) const {
  std::string emoji = getEmoji(metricType, direction);
  std::string metricName = humanMetricName(metricType);
  std::string statusText = humanDirection(direction);
  std::string niceValue = formatDoubleNice(value);
  std::string unit = (metricType == "temperature") ? "°C" : "%";

  std::ostringstream html;
  html << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>IoT Alert</title>
    <style>
        body { font-family: Arial, sans-serif; color: #222; line-height: 1.6; margin: 0; padding: 20px; background-color: #f5f5f5; }
        .container { max-width: 600px; margin: 0 auto; background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); overflow: hidden; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 20px; text-align: center; }
        .content { padding: 30px; }
        .alert-icon { font-size: 48px; margin-bottom: 20px; }
        table { width: 100%; border-collapse: collapse; margin: 20px 0; }
        th { background-color: #f7f7f7; text-align: left; padding: 12px 15px; border: 1px solid #ddd; }
        td { padding: 12px 15px; border: 1px solid #ddd; }
        .footer { background-color: #f9f9f9; padding: 20px; text-align: center; font-size: 12px; color: #888; border-top: 1px solid #eee; }
        .value-highlight { font-size: 24px; font-weight: bold; color: #e74c3c; }
        .device-name { color: #3498db; font-weight: bold; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div class="alert-icon">)"
       << emoji << R"(</div>
            <h1>IoT Platform Alert</h1>
        </div>
        <div class="content">
            <p>An abnormal reading has been detected from your IoT device. Details are shown below:</p>
            
            <table>
                <tr>
                    <th>Device Name</th>
                    <td class="device-name">)"
       << deviceId << R"(</td>
                </tr>
                <tr>
                    <th>Metric</th>
                    <td>)"
       << metricName << R"(</td>
                </tr>
                <tr>
                    <th>Status</th>
                    <td>)"
       << statusText << R"(</td>
                </tr>
                <tr>
                    <th>Current Value</th>
                    <td><span class="value-highlight">)"
       << niceValue << " " << unit << R"(</span></td>
                </tr>
                <tr>
                    <th>Alert Time</th>
                    <td>)"
       <<
      []() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
      }()
       << R"(</td>
                </tr>
            </table>
            
            <p><strong>Recommended Action:</strong></p>
            <ul>
                <li>Check the physical device for any issues</li>
                <li>Verify sensor calibration if applicable</li>
                <li>Review historical data for patterns</li>
                <li>Adjust alert thresholds if needed</li>
            </ul>
        </div>
        <div class="footer">
            <p>This is an automated alert from IoT Platform.</p>
            <p>Please do not reply to this email. To manage alerts, visit your IoT dashboard.</p>
        </div>
    </div>
</body>
</html>)";

  return html.str();
}

// Вспомогательные методы форматирования
std::string EmailService::formatDoubleNice(double value) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << value;
  std::string s = oss.str();

  while (!s.empty() && s.back() == '0') {
    s.pop_back();
  }
  if (!s.empty() && s.back() == '.') {
    s.pop_back();
  }
  return s;
}

std::string EmailService::humanMetricName(const std::string& type) {
  if (type == "temperature") return "Temperature";
  if (type == "humidity") return "Humidity";
  return type;
}

std::string EmailService::humanDirection(const std::string& direction) {
  if (direction == "above") return "too high";
  if (direction == "below") return "too low";
  return direction;
}

std::string EmailService::getEmoji(const std::string& type,
                                   const std::string& direction) {
  if (type == "temperature") {
    if (direction == "above") return "🔥";
    if (direction == "below") return "❄️";
  }
  if (type == "humidity") {
    if (direction == "above") return "💧";
    if (direction == "below") return "🏜️";
  }
  return "⚠️";
}

}  // namespace iot_core::smtp