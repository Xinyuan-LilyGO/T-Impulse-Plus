#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pin_config.h"
#include "Display_Fonts.h"
#include <bluefruit.h>
#include "Adafruit_SPIFlash.h"
#include "ICM20948_WE.h"
#include "RadioLib.h"
#include "cpp_bus_driver_library.h"

enum class System_Window
{
    HOME = 0,
    FLASH_TEST,
    BATTERY_TEST,
    IMU_TEST,
    GPS_TEST,
    LORA_TEST,

    END,
};

static const uint32_t Local_MAC[2] =
    {
        NRF_FICR->DEVICEID[0],
        NRF_FICR->DEVICEID[1],
};

SPIFlash_Device_t ZD25WQ32C =
    {
        total_size : (1UL << 22), /* 4 MiB */
        start_up_time_us : 12000,
        manufacturer_id : 0xBA,
        memory_type : 0x60,
        capacity : 0x16,
        max_clock_speed_mhz : 104,
        quad_enable_bit_mask : 0x02,
        has_sector_protection : false,
        supports_fast_read : true,
        supports_qspi : true,
        supports_qspi_writes : true,
        write_status_register_split : false,
        single_status_byte : false,
        is_fram : false,
    };

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

struct System_Operator
{
    size_t time = 0;

    struct
    {
        bool screen = false;
        bool flash = false;
        bool imu = false;
        bool lora = false;

    } init_flag;
};

struct SX1262_Operator
{
    using state = enum {
        UNCONNECTED, // not connected
        CONNECTED,   // connected already
        CONNECTING,  // connecting
    };

    using mode = enum {
        LORA, // lora mode
        FSK,  // fsk mode
    };

    struct
    {
        float value = 850.0;
        bool change_flag = false;
    } frequency;
    struct
    {
        float value = 125.0;
        bool change_flag = false;
    } bandwidth;
    struct
    {
        uint8_t value = 12;
        bool change_flag = false;
    } spreading_factor;
    struct
    {
        uint8_t value = 8;
        bool change_flag = false;
    } coding_rate;
    struct
    {
        uint8_t value = 0xAB;
        bool change_flag = false;
    } sync_word;
    struct
    {
        int8_t value = 22;
        bool change_flag = false;
    } output_power;
    struct
    {
        float value = 140;
        bool change_flag = false;
    } current_limit;
    struct
    {
        int16_t value = 16;
        bool change_flag = false;
    } preamble_length;
    struct
    {
        bool value = false;
        bool change_flag = false;
    } crc;

    struct
    {

        uint32_t mac[2] = {0};
        uint32_t send_data = 0;
        uint32_t receive_data = 0;
        uint8_t connection_flag = state::UNCONNECTED;
        bool send_flag = false;
        uint8_t error_count = 11;
    } device_1;

    uint8_t current_mode = mode::LORA;

    volatile bool operation_flag = false;
    bool initialization_flag = false;
    bool mode_change_flag = false;

    uint8_t send_package[16] = {'M', 'A', 'C', ':',
                                (uint8_t)(Local_MAC[1] >> 24), (uint8_t)(Local_MAC[1] >> 16),
                                (uint8_t)(Local_MAC[1] >> 8), (uint8_t)(Local_MAC[1]),
                                (uint8_t)(Local_MAC[0] >> 24), (uint8_t)(Local_MAC[0] >> 16),
                                (uint8_t)(Local_MAC[0] >> 8), (uint8_t)Local_MAC[0],
                                0, 0, 0, 0};

    float receive_rssi = 0;
    float receive_snr = 0;
};

uint8_t Current_Window_Count = 0;
System_Window Current_Window = System_Window::HOME;

bool Battery_Control_Switch = false;

size_t CycleTime = 0;
size_t CycleTime_2 = 0;

volatile bool Ttp223_Key_Trigger_Flag = false;
volatile bool Nrf52840_Boot_Key_Trigger_Flag = false;

bool Gps_Positioning_Flag = false;
size_t Gps_Positioning_Time = 0;

/* UART Serivce: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * UART RXD    : 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
 * UART TXD    : 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
 */

// BLE Service
BLEDfu bledfu;   // OTA DFU service
BLEDis bledis;   // device information
BLEUart bleuart; // uart over ble

BLE_Uart_Operator BLE_Uart_Op;
System_Operator System_Op;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, SCREEN_RST);

