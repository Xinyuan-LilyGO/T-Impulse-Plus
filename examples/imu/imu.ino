/*
 * @Description: T-Impulse Plus dual-version IMU test
 * @Author: LILYGO_L
 * @Date: 2026-10-30 00:00:00
 * @LastEditTime: 2026-10-30 11:26:00
 * @License: GPL 3.0
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "ICM20948_WE.h"
#include "SensorQMI8658.hpp"
#include "SensorQMC6309.hpp"
#include "pin_config.h"

ICM20948_WE icm20948(ICM20948_ADDRESS);
SensorQMI8658 qmi8658;
SensorQMC6309 qmc6309;

enum class ImuKind { kNone, kIcm20948, kQmi8658Qmc6309 };
ImuKind imu_kind = ImuKind::kNone;

bool InitQmi8658Qmc6309()
{
    if (!qmi8658.begin(Wire, QMI8658_ADDRESS, QMI8658_SDA, QMI8658_SCL) ||
        !qmc6309.begin(Wire, QMC6309_ADDRESS, QMC6309_SDA, QMC6309_SCL)) {
        return false;
    }
    if (!qmi8658.configAccelerometer(SensorQMI8658::ACC_RANGE_2G,
                                     SensorQMI8658::ACC_ODR_125Hz,
                                     SensorQMI8658::LPF_MODE_3) ||
        !qmi8658.configGyroscope(SensorQMI8658::GYR_RANGE_256DPS,
                                  SensorQMI8658::GYR_ODR_112_1Hz,
                                  SensorQMI8658::LPF_MODE_3) ||
        !qmi8658.disableSyncSampleMode() ||
        !qmc6309.setOperationMode(OperationMode::CONTINUOUS_MEASUREMENT) ||
        !qmc6309.setFullScaleRange(MagFullScaleRange::FS_32G) ||
        !qmc6309.setOversamplingRate(MagOverSampleRatio::OSR_8) ||
        !qmc6309.setLowPassFilter(MagLowPassFilter::LPF_8) ||
        !qmc6309.setSetResetMode(SensorQMC6309::MagSetResetMode::SET_AND_RESET_ON)) {
        return false;
    }
    qmi8658.enableAccelerometer();
    qmi8658.enableGyroscope();
    Serial.printf("IMU selected: QMI8658 + QMC6309 (QMI address 0x%02X)\n", QMI8658_ADDRESS);
    return true;
}

bool InitImu()
{
    pinMode(RT9080_EN, OUTPUT);
    digitalWrite(RT9080_EN, HIGH);
    Wire.setPins(ICM20948_SDA, ICM20948_SCL);
    Wire.begin();

    if (icm20948.init() && icm20948.initMagnetometer()) {
        icm20948.sleep(false);
        icm20948.setAccRange(ICM20948_ACC_RANGE_2G);
        icm20948.setAccDLPF(ICM20948_DLPF_6);
        icm20948.setMagOpMode(AK09916_CONT_MODE_20HZ);
        imu_kind = ImuKind::kIcm20948;
        Serial.println("IMU selected: ICM20948");
        return true;
    }
    if (InitQmi8658Qmc6309()) {
        imu_kind = ImuKind::kQmi8658Qmc6309;
        return true;
    }
    return false;
}

void PrintIcm20948()
{
    icm20948.readSensor();
    const xyzFloat acceleration = icm20948.getGValues();
    const xyzFloat angles = icm20948.getAngles();
    const xyzFloat angular_velocity = icm20948.getGyrValues();
    const xyzFloat magnetic = icm20948.getMagValues();
    const float yaw = atan2f(magnetic.y, magnetic.x) * 180.0f / PI;
    Serial.printf("ACC (g): %.3f %.3f %.3f\n", acceleration.x, acceleration.y, acceleration.z);
    Serial.printf("ANGLE (deg): %.2f %.2f %.2f\n", angles.x, angles.y, angles.z);
    Serial.printf("GYR (dps): %.3f %.3f %.3f\n", angular_velocity.x, angular_velocity.y, angular_velocity.z);
    Serial.printf("MAG (uT): %.3f %.3f %.3f, YAW %.2f\n", magnetic.x, magnetic.y, magnetic.z, yaw);
}

void PrintQmi8658Qmc6309()
{
    if (qmi8658.getDataReady()) {
        IMUdata acceleration = {};
        IMUdata angular_velocity = {};
        if (qmi8658.getAccelerometer(acceleration.x, acceleration.y, acceleration.z) &&
            qmi8658.getGyroscope(angular_velocity.x, angular_velocity.y, angular_velocity.z)) {
            Serial.printf("ACC (m/s2): %.3f %.3f %.3f\n", acceleration.x, acceleration.y, acceleration.z);
            Serial.printf("GYR (dps): %.3f %.3f %.3f\n", angular_velocity.x, angular_velocity.y, angular_velocity.z);
        }
    }
    MagnetometerData magnetic = {};
    if (qmc6309.readData(magnetic)) {
        Serial.printf("MAG (uT): %.3f %.3f %.3f%s\n",
                      magnetic.magnetic_field.x * 100.0f,
                      magnetic.magnetic_field.y * 100.0f,
                      magnetic.magnetic_field.z * 100.0f,
                      magnetic.overflow ? " (overflow)" : "");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(20);
    if (!InitImu()) {
        Serial.println("No supported IMU found");
        while (true) {
            delay(1000);
        }
    }
}

void loop()
{
    if (imu_kind == ImuKind::kIcm20948) {
        PrintIcm20948();
    } else if (imu_kind == ImuKind::kQmi8658Qmc6309) {
        PrintQmi8658Qmc6309();
    }
    delay(100);
}
