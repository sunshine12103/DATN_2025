#include "Arduino.h"
#include <60ghzbreathheart.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "Adafruit_VL53L0X.h"
#include "DHT.h"

#define DHTPIN 32  
#define RELAY_PIN 33
#define DHTTYPE DHT22 
BreathHeart_60GHz radar = BreathHeart_60GHz(&Serial2);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
DHT dht(DHTPIN, DHTTYPE); 

#define MAX_SAMPLES 60
int heartRateSamples[MAX_SAMPLES];
int breathRateSamples[MAX_SAMPLES];
int sampleCount = 0;
unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 1000;

int latestHeartRate = 0;
int latestRespirationRate = 0;
int latestDistance = 0; 
float latestTemperature = 0;
float latestHumidity = 0; 
unsigned long lastRadarUpdate = 0;
const unsigned long radarInterval = 1000;
const unsigned long displayInterval = 500;
unsigned long lastDisplayUpdate = 0;
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R2, /* clock=*/ 18, /* data=*/ 23, /* CS=*/ 5, /* reset=*/ 22);

int average(int* arr, int count);

void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr); 
  u8g2.drawStr(5, 15, "FUVI CAFE");
  u8g2.drawHLine(0, 20, 128); 

  u8g2.setFont(u8g2_font_ncenB08_tr); 
  u8g2.drawStr(5, 35, "Nhiet do:");
  char tempStr[10];
  sprintf(tempStr, "%.1f C", latestTemperature);
  u8g2.drawStr(70, 35, tempStr);
  
  u8g2.drawStr(5, 50, "Do am:");
  char humStr[10];
  sprintf(humStr, "%.1f %%", latestHumidity);
  u8g2.drawStr(70, 50, humStr);
  
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  while (!Serial);
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
  Serial.println(F("VL53L0X API Simple Ranging example\n\n")); 
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(5, 30, "Khoi dong...");
  u8g2.drawStr(5, 50, "FUVI CAFE"); // Thay RADAR R60ABD1 bằng FUVI CAFE
  u8g2.sendBuffer();
  delay(2000);
  Serial.println("Radar R60ABD1 Initialized");
}

void loop() {
  VL53L0X_RangingMeasurementData_t measure;
  Serial.print("Reading a measurement... ");
  lox.rangingTest(&measure, false); 
  if (measure.RangeStatus != 4) {
    Serial.print("Distance (mm): "); 
    Serial.println(measure.RangeMilliMeter);
    latestDistance = measure.RangeMilliMeter; 
  } else {
    Serial.println(" out of range ");
    latestDistance = 0;
  }
  latestTemperature = dht.readTemperature();
  latestHumidity = dht.readHumidity();
  if(isnan(latestTemperature) || isnan(latestHumidity)){
    Serial.println("Lỗi đọc cảm biến DHT22!");
  }
  radar.Breath_Heart();        
  if (radar.sensor_report != 0x00) {
    switch (radar.sensor_report) {
      case HEARTRATEVAL:
        Serial.print("Sensor monitored the current heart rate value is: ");
        Serial.println(radar.heart_rate, DEC);
        latestHeartRate = radar.heart_rate;
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
  // Lấy mẫu mỗi 1 giây
  if (millis() - lastSampleTime >= sampleInterval) {
    if (sampleCount < MAX_SAMPLES) {
      heartRateSamples[sampleCount] = latestHeartRate;
      breathRateSamples[sampleCount] = latestRespirationRate;
      sampleCount++;
    } else {
      // Dịch mảng sang trái để thêm mẫu mới
      for (int i = 1; i < MAX_SAMPLES; i++) {
        heartRateSamples[i-1] = heartRateSamples[i];
        breathRateSamples[i-1] = breathRateSamples[i];
      }
      heartRateSamples[MAX_SAMPLES-1] = latestHeartRate;
      breathRateSamples[MAX_SAMPLES-1] = latestRespirationRate;
    }
    lastSampleTime = millis();
  }
  if (millis() - lastDisplayUpdate >= displayInterval) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  delay(200);
}

int average(int* arr, int count) {
  if (count == 0) return 0;
  int sum = 0;
  for (int i = 0; i < count; i++) sum += arr[i];
  return sum / count;
}