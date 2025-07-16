/*
 * @Description: ICM20948 Sensor Test
 * @Author: LILYGO_L
 * @Date: 2024-11-07 12:04:52
 * @LastEditTime: 2025-06-05 14:57:14
 * @License: GPL 3.0
 */

#include <Wire.h>
#include "ICM20948_WE.h"
#include <Adafruit_TinyUSB.h>
#include "pin_config.h"

/* There are several ways to create your ICM20948 object:
 * ICM20948_WE myIMU = ICM20948_WE()              -> uses Wire / I2C Address = 0x68
 * ICM20948_WE myIMU = ICM20948_WE(ICM20948_ADDR) -> uses Wire / ICM20948_ADDR
 * ICM20948_WE myIMU = ICM20948_WE(&wire2)        -> uses the TwoWire object wire2 / ICM20948_ADDR
 * ICM20948_WE myIMU = ICM20948_WE(&wire2, ICM20948_ADDR) -> all together
 * ICM20948_WE myIMU = ICM20948_WE(CS_PIN, spi);  -> uses SPI, spi is just a flag, see SPI example
 * ICM20948_WE myIMU = ICM20948_WE(&SPI, CS_PIN, spi);  -> uses SPI / passes the SPI object, spi is just a flag, see SPI example
 */
ICM20948_WE myIMU = ICM20948_WE(ICM20948_ADDRESS);

void setup()
{
    Serial.begin(115200);
    Serial.println("Ciallo");

    // 3.3V Power ON
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);

    Wire.setPins(ICM20948_SDA, ICM20948_SCL);
    Wire.begin();

    while (myIMU.init() == false)
    {
        Serial.println("ICM20948 AG initialization failed");
        delay(1000);
    }

    while (myIMU.initMagnetometer() == false)
    {
        Serial.println("ICM20948 M initialization failed");
        delay(1000);
    }

    Serial.println("ICM20948 initialization successful");

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
    Serial.println("Position your ICM20948 flat and don't move it - calibrating...");
    delay(1000);
    myIMU.autoOffsets();
    Serial.println("Done!");

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

void loop()
{
    myIMU.readSensor();
    xyzFloat gValue = myIMU.getGValues();
    xyzFloat angle = myIMU.getAngles();
    float pitch = myIMU.getPitch();
    float roll = myIMU.getRoll();

    // obtain the x and y values of the magnetometer to calculate the heading angle (Yaw)
    xyzFloat magValues = myIMU.getMagValues();
    float yaw = atan2(magValues.y, magValues.x) * (180.0 / M_PI); // calculate heading angle

    Serial.println("G values (x,y,z):");
    Serial.print(gValue.x);
    Serial.print("   ");
    Serial.print(gValue.y);
    Serial.print("   ");
    Serial.println(gValue.z);
    Serial.println("Angles (x,y,z):");
    Serial.print(angle.x);
    Serial.print("   ");
    Serial.print(angle.y);
    Serial.print("   ");
    Serial.println(angle.z);

    Serial.print("Pitch = ");
    Serial.print(pitch);
    Serial.print("  |  Roll = ");
    Serial.print(roll);
    Serial.print("  |  Yaw = ");
    Serial.println(yaw);

    Serial.println();
    Serial.println();

    delay(100);
}
