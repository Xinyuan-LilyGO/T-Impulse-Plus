/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2025-09-12 16:42:57
 * @LastEditTime: 2025-09-22 15:28:52
 * @License: GPL 3.0
 */
#include "pin_config.h"
#include "cpp_bus_driver_library.h"
#include <Adafruit_TinyUSB.h>

constexpr uint32_t GNSS_BAUD_RATE = 38400;
constexpr uint32_t USB_BAUD_RATE = GNSS_BAUD_RATE;

auto sgm41562I2cBus = std::make_shared<cpp_bus_driver::HardwareI2c2>(
    SGM41562_SDA, SGM41562_SCL, &Wire);
auto sgm41562 = std::make_unique<cpp_bus_driver::Sgm41562xx>(
    sgm41562I2cBus, SGM41562_ADDRESS);

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
    // 在启动 USB 串口前完成电源管理初始化，避免驱动日志混入原始 GNSS 数据。
    const bool powerManagementReady = initializePowerManagement();

    // USB CDC 不受物理波特率限制，这里设置为 38400，与 GNSS UART 保持一致。
    Serial.begin(USB_BAUD_RATE);

    // 电源管理初始化失败时停止启动，避免 GNSS 在异常供电状态下工作。
    if (!powerManagementReady)
    {
        Serial.println("SGM41562xx initialization failed");
        while (true)
        {
            delay(1000);
        }
    }

    // 拉低使能引脚，开启 GNSS 模块。
    pinMode(GPS_EN, OUTPUT);
    digitalWrite(GPS_EN, LOW);

    // Adafruit nRF52 的 setPins() 参数顺序为 MCU RX、MCU TX。
    // 板级引脚名称是以 GNSS 模块信号方向命名的，因此这里需要交叉连接。
    Serial2.setPins(GPS_UART_TX, GPS_UART_RX);
    Serial2.begin(GNSS_BAUD_RATE);
}

void loop()
{
    // 按字节原样输出 GNSS 数据，保证电脑端工具能够直接识别原始 NMEA 数据。
    while (Serial2.available() > 0)
    {
        Serial.write(Serial2.read());
    }

    // 允许电脑端认证工具向 GNSS 模块发送配置命令。
    while (Serial.available() > 0)
    {
        Serial2.write(Serial.read());
    }
}
