/*
 * @Description: SGM41562系列充电管理芯片与电池电压示例
 * @Author: LILYGO_L
 * @Date: 2024-03-26 15:51:59
 * @LastEditTime: 2026-04-16 16:37:55
 * @License: GPL 3.0
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include "pin_config.h"
#include "cpp_bus_driver_library.h"

// ADC使用3.0V内部参考和12位分辨率，电池输入经过1:1分压
constexpr float kAdcReferenceVoltageMv = 3000.0F;
constexpr float kAdcResolution = 4096.0F;
constexpr float kBatteryVoltageDividerRatio = 2.0F;

/**
 * @brief 读取并换算电池电压
 * @return 电池电压，单位为V
 */
float ReadBatteryVoltage() {
  const uint16_t adc_value = analogRead(BATTERY_ADC_DATA);
  return static_cast<float>(adc_value) * kAdcReferenceVoltageMv /
         kAdcResolution / 1000.0F * kBatteryVoltageDividerRatio;
}

/**
 * @brief 输出测试所需的关键充电与保护配置
 * @param config 充电管理芯片关键配置
 */
void PrintChargerConfig(
    const cpp_bus_driver::Sgm41562xx::ChargerConfig& config) {
  Serial.printf("charger config:\n");
  Serial.printf("  charge: %s, high impedance: %s\n",
      config.charge_enabled ? "enabled" : "disabled",
      config.high_impedance_enabled ? "enabled" : "disabled");
  Serial.printf("  input voltage limit: %u mV\n",
      static_cast<unsigned int>(config.input_voltage_limit_mv));
  if (config.input_current_limit_enabled) {
    Serial.printf("  input current limit: %u mA\n",
        static_cast<unsigned int>(config.input_current_limit_ma));
  } else {
    Serial.printf("  input current limit: released\n");
  }
  Serial.printf("  fast charge current: %u mA\n",
      static_cast<unsigned int>(config.fast_charge_current_ma));
  Serial.printf("  termination current: %u mA\n",
      static_cast<unsigned int>(config.termination_current_ma));
  Serial.printf("  charge voltage limit: %u mV\n",
      static_cast<unsigned int>(config.charge_voltage_limit_mv));
  Serial.printf("  system regulation voltage: %u mV\n",
      static_cast<unsigned int>(config.system_voltage_regulation_mv));
  Serial.printf("  input overvoltage threshold: %u mV\n",
      static_cast<unsigned int>(
          config.input_overvoltage_threshold_mv));

  if (config.watchdog_enabled) {
    Serial.printf("  watchdog: enabled, %u s\n",
        static_cast<unsigned int>(config.watchdog_timeout_s));
  } else {
    Serial.printf("  watchdog: disabled\n");
  }

  if (config.safety_timer_enabled) {
    Serial.printf("  safety timer: enabled, %u h, PPM extension: %s\n",
        static_cast<unsigned int>(config.safety_timer_hours),
        config.safety_timer_extended_in_ppm ? "enabled" : "disabled");
  } else {
    Serial.printf("  safety timer: disabled\n");
  }

  Serial.printf("  charge termination: %s\n",
      config.charge_termination_enabled ? "enabled" : "disabled");
  Serial.printf("  NTC: %s, thermal regulation threshold: %u C\n",
      config.ntc_enabled ? "enabled" : "disabled",
      static_cast<unsigned int>(
          config.thermal_regulation_threshold_c));
  Serial.printf("  input voltage loop: %s, PCB overtemperature: %s\n",
      config.input_voltage_loop_enabled ? "enabled" : "disabled",
      config.pcb_overtemperature_protection_enabled
          ? "enabled"
          : "disabled");
}

auto sgm41562_i2c_bus = std::make_shared<cpp_bus_driver::HardwareI2c2>(
    SGM41562_SDA, SGM41562_SCL, &Wire);

auto sgm41562 = std::make_unique<cpp_bus_driver::Sgm41562xx>(
    sgm41562_i2c_bus, SGM41562_ADDRESS);
