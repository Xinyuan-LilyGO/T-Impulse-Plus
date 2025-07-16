/**************************************************************************
 This is an example for our Monochrome OLEDs based on SSD1306 drivers

 Pick one up today in the adafruit shop!
 ------> http://www.adafruit.com/category/63_98

 This example is for a 128x32 pixel display using I2C to communicate
 3 pins are required to interface (two I2C and one reset).

 Adafruit invests time and resources providing this open
 source code, please support Adafruit and open-source
 hardware by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries,
 with contributions from the open source community.
 BSD license, check license.txt for more information
 All text above, and the splash screen below must be
 included in any redistribution.
 **************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pin_config.h"
#include "Display_Fonts.h"
#include "TinyGPSPlus.h"
#include <bluefruit.h>

struct BLE_Uart_Operator
{
    using state = enum {
        UNCONNECTED, // not connected
        CONNECTED,   // connected already
    };

    struct
    {
        bool state_flag = state::UNCONNECTED;
        char device_name[32] = {'\0'};
    } connection;

    struct
    {
        uint8_t receive_data[100] = {'\0'};
    } transmission;

    bool initialization_flag = false;
};

bool Gps_Positioning_Flag = false;
size_t Gps_Positioning_Time = 0;

TinyGPSPlus gps;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, SCREEN_RST);

size_t CycleTime = 0;

/* UART Serivce: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * UART RXD    : 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
 * UART TXD    : 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
 */

// BLE Service
BLEDfu bledfu;   // OTA DFU service
BLEDis bledis;   // device information
BLEUart bleuart; // uart over ble

BLE_Uart_Operator BLE_Uart_OP;

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
    // Get the reference to current connection
    BLEConnection *connection = Bluefruit.Connection(conn_handle);

    char central_name[32] = {0};
    connection->getPeerName(central_name, sizeof(central_name));

    Serial.print("Connected to ");
    Serial.println(central_name);

    strcpy(BLE_Uart_OP.connection.device_name, central_name);

    BLE_Uart_OP.connection.state_flag = BLE_Uart_OP.state::CONNECTED;
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
    (void)conn_handle;
    (void)reason;

    Serial.println();
    Serial.print("Disconnected, reason = 0x");
    Serial.println(reason, HEX);
    BLE_Uart_OP.connection.state_flag = BLE_Uart_OP.state::UNCONNECTED;
}

void startAdv(void)
{
    // Advertising packet
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();

    // Include bleuart 128-bit uuid
    Bluefruit.Advertising.addService(bleuart);

    // Secondary Scan Response packet (optional)
    // Since there is no room for 'Name' in Advertising packet
    Bluefruit.ScanResponse.addName();

    /* Start Advertising
     * - Enable auto advertising if disconnected
     * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
     * - Timeout for fast mode is 30 seconds
     * - Start(timeout) with timeout = 0 will advertise forever (until connected)
     *
     * For recommended advertising interval
     * https://developer.apple.com/library/content/qa/qa1931/_index.html
     */
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
    Bluefruit.Advertising.setFastTimeout(30);   // number of seconds in fast mode
    Bluefruit.Advertising.start(0);             // 0 = Don't stop advertising after n seconds
}