// // SPI
// SPIClass Custom_SPI(NRF_SPIM3, ZD25WQ32C_MISO, ZD25WQ32C_SCLK, ZD25WQ32C_MOSI);
// Adafruit_FlashTransport_SPI flashTransport(ZD25WQ32C_CS, Custom_SPI);

// QSPI
Adafruit_FlashTransport_QSPI flashTransport(ZD25WQ32C_SCLK, ZD25WQ32C_CS,
                                            ZD25WQ32C_IO0, ZD25WQ32C_IO1,
                                            ZD25WQ32C_IO2, ZD25WQ32C_IO3);

Adafruit_SPIFlash flash(&flashTransport);

/* There are several ways to create your ICM20948 object:
 * ICM20948_WE myIMU = ICM20948_WE()              -> uses Wire / I2C Address = 0x68
 * ICM20948_WE myIMU = ICM20948_WE(ICM20948_ADDR) -> uses Wire / ICM20948_ADDR
 * ICM20948_WE myIMU = ICM20948_WE(&wire2)        -> uses the TwoWire object wire2 / ICM20948_ADDR
 * ICM20948_WE myIMU = ICM20948_WE(&wire2, ICM20948_ADDR) -> all together
 * ICM20948_WE myIMU = ICM20948_WE(CS_PIN, spi);  -> uses SPI, spi is just a flag, see SPI example
 * ICM20948_WE myIMU = ICM20948_WE(&SPI, CS_PIN, spi);  -> uses SPI / passes the SPI object, spi is just a flag, see SPI example
 */
ICM20948_WE myIMU = ICM20948_WE(&Wire, ICM20948_ADDRESS);

SX1262_Operator SX1262_OP;

SPIClass Custom_SPI_3(NRF_SPIM3, SX1262_MISO, SX1262_SCLK, SX1262_MOSI);
SX1262 radio = new Module(SX1262_CS, SX1262_DIO1, SX1262_RST, SX1262_BUSY, Custom_SPI_3);

auto Nrf52840_Gnss = std::make_shared<Cpp_Bus_Driver::Gnss>();

void log_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    Serial.print(buffer);
    if ((BLE_Uart_Op.initialization_flag == true) && (BLE_Uart_Op.connection.state_flag == BLE_Uart_Op.state::CONNECTED))
    {
        bleuart.print(buffer);
    }

    va_end(args);
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
    // Get the reference to current connection
    BLEConnection *connection = Bluefruit.Connection(conn_handle);

    char central_name[32] = {0};
    connection->getPeerName(central_name, sizeof(central_name));

    Serial.print("Connected to ");
    Serial.println(central_name);

    strcpy(BLE_Uart_Op.connection.device_name, central_name);

    BLE_Uart_Op.connection.state_flag = BLE_Uart_Op.state::CONNECTED;
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
    BLE_Uart_Op.connection.state_flag = BLE_Uart_Op.state::UNCONNECTED;
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

bool SX1262_Set_Default_Parameters(String *assertion)
{
    if (radio.setFrequency(SX1262_OP.frequency.value) == RADIOLIB_ERR_INVALID_FREQUENCY)
    {
        *assertion = "Failed to set frequency value";
        return false;
    }
    if (radio.setBandwidth(SX1262_OP.bandwidth.value) == RADIOLIB_ERR_INVALID_BANDWIDTH)
    {
        *assertion = "Failed to set bandwidth value";
        return false;
    }
    if (radio.setOutputPower(SX1262_OP.output_power.value) == RADIOLIB_ERR_INVALID_OUTPUT_POWER)
    {
        *assertion = "Failed to set output_power value";
        return false;
    }
    if (radio.setCurrentLimit(SX1262_OP.current_limit.value) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT)
    {
        *assertion = "Failed to set current_limit value";
        return false;
    }
    if (radio.setPreambleLength(SX1262_OP.preamble_length.value) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH)
    {
        *assertion = "Failed to set preamble_length value";
        return false;
    }
    if (radio.setCRC(SX1262_OP.crc.value) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION)
    {
        *assertion = "Failed to set crc value";
        return false;
    }
    if (SX1262_OP.current_mode == SX1262_OP.mode::LORA)
    {
        if (radio.setSpreadingFactor(SX1262_OP.spreading_factor.value) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR)
        {
            *assertion = "Failed to set spreading_factor value";
            return false;
        }
        if (radio.setCodingRate(SX1262_OP.coding_rate.value) == RADIOLIB_ERR_INVALID_CODING_RATE)
        {
            *assertion = "Failed to set coding_rate value";
            return false;
        }
        if (radio.setSyncWord(SX1262_OP.sync_word.value) != RADIOLIB_ERR_NONE)
        {
            *assertion = "Failed to set sync_word value";
            return false;
        }
    }
    else
    {
    }
    return true;
}

