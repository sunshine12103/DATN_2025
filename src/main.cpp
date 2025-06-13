#include "Arduino.h"
#include <60ghzbreathheart.h>
#include <U8g2lib.h>
#include <SPI.h>

BreathHeart_60GHz radar = BreathHeart_60GHz(&Serial2);

int latestHeartRate = 0;
int latestRespirationRate = 0;
unsigned long lastRadarUpdate = 0;
const unsigned long radarInterval = 1000;
const unsigned long displayInterval = 500;
unsigned long lastDisplayUpdate = 0;

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R2, /* clock=*/ 18, /* data=*/ 23, /* CS=*/ 5, /* reset=*/ 22); // ESP32, xoay 180°

void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  
  u8g2.drawStr(5, 15, "RADAR R60ABD1");
  u8g2.drawHLine(0, 18, 128);
  
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(5, 33, "Nhip tim:");
  u8g2.setFont(u8g2_font_ncenB12_tr);
  char heartRateStr[10];
  sprintf(heartRateStr, "%d", latestHeartRate);
  u8g2.drawStr(70, 33, heartRateStr);
  
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(5, 53, "Nhip tho:");
  u8g2.setFont(u8g2_font_ncenB12_tr);
  char respRateStr[10];
  sprintf(respRateStr, "%d", latestRespirationRate);
  u8g2.drawStr(70, 53, respRateStr);
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  while (!Serial);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(5, 30, "Khoi dong...");
  u8g2.drawStr(5, 50, "RADAR R60ABD1");
  u8g2.sendBuffer();
  delay(2000);
  Serial.println("Radar R60ABD1 Initialized");
}

void loop() {
  radar.Breath_Heart();        
  if (radar.sensor_report != 0x00) {
    switch (radar.sensor_report) {
      case HEARTRATEVAL:
        Serial.print("Sensor monitored the current heart rate value is: ");
        Serial.println(radar.heart_rate, DEC);
        latestHeartRate = radar.heart_rate; // Update heart rate
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
        latestRespirationRate = radar.breath_rate; // Update respiration rate
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

  // Update display at specified interval
  if (millis() - lastDisplayUpdate >= displayInterval) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  delay(200);
}