bool BLE_Uart_Initialization(void)
{
    // Setup the BLE LED to be enabled on CONNECT
    // Note: This is actually the default behavior, but provided
    // here in case you want to control this LED manually via PIN 19
    Bluefruit.autoConnLed(true);

    // Config the peripheral connection with maximum bandwidth
    // more SRAM required by SoftDevice
    // Note: All config***() function must be called before begin()
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

    if (Bluefruit.begin() == false)
    {
        Serial.println("BLE initialization failed");
        return false;
    }
    Serial.println("BLE initialization successful");

    Bluefruit.setTxPower(8); // Check bluefruit.h for supported values
    // Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections
    Bluefruit.Periph.setConnectCallback(connect_callback);
    Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

    // To be consistent OTA DFU should be added first if it exists
    bledfu.begin();

    // Configure and Start Device Information Service
    bledis.setManufacturer("LILYGO Industries");
    bledis.setModel("T-Impulse-Plus");
    bledis.begin();

    // Configure and Start BLE Uart Service
    bleuart.begin();

    // Set up and start advertising
    startAdv();

    Serial.println("Please use the BLE debugging tool to connect to the development board.");
    Serial.println("Once connected, enter character(s) that you wish to send");

    return true;
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Ciallo");

    Serial2.setPins(GPS_UART_TX, GPS_UART_RX);
    Serial2.begin(38400);

    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);

    pinMode(GPS_1PPS, INPUT);

    // 3.3V Power ON
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);

    if (BLE_Uart_Initialization() == true)
    {
        BLE_Uart_OP.initialization_flag = true;
        Serial.println("BLE_Uart_Initialization successful");
    }
    else
    {
        BLE_Uart_OP.initialization_flag = false;
        Serial.println("BLE_Uart_Initialization fail");
    }

    Wire.setPins(SCREEN_SDA, SCREEN_SCL);
    Wire.begin();

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println("SSD1306 allocation failed");
        while (1)
        {
            /* code */
        }
    }

    display.setOffsetCursor(32, 32);
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.fillScreen(BLACK);

    display.display();

    delay(100);
}

void log_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[128];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    Serial.print(buffer);
    if ((BLE_Uart_OP.initialization_flag == true) && (BLE_Uart_OP.connection.state_flag == BLE_Uart_OP.state::CONNECTED))
    {
        bleuart.print(buffer);
    }

    va_end(args);
}

void loop()
{
    if (millis() > CycleTime)
    {
        if (Gps_Positioning_Flag == false)
        {
            Gps_Positioning_Time++;
        }

        while (Serial2.available() > 0)
        {
            int buffer = Serial2.read();

            // 将所有Serial2.read()的数据收集到一个缓冲区
            static char gps_buffer[128];
            static size_t gps_buffer_len = 0;

            // 只收集可打印字符，避免乱码
            if (buffer >= 32 && buffer <= 126 && gps_buffer_len < sizeof(gps_buffer) - 1)
            {
                gps_buffer[gps_buffer_len++] = (char)buffer;
            }

            // 如果遇到换行或缓冲区满，输出并清空
            if (buffer == '\n' || gps_buffer_len == sizeof(gps_buffer) - 1)
            {
                gps_buffer[gps_buffer_len] = '\0';
                log_printf("---gps data begin---\n");
                log_printf("%s\n", gps_buffer);
                log_printf("---gps data end---\n");
                gps_buffer_len = 0;
            }

            if (gps.encode(buffer))
            {
                display.clearDisplay();
                display.setCursor(0, 0);

                if (Gps_Positioning_Flag == false)
                {
                    display.printf("Gps n:%ds", Gps_Positioning_Time);
                    display.display();
                    log_printf("Gps n:%ds\n", Gps_Positioning_Time);
                }
                else
                {
                    display.printf("Gps y:%d s", Gps_Positioning_Time);
                    display.display();
                    log_printf("Gps y:%d s\n", Gps_Positioning_Time);
                }

                if (gps.date.isValid())
                {
                    log_printf("data: %04d/%02d/%02d\n", gps.date.year(), gps.date.month(), gps.date.day());
                }
                else
                {
                    log_printf("data: invalid\n");
                }

                if (gps.time.isValid())
                {
                    log_printf("time: %02d:%02d:%02d\n", gps.time.hour(), gps.time.minute(), gps.time.second());
                    log_printf("china time: %02d:%02d:%02d\n", (gps.time.hour() + 8 + 24) % 24, gps.time.minute(), gps.time.second());
                }
                else
                {
                    log_printf("time: invalid\n");
                }

                display.setCursor(0, 10);
                if (gps.location.isValid())
                {
                    display.printf("%.6f", gps.location.lat());
                    display.setCursor(0, 20);
                    display.printf("%.6f", gps.location.lng());
                    display.display();
                    log_printf("location lat: %.6f lng: %.6f\n", gps.location.lat(), gps.location.lng());
                    Gps_Positioning_Flag = true;
                }
                else
                {
                    display.printf("invalid");
                    display.setCursor(0, 20);
                    display.printf("invalid");
                    display.display();
                    log_printf("location: invalid\n");
                }
            }
        }

        CycleTime = millis() + 1000;
    }

    delay(10);
}
