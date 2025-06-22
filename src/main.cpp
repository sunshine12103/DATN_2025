#include "Arduino.h"
#include <60ghzbreathheart.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "Adafruit_VL53L0X.h"
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Fuvitech";
const char* password = "fuvitech.vn";

const char* mqtt_server = "mqtt.fuvitech.vn";
const int mqtt_port = 2883;
const char* mqtt_topic = "DATN/Weight";

WiFiClient espClient;
PubSubClient client(espClient);

#define RELAY_PIN 33
#define SOUND_PIN 34

BreathHeart_60GHz radar = BreathHeart_60GHz(&Serial2);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
DHT dht(32, DHT22); 

#define MAX_SAMPLES 60
int heartRateSamples[MAX_SAMPLES];
int breathRateSamples[MAX_SAMPLES];
float weightSamples[MAX_SAMPLES];
int sampleCount = 0;
unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 1000; 

int latestHeartRate = 0;
int latestRespirationRate = 0;
int latestDistance = 0; 
float latestHeight = 0; // Chiều cao tính toán (cm)
float latestTemperature = 0;
float latestHumidity = 0; 
float latestWeight = 0; 
float lastWeight = 0; 
float latestBMI = 0;
unsigned long lastRadarUpdate = 0;
const unsigned long radarInterval = 1000;
const unsigned long displayInterval = 500;
unsigned long lastDisplayUpdate = 0;
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R2, /* clock=*/ 18, /* data=*/ 23, /* CS=*/ 5, /* reset=*/ 22);

int dotCount = 0;
unsigned long lastDotUpdate = 0;
const unsigned long dotInterval = 500;

// Hàm tính trung bình
int averageInt(int* arr, int count) {
  if (count == 0) return 0;
  int sum = 0;
  for (int i = 0; i < count; i++) sum += arr[i];
  return sum / count;
}

float averageFloat(float* arr, int count) {
  if (count == 0) return 0;
  float sum = 0;
  for (int i = 0; i < count; i++) sum += arr[i];  return sum / count;
}

// Hàm tính BMI
float calculateBMI(float weight_kg, float height_cm) {
  if (weight_kg <= 0 || height_cm <= 0) return 0;
  float height_m = height_cm / 100.0; // Chuyển đổi cm sang m
  return weight_kg / (height_m * height_m);
}

// Hàm đặt lại mảng mẫu
void resetSamples() {
  sampleCount = 0;
  for (int i = 0; i < MAX_SAMPLES; i++) {
    heartRateSamples[i] = 0;
    breathRateSamples[i] = 0;
    weightSamples[i] = 0;
  }
  dotCount = 0;
  lastDotUpdate = millis();
}

// Callback khi nhận tin nhắn MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  // Chuyển đổi message thành float (Weight_kg)
  latestWeight = message.toFloat();
}

