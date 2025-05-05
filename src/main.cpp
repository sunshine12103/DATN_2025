#include "Arduino.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <60ghzbreathheart.h>

// Khởi tạo radar và client
BreathHeart_60GHz radar = BreathHeart_60GHz(&Serial2);
WiFiClient espClient;
PubSubClient client(espClient);

// Cấu hình Wi-Fi và MQTT
const char* ssid = "Fuvitech";
const char* password = "fuvitech.vn";
const char* mqtt_server = "mqtt.fuvitech.vn";
const int mqtt_port = 2883;
const char* topic = "Duong/ReadSensor";
const char* mqtt_client_id = "ESP32RADARClient";

const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbyKut8F8qcauKXJFb42kRnaOAQ59GT7AR25AcL5rXJTa2L4lsgL604Kyr0NWfswXRND5w/exec"; // Thay bằng Web App URL chính xác

int latestHeartRate = 0;
int latestRespirationRate = 0;
float latestHeight = 0;
float latestWeight = 0;
String latestGender = "";
unsigned long lastRadarUpdate = 0;
unsigned long lastDataPublish = 0;
const unsigned long radarInterval = 200; 
const unsigned long publishInterval = 60000;

void generateRandomData() {
  latestHeight = random(1500, 2001) / 10.0; 
  latestWeight = random(400, 1201) / 10.0;
  latestGender = (random(0, 2) == 0) ? "Male" : "Female";
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500); 
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi.");
  }
}
void connectToMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  unsigned long startTime = millis();
  while (!client.connected() && millis() - startTime < 10000) {
    Serial.println("Connecting to MQTT...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("Connected to MQTT.");
    } else {
      Serial.print("Failed to connect. Error code: ");
      Serial.print(client.state());
      Serial.println(". Retrying...");
      delay(2000); 
    }
  }
}

void publishToMQTT(int heartRate, int respirationRate, float height, float weight, String gender) {
  unsigned long startTime = millis();
  StaticJsonDocument<256> jsonDoc;
  jsonDoc["HeartRate"] = heartRate;
  jsonDoc["RespirationRate"] = respirationRate;
  jsonDoc["Height"] = height;
  jsonDoc["Weight"] = weight;
  jsonDoc["Gender"] = gender;

  char buffer[256];
  serializeJson(jsonDoc, buffer);

  if (client.publish(topic, buffer)) {
    Serial.print("Data published to MQTT: ");
    Serial.println(buffer);
  } else {
    Serial.println("Failed to publish to MQTT.");
  }
  Serial.print("MQTT publish took: ");
  Serial.print(millis() - startTime);
  Serial.println(" ms");
}

void publishToGoogleSheets(int heartRate, int respirationRate, float height, float weight, String gender) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    connectToWiFi();
  }

  unsigned long startTime = millis();
  HTTPClient http;
  http.setTimeout(5000);
  http.begin(googleScriptURL);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["HeartRate"] = heartRate;
  jsonDoc["RespirationRate"] = respirationRate;
  jsonDoc["Height"] = height;
  jsonDoc["Weight"] = weight;
  jsonDoc["Gender"] = gender;

  char buffer[256];
  serializeJson(jsonDoc, buffer);

  int httpCode = http.POST(buffer);
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.print("Data published to Google Sheets: ");
    Serial.println(buffer);
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.print("Failed to publish to Google Sheets. HTTP code: ");
    Serial.println(httpCode);
  }
  http.end();
  Serial.print("Google Sheets publish took: ");
  Serial.print(millis() - startTime);
  Serial.println(" ms");
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17); 
  while (!Serial);

  Serial.println("Radar R60ABD1 Initialized");


  connectToWiFi();
  connectToMQTT();
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  unsigned long currentTime = millis();
  if (currentTime - lastRadarUpdate >= radarInterval) {
    radar.Breath_Heart(); 

  
    if (radar.sensor_report != 0x00) {
      if (radar.sensor_report == HEARTRATEVAL && radar.heart_rate > 0) {
        latestHeartRate = radar.heart_rate;
        Serial.print("Heart Rate: ");
        Serial.print(latestHeartRate);
        Serial.println(" bpm");
      }
      if (radar.sensor_report == BREATHVAL && radar.breath_rate > 0) {
        latestRespirationRate = radar.breath_rate;
        Serial.print("Respiration Rate: ");
        Serial.print(latestRespirationRate);
        Serial.println(" breaths/min");
      }
    }

    lastRadarUpdate = currentTime;
  }

  if (currentTime - lastDataPublish >= publishInterval) {
    if (latestHeartRate > 0 || latestRespirationRate > 0) { 
      generateRandomData();

      publishToMQTT(latestHeartRate, latestRespirationRate, latestHeight, latestWeight, latestGender);

      publishToGoogleSheets(latestHeartRate, latestRespirationRate, latestHeight, latestWeight, latestGender);

      Serial.print("Height: ");
      Serial.print(latestHeight);
      Serial.println(" cm");
      Serial.print("Weight: ");
      Serial.print(latestWeight);
      Serial.println(" kg");
      Serial.print("Gender: ");
      Serial.println(latestGender);
    } else {
      Serial.println("No valid radar data to publish.");
    }
    lastDataPublish = currentTime;
  }
}