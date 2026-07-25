/*
 * @Description: GPS test
 * @Author: LILYGO_L
 * @Date: 2024-10-25 17:57:30
 * @LastEditTime: 2026-01-23 15:01:14
 * @License: GPL 3.0
 */
#include "cpp_bus_driver_library.h"
#include "pin_config.h"
#include <Adafruit_TinyUSB.h>

#define MAX_UART_RX_BUFFER_SIZE 1024

size_t CycleTime = 0;

bool Gps_Positioning_Flag = false;
size_t Gps_Positioning_Time = 0;

auto Uart_Rx_Buffer = std::make_unique<uint8_t[]>(MAX_UART_RX_BUFFER_SIZE);
size_t Uart_Rx_Count = 0;

auto Nrf52840_Gnss = std::make_shared<cpp_bus_driver::GnssParser>();

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

    // 3.3V Power ON
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);

    pinMode(GPS_EN, OUTPUT);
    digitalWrite(GPS_EN, LOW); // gps开启

    Serial2.setPins(GPS_UART_TX, GPS_UART_RX);
    Serial2.begin(38400);
}

void loop()
{
    while (Serial2.available() && Uart_Rx_Count < MAX_UART_RX_BUFFER_SIZE)
    {
        Uart_Rx_Buffer[Uart_Rx_Count] = Serial2.read();
        Uart_Rx_Count++;
    }

    if (millis() > CycleTime)
    {
        if (Gps_Positioning_Flag == false)
        {
            Gps_Positioning_Time += 5;
        }

        // 检查Serial2是否有可用数据
        if (Uart_Rx_Count >= MAX_UART_RX_BUFFER_SIZE)
        {
            Uart_Rx_Buffer[MAX_UART_RX_BUFFER_SIZE - 1] = '\0';

            // 打印RMC的相关信息
            printf("---begin---\n%s \n---end---\n", Uart_Rx_Buffer.get());

            printf("---RMC---\n");

            // 创建Rmc对象用于存储解析结果
            cpp_bus_driver::GnssParser::Rmc rmc;

            if (Gps_Positioning_Flag == false)
            {
                printf("Gps N:%ds\n", Gps_Positioning_Time);
            }
            else
            {
                printf("Gps Y:%d s\n", Gps_Positioning_Time);
            }

            // 调用 ParseRmcInfo 进行解码
            if (Nrf52840_Gnss->ParseRmcInfo(Uart_Rx_Buffer.get(), Uart_Rx_Count, rmc) == true)
            {
                printf("location status: %s\n", (rmc.location_status).c_str());

                if (rmc.data.update_flag == true)
                {
                    printf("utc data: %d/%d/%d\n", rmc.data.year + 2000, rmc.data.month, rmc.data.day);
                    rmc.data.update_flag = false;
                }
                if (rmc.utc.update_flag == true)
                {
                    printf("utc time: %d:%d:%.03f\n", rmc.utc.hour, rmc.utc.minute, rmc.utc.second);
                    printf("china time: %d:%d:%.03f\n", (rmc.utc.hour + 8 + 24) % 24, rmc.utc.minute, rmc.utc.second);
                    rmc.utc.update_flag = false;
                }

                if ((rmc.location.lat.update_flag == true) && (rmc.location.lat.direction_update_flag == true))
                {
                    printf("location lat degrees: %d \nlocation lat minutes: %.10lf \nlocation lat degrees_minutes: %.10lf \nlocation lat direction: %s\n",
                           rmc.location.lat.degrees, rmc.location.lat.minutes, rmc.location.lat.degrees_minutes, (rmc.location.lat.direction).c_str());
                    rmc.location.lat.update_flag = false;
                    rmc.location.lat.direction_update_flag = false;

                    Gps_Positioning_Flag = true;
                }
                if ((rmc.location.lon.update_flag == true) && (rmc.location.lon.direction_update_flag == true))
                {
                    printf("location lon degrees: %d \nlocation lon minutes: %.10lf \nlocation lon degrees_minutes: %.10lf \nlocation lon direction: %s\n",
                           rmc.location.lon.degrees, rmc.location.lon.minutes, rmc.location.lon.degrees_minutes, (rmc.location.lon.direction).c_str());
                    rmc.location.lon.update_flag = false;
                    rmc.location.lon.direction_update_flag = false;

                    Gps_Positioning_Flag = true;
                }
            }
            else
            {
                printf("gps data: read fail\n");
            }

            Uart_Rx_Count = 0;
        }
        else
        {
            printf("Gps E:%ds\n", Gps_Positioning_Time);
        }

        CycleTime = millis() + 5000;
    }
}
