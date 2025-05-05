#include "Arduino.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <60ghzbreathheart.h>

BreathHeart_60GHz radar = BreathHeart_60GHz(&Serial2);
WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "Fuvitech";
const char* password = "fuvitech.vn";
const char* mqtt_server = "mqtt.fuvitech.vn";
const int mqtt_port = 2883;
const char* topic = "Duong/ReadSensor";
const char* mqtt_client_id = "ESP32RADARClient";

int latestHeartRate = 0;
int latestRespirationRate = 0;
float latestHeight = 0; 
float latestWeight = 0; 
String latestGender = ""; 
unsigned long lastRadarUpdate = 0;
unsigned long lastMqttPublish = 0;
const unsigned long radarInterval = 1000; 
const unsigned long mqttInterval = 30000; 


void generateRandomData() {
  latestHeight = random(1500, 2001) / 10.0; 
  latestWeight = random(400, 1201) / 10.0;
  latestGender = (random(0, 2) == 0) ? "Male" : "Female";
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000); 
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void connectToMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("Connected to MQTT.");
    } else {
      Serial.print("Failed to connect. Error code: ");
      Serial.print(client.state());
      Serial.println(". Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void publishSensorData(int heartRate, int respirationRate, float height, float weight, String gender) {
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
    Serial.println("Failed to publish data.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17); 
  while (!Serial);

  Serial.println("Radar R60ABD1 Initialized");


  randomSeed(analogRead(0));

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


  if (currentTime - lastMqttPublish >= mqttInterval) {
    if (latestHeartRate > 0 || latestRespirationRate > 0) { 
     
      generateRandomData();
      
  
      publishSensorData(latestHeartRate, latestRespirationRate, latestHeight, latestWeight, latestGender);
      
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
    lastMqttPublish = currentTime;
  }
}