void Window_Init(System_Window Window)
{
    switch (Window)
    {
    case System_Window::FLASH_TEST:
        // Custom_SPI.setClockDivider(SPI_CLOCK_DIV2); // dual frequency 32MHz

        flash.begin();
        flashTransport.setClockSpeed(32000000UL, 0);
        flashTransport.runCommand(0xAB); // Exit deep sleep mode

        if (flash.begin(&ZD25WQ32C) == false)
        {
            log_printf("flash init fail\n");
            System_Op.init_flag.flash = false;
        }
        else
        {
            log_printf("flash init successful\n");
            System_Op.init_flag.flash = true;
        }

        break;

    case System_Window::BATTERY_TEST:
        attachInterrupt(
            nRF52840_BOOT,
            []
            {
                Nrf52840_Boot_Key_Trigger_Flag = true;
            },
            FALLING);

        // Measure battery
        pinMode(BATTERY_ADC_DATA, INPUT);
        pinMode(BATTERY_MEASUREMENT_CONTROL, OUTPUT);
        digitalWrite(BATTERY_MEASUREMENT_CONTROL, HIGH); // Turn on battery voltage measurement

        Battery_Control_Switch = true;

        // Set the analog reference to 3.0V (default = 3.6V)
        analogReference(AR_INTERNAL_3_0);
        // Set the resolution to 12-bit (0..4095)
        analogReadResolution(12); // Can be 8, 10, 12 or 14

        break;

    case System_Window::IMU_TEST:
        Wire.setPins(ICM20948_SDA, ICM20948_SCL);
        Wire.begin();

        if (myIMU.init() == false)
        {
            log_printf("ICM20948 AG init fail\n");
            System_Op.init_flag.imu = false;
        }
        else
        {
            myIMU.sleep(false);
            if (myIMU.initMagnetometer() == false)
            {
                log_printf("ICM20948 M init fail\n");
                System_Op.init_flag.imu = false;
            }

            log_printf("ICM20948 init successful\n");
            System_Op.init_flag.imu = true;

            /*  This is a method to calibrate. You have to determine the minimum and maximum
             *  raw acceleration values of the axes determined in the range +/- 2 g.
             *  You call the function as follows: setAccOffsets(xMin,xMax,yMin,yMax,zMin,zMax);
             *  The parameters are floats.
             *  The calibration changes the slope / ratio of raw acceleration vs g. The zero point
             *  is set as (min + max)/2.
             */
            // myIMU.setAccOffsets(-16330.0, 16450.0, -16600.0, 16180.0, -16520.0, 16690.0);

            /* The starting point, if you position the ICM20948 flat, is not necessarily 0g/0g/1g for x/y/z.
             * The autoOffset function measures offset. It assumes your ICM20948 is positioned flat with its
             * x,y-plane. The more you deviate from this, the less accurate will be your results.
             * It overwrites the zero points of setAccOffsets, but keeps the correction of the slope.
             * The function also measures the offset of the gyroscope data. The gyroscope offset does not
             * depend on the positioning.
             * This function needs to be called after setAccsOffsets but before other settings since it will
             * overwrite settings!
             * You can query the offsets with the functions:
             * xyzFloat getAccOffsets() and xyzFloat getGyrOffsets()
             * You can apply the offsets using:
             * setAccOffsets(xyzFloat yourOffsets) and setGyrOffsets(xyzFloat yourOffsets)
             */
            log_printf("position your icm20948 flat and don't move it - calibrating...\n");
            delay(1000);
            myIMU.autoOffsets();
            log_printf("done!\n");

            /* enables or disables the acceleration sensor, default: enabled */
            // myIMU.enableAcc(true);

            /*  ICM20948_ACC_RANGE_2G      2 g   (default)
             *  ICM20948_ACC_RANGE_4G      4 g
             *  ICM20948_ACC_RANGE_8G      8 g
             *  ICM20948_ACC_RANGE_16G    16 g
             */
            myIMU.setAccRange(ICM20948_ACC_RANGE_2G);

            /*  Choose a level for the Digital Low Pass Filter or switch it off.
             *  ICM20948_DLPF_0, ICM20948_DLPF_2, ...... ICM20948_DLPF_7, ICM20948_DLPF_OFF
             *
             *  IMPORTANT: This needs to be ICM20948_DLPF_7 if DLPF is used in cycle mode!
             *
             *  DLPF       3dB Bandwidth [Hz]      Output Rate [Hz]
             *    0              246.0               1125/(1+ASRD)
             *    1              246.0               1125/(1+ASRD)
             *    2              111.4               1125/(1+ASRD)
             *    3               50.4               1125/(1+ASRD)
             *    4               23.9               1125/(1+ASRD)
             *    5               11.5               1125/(1+ASRD)
             *    6                5.7               1125/(1+ASRD)
             *    7              473.0               1125/(1+ASRD)
             *    OFF           1209.0               4500
             *
             *    ASRD = Accelerometer Sample Rate Divider (0...4095)
             *    You achieve lowest noise using level 6
             */
            myIMU.setAccDLPF(ICM20948_DLPF_6);

            /*  Acceleration sample rate divider divides the output rate of the accelerometer.
             *  Sample rate = Basic sample rate / (1 + divider)
             *  It can only be applied if the corresponding DLPF is not off!
             *  Divider is a number 0...4095 (different range compared to gyroscope)
             *  If sample rates are set for the accelerometer and the gyroscope, the gyroscope
             *  sample rate has priority.
             */
            // myIMU.setAccSampleRateDivider(10);

            /* You can set the following modes for the magnetometer:
             * AK09916_PWR_DOWN          Power down to save energy
             * AK09916_TRIGGER_MODE      Measurements on request, a measurement is triggered by
             *                           calling setMagOpMode(AK09916_TRIGGER_MODE)
             * AK09916_CONT_MODE_10HZ    Continuous measurements, 10 Hz rate
             * AK09916_CONT_MODE_20HZ    Continuous measurements, 20 Hz rate
             * AK09916_CONT_MODE_50HZ    Continuous measurements, 50 Hz rate
             * AK09916_CONT_MODE_100HZ   Continuous measurements, 100 Hz rate (default)
             */
            myIMU.setMagOpMode(AK09916_CONT_MODE_20HZ);
        }

        break;

    case System_Window::GPS_TEST:
        Serial2.setPins(GPS_UART_TX, GPS_UART_RX);
        Serial2.begin(38400);

        Gps_Positioning_Flag = false;
        Gps_Positioning_Time = 0;

        break;
    case System_Window::LORA_TEST:
    {
        attachInterrupt(
            nRF52840_BOOT,
            []
            {
                Nrf52840_Boot_Key_Trigger_Flag = true;
            },
            FALLING);

        Custom_SPI_3.begin();
        Custom_SPI_3.setClockDivider(SPI_CLOCK_DIV2);

        int16_t state = radio.begin();
        if (state != RADIOLIB_ERR_NONE)
        {
            log_printf("sx1262 init fail\n");
            log_printf("error code: %d\n", state);

            System_Op.init_flag.lora = false;
        }
        else
        {
            log_printf("sx1262 init successful\n");

            radio.setRfSwitchPins(SX1262_RF_VC2, SX1262_RF_VC1);

            radio.setDio1Action([]()
                                { SX1262_OP.operation_flag = true; });

            String buffer_str;
            if (SX1262_Set_Default_Parameters(&buffer_str) == false)
            {
                log_printf("sx1262 failed to set default parameters\n");
                log_printf("sx1262 assertion: %s\n", buffer_str.c_str());
                System_Op.init_flag.lora = false;
            }

            if (radio.startReceive() != RADIOLIB_ERR_NONE)
            {
                log_printf("sx1262 failed to start receive\n");
            }

            display.clearDisplay();
            display.setCursor(0, 0);
            display.printf("Lora Y UNC");
            display.display();

            System_Op.init_flag.lora = true;
        }

        break;
    }
    default:
        break;
    }
}

