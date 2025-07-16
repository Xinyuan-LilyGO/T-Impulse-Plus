/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2024-10-25 17:57:30
 * @LastEditTime: 2025-06-11 10:22:01
 * @License: GPL 3.0
 */
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "pin_config.h"

size_t CycleTime = 0;

void setup()
{
    Serial.begin(115200);
    Serial.println("Ciallo");

    // 3.3V Power ON
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);

    Serial2.setPins(GPS_UART_TX, GPS_UART_RX);
    Serial2.begin(38400);
}

void loop()
{
    while (Serial2.available() > 0)
    {
        // Serial.printf("%c", Serial2.read());

        Serial.write(Serial2.read());
    }

    if (millis() > CycleTime)
    {
        Serial.println("Ciallo");
        // Serial2.println("Ciallo");
        CycleTime = millis() + 1000;
    }

    // delay(3000);
}
