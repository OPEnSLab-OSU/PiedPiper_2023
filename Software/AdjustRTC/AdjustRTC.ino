/* 
  Adjust RTC on the HYPNOS board to compilation time of sketch
*/

#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"

#define HYPNOS_5VR 6
#define HYPNOS_3VR 5

RTC_DS3231 rtc;

void setup() {
  Serial.begin(2000000);
  delay(2000);
  pinMode(HYPNOS_5VR, OUTPUT);
  pinMode(HYPNOS_3VR, OUTPUT);

  digitalWrite(HYPNOS_3VR, LOW);
  Wire.begin();

  if (!rtc.begin()) {
    Serial.println("RTC failed to begin.");
    initializationFailFlash();
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  delay(4000);

  char date[10] = "hh:mm:ss";
  rtc.now().toString(date);
  Serial.print("Current RTC Time: ");
  Serial.println(date);

  Wire.end();
  digitalWrite(HYPNOS_3VR, HIGH);

}

void loop() {
}

void initializationFailFlash() {
  while(1) {
    digitalWrite(HYPNOS_3VR, LOW);
    delay(500);
    digitalWrite(HYPNOS_3VR, HIGH);
    delay(500);
  }
}