void Window_End(System_Window Window)
{
    switch (Window)
    {
    case System_Window::HOME:
        CycleTime = 0;
        break;
    case System_Window::FLASH_TEST:
        flashTransport.runCommand(0xB9); // Flash Deep Sleep
        flash.end();
        pinMode(ZD25WQ32C_SCLK, INPUT);
        pinMode(ZD25WQ32C_CS, INPUT);
        pinMode(ZD25WQ32C_IO0, INPUT);
        pinMode(ZD25WQ32C_IO1, INPUT);
        pinMode(ZD25WQ32C_IO2, INPUT);
        pinMode(ZD25WQ32C_IO3, INPUT);
        CycleTime = 0;
        break;
    case System_Window::BATTERY_TEST:
        detachInterrupt(nRF52840_BOOT);
        digitalWrite(BATTERY_MEASUREMENT_CONTROL, LOW); // Turn off battery voltage measurement
        CycleTime = 0;
        break;
    case System_Window::IMU_TEST:
        myIMU.sleep(true);
        Wire.end();
        pinMode(ICM20948_SDA, INPUT);
        pinMode(ICM20948_SCL, INPUT);
        CycleTime = 0;
        break;
    case System_Window::GPS_TEST:
        Serial2.end();
        CycleTime = 0;
        break;
    case System_Window::LORA_TEST:
        detachInterrupt(nRF52840_BOOT);
        radio.sleep();
        Custom_SPI_3.end();
        pinMode(SX1262_MISO, INPUT);
        pinMode(SX1262_MOSI, INPUT);
        pinMode(SX1262_SCLK, INPUT);
        pinMode(SX1262_CS, INPUT);
        pinMode(SX1262_DIO1, INPUT);
        pinMode(SX1262_RST, INPUT);
        pinMode(SX1262_BUSY, INPUT);
        CycleTime = 0;
        break;

    default:
        break;
    }
}

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

    pinMode(TTP223_EN, OUTPUT);
    digitalWrite(TTP223_EN, HIGH);

    pinMode(GPS_1PPS, INPUT);

    attachInterrupt(
        TTP223_KEY,
        []
        {
            Ttp223_Key_Trigger_Flag = true;
        },
        RISING);

    if (BLE_Uart_Initialization() == true)
    {
        BLE_Uart_Op.initialization_flag = true;
        Serial.println("BLE_Uart_Initialization successful");
    }
    else
    {
        BLE_Uart_Op.initialization_flag = false;
        Serial.println("BLE_Uart_Initialization fail");
    }

    Wire1.setPins(SCREEN_SDA, SCREEN_SCL);
    Wire1.begin();

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS) == false)
    {
        Serial.println("SSD1306 init fail");
        System_Op.init_flag.screen = false;
    }
    else
    {
        Serial.println("SSD1306 init successful");
        System_Op.init_flag.screen = true;
    }

    display.setOffsetCursor(32, 32);
    display.clearDisplay();
    display.setTextColor(WHITE);

    display.display();

    Window_Init(Current_Window);
}

