/*
 * @Description: IIC Scan
 * @Author: LILYGO_L
 * @Date: 2024-03-26 15:51:59
 * @LastEditTime: 2025-07-17 18:16:22
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

    Sgm41562->begin();
}

void loop()
{
    // scan_i2c_device(Wire);

    Serial.printf("----------sgm41562----------\n");

    Cpp_Bus_Driver::Sgm41562xx::Irq_Status irq_status;
    if (Sgm41562->parse_irq_status(Sgm41562->get_irq_flag(), irq_status))
    {
        Serial.printf("irq status:\n");
        Serial.printf("  input power fault: %d\n", irq_status.Input_power_fault_flag);
        Serial.printf("  thermal shutdown: %d\n", irq_status.thermal_shutdown_flag);
        Serial.printf("  battery over voltage fault: %d\n", irq_status.battery_over_voltage_fault_flag);
        Serial.printf("  safety timer expiration fault: %d\n", irq_status.safety_timer_expiration_fault_flag);
        Serial.printf("  ntc exceeding hot: %d\n", irq_status.ntc_exceeding_hot_flag);
        Serial.printf("  ntc exceeding cold: %d\n", irq_status.ntc_exceeding_cold_flag);
    }
    else
    {
        Serial.printf("failed to parse irq status\n");
    }

    Cpp_Bus_Driver::Sgm41562xx::Chip_Status chip_status;
    if (Sgm41562->parse_chip_status(Sgm41562->get_chip_status(), chip_status))
    {
        Serial.printf("chip status:\n");
        Serial.printf("  watchdog expiration: %d\n", chip_status.watchdog_expiration_flag);
        Serial.printf("  charge status: ");
        switch (chip_status.charge_status)
        {
        case Cpp_Bus_Driver::Sgm41562xx::Charge_Status::NOT_CHARGING:
            Serial.printf("not_charging\n");
            break;
        case Cpp_Bus_Driver::Sgm41562xx::Charge_Status::PRECHARGE:
            Serial.printf("precharge\n");
            break;
        case Cpp_Bus_Driver::Sgm41562xx::Charge_Status::CHARGE:
            Serial.printf("charge\n");
            break;
        case Cpp_Bus_Driver::Sgm41562xx::Charge_Status::CHARGE_DONE:
            Serial.printf("charge_done\n");
            break;
        default:
            Serial.printf("unknown\n");
            break;
        }
        Serial.printf("  power path management mode: %d\n", chip_status.device_in_power_path_management_mode_flag);
        Serial.printf("  input power status: %d\n", chip_status.input_power_status_flag);
        Serial.printf("  thermal regulation: %d\n", chip_status.thermal_regulation_status_flag);
    }
    else
    {
        Serial.printf("failed to parse chip status\n");
    }
    Serial.printf("\n");

    delay(1000);
}