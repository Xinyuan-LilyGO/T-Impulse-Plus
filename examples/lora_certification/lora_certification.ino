/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2025-09-12 16:42:57
 * @LastEditTime: 2026-08-24 15:24:13
 * @License: GPL 3.0
 */
#include "RadioLib.h"
#include "cpp_bus_driver_library.h"
#include "pin_config.h"

constexpr float RF_FREQUENCY_MHZ = 868.0;
constexpr float RF_BANDWIDTH_KHZ = 500.0;
constexpr uint8_t LORA_SPREADING_FACTOR = 12;
constexpr uint8_t LORA_CODING_RATE = 8;
constexpr uint8_t LORA_SYNC_WORD = 0xAB;
constexpr int8_t RF_OUTPUT_POWER_DBM = 7;
constexpr float RF_CURRENT_LIMIT_MA = 140.0;
constexpr uint16_t LORA_PREAMBLE_LENGTH = 16;
constexpr bool LORA_CRC_ENABLED = false;
constexpr uint32_t TX_INTERVAL_MS = 3000;
constexpr size_t PAYLOAD_SIZE = 32;

SPIClass radioSpi(NRF_SPIM3, SX1262_MISO, SX1262_SCLK, SX1262_MOSI);
SX1262 radio = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, radioSpi);

auto sgm41562I2cBus = std::make_shared<cpp_bus_driver::HardwareI2c2>(
    SGM41562_SDA, SGM41562_SCL, &Wire);
auto sgm41562 = std::make_unique<cpp_bus_driver::Sgm41562xx>(
    sgm41562I2cBus, SGM41562_ADDRESS);

uint8_t payload[PAYLOAD_SIZE];
uint32_t packetCounter = 0;
uint32_t nextTransmissionMs = 0;

void haltOnRadioError(const char *operation, int16_t state)
{
    Serial.print("[SX1262] ");
    Serial.print(operation);
    Serial.print(" failed, code ");
    Serial.println(state);

    while (true)
    {
        delay(1000);
    }
}

void preparePayload()
{
    for (size_t i = 0; i < PAYLOAD_SIZE; ++i)
    {
        payload[i] = (i & 1U) ? 0xAA : 0x55;
    }

    payload[0] = static_cast<uint8_t>(packetCounter >> 24);
    payload[1] = static_cast<uint8_t>(packetCounter >> 16);
    payload[2] = static_cast<uint8_t>(packetCounter >> 8);
    payload[3] = static_cast<uint8_t>(packetCounter);
}

bool initializePowerManagement()
{
    // 按原始测试程序的时序重新启动 3.3 V 外设电源。
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);
    delay(100);
    digitalWrite(RT9080_EN, LOW);
    delay(100);
    digitalWrite(RT9080_EN, HIGH);
    delay(100);

    // 初始化充电管理芯片，并保持与原始测试程序相同的运输模式延时。
    if (!sgm41562->Init())
    {
        return false;
    }

    return sgm41562->SetShippingModeDelay(
        cpp_bus_driver::Sgm41562xx::ShippingModeDelay::k1Second);
}

void setup()
{
    Serial.begin(115200);

    // 未连接串口监视器时不阻塞射频测试，最多等待 3 秒。
    const uint32_t serialWaitStartedMs = millis();
    while (!Serial && (millis() - serialWaitStartedMs < 3000))
    {
        delay(10);
    }

    if (!initializePowerManagement())
    {
        Serial.println("SGM41562xx initialization failed");
        while (true)
        {
            delay(1000);
        }
    }
    Serial.println("SGM41562xx initialization successful");

    radioSpi.begin();

    int16_t state = radio.begin(
        RF_FREQUENCY_MHZ,
        RF_BANDWIDTH_KHZ,
        LORA_SPREADING_FACTOR,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        RF_OUTPUT_POWER_DBM,
        LORA_PREAMBLE_LENGTH,
        SX1262_TCXO_VOLTAGE,
        SX1262_USE_REGULATOR_LDO);
    if (state != RADIOLIB_ERR_NONE)
    {
        haltOnRadioError("initialization", state);
    }

    radio.setRfSwitchPins(SX1262_RF_VC2, SX1262_RF_VC1);

    state = radio.setCurrentLimit(RF_CURRENT_LIMIT_MA);
    if (state != RADIOLIB_ERR_NONE)
    {
        haltOnRadioError("current limit configuration", state);
    }

    state = radio.setCRC(LORA_CRC_ENABLED);
    if (state != RADIOLIB_ERR_NONE)
    {
        haltOnRadioError("CRC configuration", state);
    }

    Serial.println("T-Impulse Plus LoRa certification test");
    Serial.println("Frequency: 868.0 MHz");
    Serial.println("Bandwidth: 500.0 kHz");
    Serial.println("Output power: 7 dBm");
    Serial.println("Transmit interval: 3000 ms");

    nextTransmissionMs = millis();
}

void loop()
{
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - nextTransmissionMs) < 0)
    {
        return;
    }

    nextTransmissionMs = now + TX_INTERVAL_MS;
    preparePayload();

    Serial.print("[SX1262] Transmitting packet ");
    Serial.print(packetCounter);
    Serial.print(" ... ");

    const int16_t state = radio.transmit(payload, PAYLOAD_SIZE);
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println("success");
    }
    else
    {
        Serial.print("failed, code ");
        Serial.println(state);
    }

    ++packetCounter;
}
