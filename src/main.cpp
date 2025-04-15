#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include "DHT20.h"
#include "Wire.h"
#include <HTTPClient.h>
#include <Update.h>
#include <math.h>


#define LED_PIN 2                        
#define SDA_PIN 21
#define SCL_PIN 22         

// WiFi credentials
constexpr char WIFI_SSID[] = "DC";
constexpr char WIFI_PASSWORD[] = "2444666668888888";

// ThingsBoard credentials
constexpr char TOKEN[] = "2nm80obclx3l43zw0vc5";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

const char* firmwareURL = "https://sr4rch1t3ct.github.io/IOT_Lab3/firmware.bin";
#define OTA_CHECK_INTERVAL 60000  // 1 minute

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient);
DHT20 dht20;

void otaUpdateTask(void *pvParameters) {
  while (true) {
    Serial.println("Checking for OTA update...");
    WiFiClient client;
    HTTPClient http;

    http.begin(client, firmwareURL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      int contentLength = http.getSize();
      if (contentLength <= 0) {
        Serial.println("Invalid content length");
        http.end();
        vTaskDelay(OTA_CHECK_INTERVAL / portTICK_PERIOD_MS);
        continue;
      }

      bool canBegin = Update.begin(contentLength);
      if (canBegin) {
        Serial.println("Starting OTA update...");
        size_t written = Update.writeStream(http.getStream());

        if (written == contentLength) {
          Serial.println("OTA written successfully. Rebooting...");
          if (Update.end() && Update.isFinished()) {
            ESP.restart();
          } else {
            Serial.printf("OTA failed: %s\n", Update.errorString());
          }
        } else {
          Serial.println("Written size mismatch.");
        }
      } else {
        Serial.println("Not enough space for OTA update.");
      }
    } else {
      Serial.printf("HTTP Error: %d\n", httpCode);
    }

    http.end();
    vTaskDelay(OTA_CHECK_INTERVAL / portTICK_PERIOD_MS);
  }
}

void checkWiFiTask(void *pvParameters) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected. Reconnecting...");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
      }
      Serial.println("Reconnected to WiFi");
    }
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

void checkCoreIoTTask(void *pvParameters) {
  while (true) {
    if (!tb.connected()) {
      Serial.println("Connecting to ThingsBoard...");
      if (tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
        Serial.println("Connected to ThingsBoard");
      } else {
        Serial.println("Failed to connect to ThingsBoard");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        continue;
      }
    }
    tb.loop();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void telemetryTask(void *pvParameters) {
  while (true) {
    dht20.read();
    float temperature = dht20.getTemperature();
    float humidity = dht20.getHumidity();

    if (!isnan(temperature) && !isnan(humidity)) {
      Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
      tb.sendTelemetryData("temperature", temperature);
      tb.sendTelemetryData("humidity", humidity);
    } else {
      Serial.println("Failed to read from DHT20 sensor!");
    }
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Wire.begin(SDA_PIN, SCL_PIN);
  dht20.begin();

  xTaskCreate(checkWiFiTask, "Check WiFi", 4096, NULL, 1, NULL);
  xTaskCreate(checkCoreIoTTask, "Check CoreIoT", 4096, NULL, 1, NULL);
  xTaskCreate(telemetryTask, "Telemetry", 4096, NULL, 1, NULL);
  xTaskCreate(otaUpdateTask, "OTA Update", 8192, NULL, 1, NULL);
}

void loop() {
  delay(1000);
}