// Kết nối MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Hàm cập nhật màn hình
void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(5, 10, "FUVI CAFE");
  u8g2.drawHLine(0, 15, 128);

  // Nếu Weight_kg <= 0, chỉ hiển thị nhiệt độ và độ ẩm
  if (latestWeight <= 0) {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5, 35, "Nhiet do:");
  char tempStr[10];
  sprintf(tempStr, "%.1f C", latestTemperature);
    u8g2.drawStr(70, 35, tempStr);
  
    u8g2.drawStr(5, 50, "Do am:");
  char humStr[10];
  sprintf(humStr, "%.1f %%", latestHumidity);
    u8g2.drawStr(70, 50, humStr);
  } else {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    if (sampleCount < MAX_SAMPLES) {
      String dots = "";
      for (int i = 0; i < dotCount; i++) dots += ".";
      u8g2.drawStr(5, 35, "Dang doc");
      u8g2.drawStr(60, 35, dots.c_str());    } else {      u8g2.drawStr(5, 25, "Nhip tim:");
      char heartRateStr[10];
      sprintf(heartRateStr, "%d bpm", averageInt(heartRateSamples, sampleCount));
      u8g2.drawStr(70, 25, heartRateStr);
      
      u8g2.drawStr(5, 35, "Nhip tho:");
      char respRateStr[10];
      sprintf(respRateStr, "%d rpm", averageInt(breathRateSamples, sampleCount));
      u8g2.drawStr(70, 35, respRateStr);
      
      u8g2.drawStr(5, 45, "Chieu cao:");
      char heightStr[10];
      sprintf(heightStr, "%.1f cm", latestHeight);
      u8g2.drawStr(70, 45, heightStr);
      
      u8g2.drawStr(5, 55, "BMI:");
      char bmiStr[10];
      sprintf(bmiStr, "%.1f", latestBMI);
      u8g2.drawStr(70, 55, bmiStr);
    }
  }
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  while (!Serial);

  // Kết nối Wi-Fi
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println(" connected");

  // Cấu hình MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  dht.begin(); 
  u8g2.begin();
  
  Serial.println("Adafruit VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5, 30, "VL53L0X Error!");
    u8g2.sendBuffer();
    while(1);
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(5, 30, "Khoi dong...");
  u8g2.drawStr(5, 50, "FUVI CAFE");
  u8g2.sendBuffer();
  delay(2000);
  Serial.println("System Initialized");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  VL53L0X_RangingMeasurementData_t measure;
  Serial.print("Reading a measurement... ");
  lox.rangingTest(&measure, false); 
  if (measure.RangeStatus != 4) {
    Serial.print("Distance (mm): "); 
    Serial.println(measure.RangeMilliMeter);
    latestDistance = measure.RangeMilliMeter; 
    // Tính chiều cao: 2000mm - khoảng cách đo được, đổi sang cm
    latestHeight = (2000 - latestDistance) / 10.0;
  } else {
    Serial.println(" out of range ");
    latestDistance = 0;
    latestHeight = 0;
  }

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  if (!isnan(temp) && !isnan(hum)) {
    latestTemperature = temp;
    latestHumidity = hum;
  } else {
    Serial.println("Lỗi đọc cảm biến DHT22!");
  }

  radar.Breath_Heart();        
  if (radar.sensor_report != 0x00) {
    switch (radar.sensor_report) {
      case HEARTRATEVAL:
        Serial.print("Sensor monitored the current heart rate value is: ");
        Serial.println(radar.heart_rate, DEC);
        if (radar.heart_rate == 0 || (radar.heart_rate >= 60 && radar.heart_rate <= 100)) {
          latestHeartRate = radar.heart_rate;
        } else {
          Serial.println("Heart rate out of range (60-100 bpm)! Keeping previous value.");
        }
        Serial.println("----------------------------");
        break;
      case HEARTRATEWAVE:
        Serial.print("The heart rate waveform(Sine wave) -- point 1: ");
        Serial.print(radar.heart_point_1);
        Serial.print(", point 2 : ");
        Serial.print(radar.heart_point_2);
        Serial.print(", point 3 : ");
        Serial.print(radar.heart_point_3);
        Serial.print(", point 4 : ");
        Serial.print(radar.heart_point_4);
        Serial.print(", point 5 : ");
        Serial.println(radar.heart_point_5);
        Serial.println("----------------------------");
        break;
      case BREATHNOR:
        Serial.println("Sensor detects current breath rate is normal.");
        Serial.println("----------------------------");
        break;
      case BREATHRAPID:
        Serial.println("Sensor detects current breath rate is too fast.");
        Serial.println("----------------------------");
        break;
      case BREATHSLOW:
        Serial.println("Sensor detects current breath rate is too slow.");
        Serial.println("----------------------------");
        break;
      case BREATHNONE:
        Serial.println("There is no breathing information yet, please wait...");
        Serial.println("----------------------------");
        break;
      case BREATHVAL:
        Serial.print("Sensor monitored the current breath rate value is: ");
        Serial.println(radar.breath_rate, DEC);
        latestRespirationRate = radar.breath_rate; 
        Serial.println("----------------------------");
        break;
      case BREATHWAVE:
        Serial.print("The breath rate waveform(Sine wave) -- point 1: ");
        Serial.print(radar.breath_point_1);
        Serial.print(", point 2 : ");
        Serial.print(radar.breath_point_2);
        Serial.print(", point 3 : ");
        Serial.print(radar.breath_point_3);
        Serial.print(", point 4 : ");
        Serial.print(radar.breath_point_4);
        Serial.print(", point 5 : ");
        Serial.println(radar.breath_point_5);
        Serial.println("----------------------------");
        break;
    }
  }

  // Kiểm tra thay đổi Weight_kg từ >0 sang 0 để đặt lại
  if (lastWeight > 0 && latestWeight <= 0) {
    resetSamples();
    Serial.println("Weight reset to 0, starting new measurement cycle");
  }
  lastWeight = latestWeight;  // Lấy mẫu mỗi 1 giây nếu Weight_kg > 0
  if (latestWeight > 0 && millis() - lastSampleTime >= sampleInterval) {
    // Tính BMI
    latestBMI = calculateBMI(averageFloat(weightSamples, sampleCount > 0 ? sampleCount : 1), latestHeight);
    
    if (sampleCount < MAX_SAMPLES) {
      heartRateSamples[sampleCount] = latestHeartRate;
      breathRateSamples[sampleCount] = latestRespirationRate;
      weightSamples[sampleCount] = latestWeight;
      sampleCount++;
    } else {
      // Dịch mảng để thêm mẫu mới
      for (int i = 1; i < MAX_SAMPLES; i++) {
        heartRateSamples[i-1] = heartRateSamples[i];
        breathRateSamples[i-1] = breathRateSamples[i];
        weightSamples[i-1] = weightSamples[i];
      }
      heartRateSamples[MAX_SAMPLES-1] = latestHeartRate;
      breathRateSamples[MAX_SAMPLES-1] = latestRespirationRate;
      weightSamples[MAX_SAMPLES-1] = latestWeight;
    }
    lastSampleTime = millis();
  }

  if (millis() - lastDotUpdate >= dotInterval) {
    dotCount = (dotCount % 3) + 1; // 1, 2, 3, rồi lặp
    lastDotUpdate = millis();
  }

  if (millis() - lastDisplayUpdate >= displayInterval) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  delay(200);
}