bool sgm41562_initialized = false;

void setup() {
  Serial.begin(115200);
  uint8_t serial_init_count = 0;
  while (!Serial) {
    delay(100);  // 等待原生USB串口就绪
    serial_init_count++;
    if (serial_init_count > 30) {
      break;
    }
  }

  Serial.println("Ciallo");

  // 开启3.3V电源
  pinMode(RT9080_EN, OUTPUT);
  digitalWrite(RT9080_EN, HIGH);

  // 配置电池电压采样
  pinMode(BATTERY_ADC_DATA, INPUT);
  pinMode(BATTERY_MEASUREMENT_CONTROL, OUTPUT);
  digitalWrite(BATTERY_MEASUREMENT_CONTROL, HIGH);
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);

  if (!sgm41562->Init()) {
    Serial.println("SGM41562xx init failed");
    return;
  }

  uint8_t device_id = 0;
  if (!sgm41562->GetDeviceId(device_id)) {
    Serial.println("Failed to read SGM41562xx device ID");
    return;
  }
  Serial.printf("Detected chip: %s (ID: 0x%02X)\n",
      cpp_bus_driver::Sgm41562xx::ChipTypeToString(
          sgm41562->GetChipType()),
      device_id);

  cpp_bus_driver::Sgm41562xx::ChargerConfig charger_config;
  if (sgm41562->GetChargerConfig(charger_config)) {
    PrintChargerConfig(charger_config);
  } else {
    Serial.printf("failed to read charger config\n");
  }

  sgm41562_initialized = true;
}

void loop() {
  if (!sgm41562_initialized) {
    delay(1000);
    return;
  }

  Serial.printf("----------sgm41562----------\n");

  cpp_bus_driver::Sgm41562xx::IrqStatus irq_status;
  if (sgm41562->GetIrqStatus(irq_status)) {
    Serial.printf("irq status:\n");
    Serial.printf(
        "  input power fault: %d\n", irq_status.input_power_fault);
    Serial.printf("  thermal shutdown: %d\n", irq_status.thermal_shutdown);
    Serial.printf("  battery over voltage fault: %d\n",
        irq_status.battery_overvoltage_fault);
    Serial.printf("  safety timer expiration fault: %d\n",
        irq_status.safety_timer_expired);
    Serial.printf("  ntc exceeding hot: %d\n", irq_status.ntc_hot);
    Serial.printf("  ntc exceeding cold: %d\n", irq_status.ntc_cold);
  } else {
    Serial.printf("failed to read irq status\n");
  }

  cpp_bus_driver::Sgm41562xx::ChipStatus chip_status;
  if (sgm41562->GetChipStatus(chip_status)) {
    Serial.printf("chip status:\n");
    Serial.printf(
        "  watchdog expiration: %d\n", chip_status.watchdog_expired);
    Serial.printf("  charge status: ");
    switch (chip_status.charge_status) {
      case cpp_bus_driver::Sgm41562xx::ChargeStatus::kNotCharging:
        Serial.printf("not_charging\n");
        break;
      case cpp_bus_driver::Sgm41562xx::ChargeStatus::kPrecharge:
        Serial.printf("precharge\n");
        break;
      case cpp_bus_driver::Sgm41562xx::ChargeStatus::kCharging:
        Serial.printf("charge\n");
        break;
      case cpp_bus_driver::Sgm41562xx::ChargeStatus::kChargeComplete:
        Serial.printf("charge_done\n");
        break;
      default:
        Serial.printf("unknown\n");
        break;
    }
    Serial.printf("  power path management mode: %d\n",
        chip_status.power_path_management_active);
    Serial.printf(
        "  input power status: %d\n", chip_status.input_power_good);
    Serial.printf("  thermal regulation: %d\n",
        chip_status.thermal_regulation_active);
  } else {
    Serial.printf("failed to read chip status\n");
  }
  Serial.printf("battery voltage: %.03f V\n", ReadBatteryVoltage());
  Serial.printf("\n");

  delay(1000);
}
