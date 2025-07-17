/*
 * @Description: IIC Scan
 * @Author: LILYGO_L
 * @Date: 2024-03-26 15:51:59
 * @LastEditTime: 2025-07-17 12:07:58
 * @License: GPL 3.0
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include "pin_config.h"
#include "cpp_bus_driver_library.h"

auto Sgm41562_IIC_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_2>(SGM41562_SDA, SGM41562_SCL, &Wire);

auto Sgm41562 = std::make_unique<Cpp_Bus_Driver::Sgm41562xx>(Sgm41562_IIC_Bus, SGM41562_ADDRESS, DEFAULT_CPP_BUS_DRIVER_VALUE);

void setup()
{
    Serial.begin(115200);
    uint8_t serial_init_count = 0;
    while (!Serial)
    {
        delay(100); // wait for native usb
        serial_init_count++;
        if (serial_init_count > 30)
        {
            break;
        }
    }

    Serial.println("Ciallo");

    // 3.3V Power ON
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);

    pinMode(ICM20948_INT, INPUT_PULLUP);

    Sgm41562->begin();
}

void loop()
{
    // scan_i2c_device(Wire);
    delay(1000);
}