void loop()
{
    if (Ttp223_Key_Trigger_Flag == true)
    {
        delay(100);

        log_printf("TTP223_KEY trigger\n");

        Window_End(Current_Window);

        Current_Window_Count++;
        if (Current_Window_Count >= (uint8_t)System_Window::END)
        {
            Current_Window_Count = 0;
        }

        Current_Window = (System_Window)Current_Window_Count;

        Window_Init(Current_Window);

        Ttp223_Key_Trigger_Flag = false;
    }

    switch (Current_Window)
    {
    case System_Window::HOME:
    {
        if (millis() > CycleTime)
        {
            System_Op.time = millis();

            display.clearDisplay();
            display.setCursor(17, 0);
            display.printf("T-I-P");
            log_printf("T-Impulse-Plus\n");

            display.setCursor(8, 15);
            uint32_t total_seconds = System_Op.time / 1000;
            uint8_t hours = total_seconds / 3600;
            uint8_t minutes = (total_seconds % 3600) / 60;
            uint8_t seconds = total_seconds % 60;
            display.printf("%02d:%02d:%02d", hours, minutes, seconds);
            log_printf("system time: %02d:%02d:%02d\n", hours, minutes, seconds);

            display.display();

            CycleTime = millis() + 1000;
        }
        break;
    }
    case System_Window::FLASH_TEST:
        if (millis() > CycleTime)
        {
            display.clearDisplay();
            display.setCursor(0, 0);

            if (System_Op.init_flag.flash == true)
            {
                display.printf("Flash Y");
                display.setCursor(0, 10);
                display.printf("%#X", flash.getJEDECID());
                display.setCursor(0, 20);
                display.printf("%dkbytes", flash.size() / 1024);

                log_printf("flash init successful\n");
                log_printf("id: %#X\n", flash.getJEDECID());
                log_printf("size: %d kbytes\n", flash.size() / 1024);
            }
            else
            {
                display.printf("Flash N");

                log_printf("flash init fail\n");
            }

            display.display();

            CycleTime = millis() + 1000;
        }
        break;

    case System_Window::BATTERY_TEST:
        if (Nrf52840_Boot_Key_Trigger_Flag == true)
        {
            delay(300);

            log_printf("nRF52840_BOOT trigger\n");

            Battery_Control_Switch = !Battery_Control_Switch;
            if (Battery_Control_Switch == true)
            {
                digitalWrite(BATTERY_MEASUREMENT_CONTROL, HIGH); // Enable battery voltage measurement
                log_printf("turn on battery voltage measurement\n");
            }
            else
            {
                digitalWrite(BATTERY_MEASUREMENT_CONTROL, LOW); // Turn off battery voltage measurement
                log_printf("turn off battery voltage measurement\n");
            }

            Nrf52840_Boot_Key_Trigger_Flag = false;
        }

        if (millis() > CycleTime)
        {
            float battery_voltage = (((float)analogRead(BATTERY_ADC_DATA) * ((3000.0 / 4096.0))) / 1000.0) * 2.0;

            display.clearDisplay();
            display.setCursor(0, 0);
            display.printf("Battery");
            display.setCursor(0, 10);
            if (Battery_Control_Switch == true)
            {
                display.printf("Switch:ON");
                log_printf("battery switch: on\n");
            }
            else
            {
                display.printf("Switch:OFF");
                log_printf("battery switch: off\n");
            }
            display.setCursor(0, 20);
            display.printf("%.03fv", battery_voltage);

            display.display();

            log_printf("battery voltage: %.03f v\n", battery_voltage);
            CycleTime = millis() + 1000;
        }
        break;
    case System_Window::IMU_TEST:
        if (millis() > CycleTime)
        {
            if (System_Op.init_flag.imu == true)
            {
                myIMU.readSensor();
                xyzFloat gValue = myIMU.getGValues();
                xyzFloat angle = myIMU.getAngles();
                float pitch = myIMU.getPitch();
                float roll = myIMU.getRoll();

                // obtain the x and y values of the magnetometer to calculate the heading angle (Yaw)
                xyzFloat magValues = myIMU.getMagValues();
                float yaw = atan2(magValues.y, magValues.x) * (180.0 / M_PI); // calculate heading angle

                display.clearDisplay();
                display.setCursor(0, 0);
                display.printf("Imu Y p:%.01f", pitch);
                display.setCursor(0, 10);
                display.printf("r:%.01f", roll);
                display.setCursor(0, 20);
                display.printf("y:%.01f", yaw);

                display.display();

                log_printf("imu init successful\n");
                log_printf("pitch = %.6f  |  roll = %.6f  |  yaw = %.6f\n", pitch, roll, yaw);

                CycleTime = millis() + 500;
            }
            else
            {
                display.printf("Imu N");

                display.display();

                log_printf("imu init fail\n");

                CycleTime = millis() + 1000;
            }
        }
        break;

    case System_Window::GPS_TEST:
        if (millis() > CycleTime)
        {
            if (Gps_Positioning_Flag == false)
            {
                Gps_Positioning_Time++;
            }

            // 检查Serial2是否有可用数据
            if (Serial2.available())
            {
                // 读取Serial2所有可用数据到缓冲区
                const size_t bufferSize = 512;

                std::unique_ptr<uint8_t[]> buffer(new uint8_t[bufferSize]);
                size_t bytesRead = 0;

                while (Serial2.available() && bytesRead < bufferSize)
                {
                    buffer[bytesRead++] = Serial2.read();
                }

                buffer[bytesRead] = '\0';

                // 打印RMC的相关信息
                log_printf("---begin---\n%s \n---end---\n", buffer.get());

                log_printf("---RMC---\n");

                // 创建Rmc对象用于存储解析结果
                Cpp_Bus_Driver::Gnss::Rmc rmc;

                display.clearDisplay();
                display.setCursor(0, 0);

                if (Gps_Positioning_Flag == false)
                {
                    display.printf("Gps N:%ds", Gps_Positioning_Time);
                    log_printf("Gps N:%ds\n", Gps_Positioning_Time);
                }
                else
                {
                    display.printf("Gps Y:%d s", Gps_Positioning_Time);
                    log_printf("Gps Y:%d s\n", Gps_Positioning_Time);
                }

                // 调用parse_rmc_info进行解码
                if (Nrf52840_Gnss->parse_rmc_info(buffer.get(), bytesRead, rmc) == true)
                {
                    log_printf("location status: %s\n", (rmc.location_status).c_str());

                    if (rmc.data.update_flag == true)
                    {
                        log_printf("utc data: %d/%d/%d\n", rmc.data.year + 2000, rmc.data.month, rmc.data.day);
                        rmc.data.update_flag = false;
                    }
                    if (rmc.utc.update_flag == true)
                    {
                        log_printf("utc time: %d:%d:%.02f\n", rmc.utc.hour, rmc.utc.minute, rmc.utc.second);
                        log_printf("china time: %d:%d:%.02f\n", (rmc.utc.hour + 8 + 24) % 24, rmc.utc.minute, rmc.utc.second);
                        rmc.utc.update_flag = false;
                    }

                    if ((rmc.location.lat.update_flag == true) && (rmc.location.lat.direction_update_flag == true))
                    {
                        log_printf("location lat degrees: %d \nlocation lat minutes: %.10lf \nlocation lat degrees_minutes: %.10lf \nlocation lat direction: %s\n",
                                   rmc.location.lat.degrees, rmc.location.lat.minutes, rmc.location.lat.degrees_minutes, (rmc.location.lat.direction).c_str());
                        rmc.location.lat.update_flag = false;
                        rmc.location.lat.direction_update_flag = false;

                        display.setCursor(0, 10);
                        display.printf("%.6f", rmc.location.lat.degrees_minutes);

                        Gps_Positioning_Flag = true;
                    }
                    if ((rmc.location.lon.update_flag == true) && (rmc.location.lon.direction_update_flag == true))
                    {
                        log_printf("location lon degrees: %d \nlocation lon minutes: %.10lf \nlocation lon degrees_minutes: %.10lf \nlocation lon direction: %s\n",
                                   rmc.location.lon.degrees, rmc.location.lon.minutes, rmc.location.lon.degrees_minutes, (rmc.location.lon.direction).c_str());
                        rmc.location.lon.update_flag = false;
                        rmc.location.lon.direction_update_flag = false;

                        display.setCursor(0, 20);
                        display.printf("%.6f", rmc.location.lon.degrees_minutes);

                        Gps_Positioning_Flag = true;
                    }

                    display.display();
                }
                else
                {
                    display.printf("invalid");
                    display.setCursor(0, 20);
                    display.printf("invalid");
                    display.display();

                    log_printf("gps data: read fail\n");
                }
            }

            CycleTime = millis() + 1000;
        }

        break;

    case System_Window::LORA_TEST:
        if (System_Op.init_flag.lora == true)
        {
            if (Nrf52840_Boot_Key_Trigger_Flag == true)
            {
                delay(300);

                log_printf("nRF52840_BOOT trigger\n");

                SX1262_OP.device_1.send_flag = true;
                SX1262_OP.device_1.connection_flag = SX1262_OP.state::CONNECTING;
                // clear error count watchdog
                SX1262_OP.device_1.error_count = 0;
                SX1262_OP.device_1.send_data = 0;

                CycleTime = 0;

                Nrf52840_Boot_Key_Trigger_Flag = false;
            }

            if (SX1262_OP.device_1.send_flag == true)
            {
                if (millis() > CycleTime)
                {
                    SX1262_OP.send_package[12] = (uint8_t)(SX1262_OP.device_1.send_data >> 24);
                    SX1262_OP.send_package[13] = (uint8_t)(SX1262_OP.device_1.send_data >> 16);
                    SX1262_OP.send_package[14] = (uint8_t)(SX1262_OP.device_1.send_data >> 8);
                    SX1262_OP.send_package[15] = (uint8_t)SX1262_OP.device_1.send_data;

                    display.clearDisplay();
                    display.setCursor(0, 0);
                    display.printf("Lora Y TX");
                    display.setCursor(0, 15);
                    display.printf("tx:%u", SX1262_OP.device_1.send_data);
                    display.display();

                    // send another one
                    log_printf("[SX1262] send packet\n");
                    log_printf("[SX1262] send: %u\n", SX1262_OP.device_1.send_data);
                    log_printf("local_mac[0]: %#010X, local_mac[1]: %#010X\n", Local_MAC[0], Local_MAC[1]);

                    radio.transmit(SX1262_OP.send_package, 16);
                    radio.startReceive();

                    SX1262_OP.device_1.send_flag = false;
                }
            }

            if (SX1262_OP.operation_flag == true)
            {
                uint8_t receive_package[16] = {'\0'};
                if (radio.readData(receive_package, 16) == RADIOLIB_ERR_NONE)
                {
                    if ((receive_package[0] == 'M') &&
                        (receive_package[1] == 'A') &&
                        (receive_package[2] == 'C') &&
                        (receive_package[3] == ':'))
                    {
                        uint32_t temp_mac[2];
                        temp_mac[0] =
                            ((uint32_t)receive_package[8] << 24) |
                            ((uint32_t)receive_package[9] << 16) |
                            ((uint32_t)receive_package[10] << 8) |
                            (uint32_t)receive_package[11];
                        temp_mac[1] =
                            ((uint32_t)receive_package[4] << 24) |
                            ((uint32_t)receive_package[5] << 16) |
                            ((uint32_t)receive_package[6] << 8) |
                            (uint32_t)receive_package[7];

                        if ((temp_mac[0] != Local_MAC[0]) && (temp_mac[1] != Local_MAC[1]))
                        {
                            SX1262_OP.device_1.mac[0] = temp_mac[0];
                            SX1262_OP.device_1.mac[1] = temp_mac[1];
                            SX1262_OP.device_1.receive_data =
                                ((uint32_t)receive_package[12] << 24) |
                                ((uint32_t)receive_package[13] << 16) |
                                ((uint32_t)receive_package[14] << 8) |
                                (uint32_t)receive_package[15];

                            // packet was successfully received
                            log_printf("[SX1262] received packet\n");

                            // print data of the packet
                            // for (int i = 0; i < 16; i++)
                            // {
                            //     Serial.printf("[SX1262] Data[%d]: %#X\n", i, receive_package[i]);
                            // }
                            log_printf("[SX1262] receive_mac[0]: %#010X, receive_mac[1]: %#010X\n", temp_mac[0], temp_mac[1]);
                            log_printf("[SX1262] receive_data: %u\n", SX1262_OP.device_1.receive_data);

                            // print RSSI (Received Signal Strength Indicator)
                            SX1262_OP.receive_rssi = radio.getRSSI();
                            log_printf("[SX1262] rssi: %.1f dbm\n", SX1262_OP.receive_rssi);

                            // print SNR (Signal-to-Noise Ratio)
                            SX1262_OP.receive_snr = radio.getSNR();
                            log_printf("[SX1262] snr: %.1f db\n", SX1262_OP.receive_snr);

                            display.clearDisplay();
                            display.setCursor(0, 0);
                            display.printf("Lora Y RX");
                            display.setCursor(0, 10);
                            display.printf("rx:%d", SX1262_OP.device_1.receive_data);
                            display.setCursor(0, 20);
                            display.printf("rssi:%.1fdbm", SX1262_OP.receive_rssi);
                            display.display();

                            SX1262_OP.device_1.send_data = SX1262_OP.device_1.receive_data + 1;

                            SX1262_OP.device_1.send_flag = true;
                            SX1262_OP.device_1.connection_flag = SX1262_OP.state::CONNECTED;

                            // clear error count watchdog
                            SX1262_OP.device_1.error_count = 0;
                            CycleTime = millis() + 3000;
                        }
                    }
                }

                SX1262_OP.operation_flag = false;
            }

            if (millis() > CycleTime_2)
            {
                SX1262_OP.device_1.error_count++;
                if (SX1262_OP.device_1.error_count > 10)
                {
                    SX1262_OP.device_1.error_count = 11;
                    SX1262_OP.device_1.send_data = 0;
                    SX1262_OP.device_1.connection_flag = SX1262_OP.state::UNCONNECTED;

                    display.clearDisplay();
                    display.setCursor(0, 0);
                    display.printf("Lora Y UNC");
                    display.display();
                }
                CycleTime_2 = millis() + 2000;
            }
        }
        else
        {
            if (millis() > CycleTime)
            {
                display.clearDisplay();
                display.setCursor(0, 0);
                display.printf("Lora N");
                display.display();

                log_printf("Lora init fail\n");
                CycleTime = millis() + 1000;
            }
        }

        break;

    default:
        break;
    }
}
