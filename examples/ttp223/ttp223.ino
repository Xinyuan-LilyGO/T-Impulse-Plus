/*
 * @Description: IIC Scan
 * @Author: LILYGO_L
 * @Date: 2024-03-26 15:51:59
 * @LastEditTime: 2026-07-17 08:59:39
 * @License: GPL 3.0
 */
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "pin_config.h"

void setup()
{
    Serial.begin(115200);
    Serial.println("Ciallo");

    // 3.3V Power ON
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);

    pinMode(TTP223_KEY, INPUT);
}

void loop()
{
    if (digitalRead(TTP223_KEY) == LOW)
    {
        delay(300);

        Serial.println("TTP223_KEY trigger");
    }
}