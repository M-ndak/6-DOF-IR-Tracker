/*
 * ESP32 + BNO085 via I²C
 * Outputs: TELEMETRY,Roll,Pitch,Yaw,AccX,AccY,AccZ
 *
 * WIRING:
 *   BNO085 VDD  → 3.3V
 *   BNO085 GND  → GND
 *   BNO085 SDA  → GPIO21  (+4.7kΩ pull-up to 3.3V)
 *   BNO085 SCL  → GPIO22  (+4.7kΩ pull-up to 3.3V)
 *   BNO085 RST  → GPIO4   (or set IMU_RST to -1 if not wired)
 *   BNO085 ADR  → 3.3V    → address 0x4B
 *   BNO085 PS0  → GND     (I²C mode)
 *   BNO085 PS1  → GND     (I²C mode)
 */

#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_BNO08x_Arduino_Library.h"

// ── Hardware config ───────────────────────────────────────────────────────────
#define I2C_SDA   21
#define I2C_SCL   22
#define I2C_FREQ  400000UL
#define IMU_ADDR  0x4B       // ADR → 3V3 = 0x4B,  ADR → GND = 0x4A
#define IMU_RST   -1         // Set to GPIO number if RST is wired, else -1
#define REPORT_MS 10         // 10 ms = 100 Hz

BNO08x imu;

// ── Latest data ───────────────────────────────────────────────────────────────
float roll = 0, pitch = 0, yaw = 0;
float accX = 0, accY = 0, accZ = 0;

bool hasRotation = false;
bool hasAccel    = false;

// ── Quaternion → Euler (degrees) ─────────────────────────────────────────────
struct Euler { float roll, pitch, yaw; };

static Euler quatToEuler(float qi, float qj, float qk, float qr) {
    float n = sqrtf(qr*qr + qi*qi + qj*qj + qk*qk);
    if (n > 0.0f) { qr/=n; qi/=n; qj/=n; qk/=n; }
    float t = 2.0f*(qr*qj - qk*qi);
    t = constrain(t, -1.0f, 1.0f);
    Euler e;
    e.roll  = atan2f(2.0f*(qr*qi + qj*qk), 1.0f - 2.0f*(qi*qi + qj*qj)) * RAD_TO_DEG;
    e.pitch = asinf(t) * RAD_TO_DEG;
    e.yaw   = atan2f(2.0f*(qr*qk + qi*qj), 1.0f - 2.0f*(qj*qj + qk*qk)) * RAD_TO_DEG;
    return e;
}

// ── Enable reports ────────────────────────────────────────────────────────────
void setReports() {
    imu.enableRotationVector(REPORT_MS);   // Roll, Pitch, Yaw (fused, uses mag)
    imu.enableAccelerometer(REPORT_MS);    // AccX, AccY, AccZ (gravity included)
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);

    if (IMU_RST >= 0) {
        pinMode(IMU_RST, OUTPUT);
        digitalWrite(IMU_RST, HIGH);
    }

    Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
    Wire.setTimeOut(200);

    delay(2000); // BNO085 boot time

    if (!imu.begin(IMU_ADDR, Wire, -1, IMU_RST)) {
        Serial.println("ERROR: BNO085 not found! Check wiring.");
        while (true) delay(100);
    }

    setReports();
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
    if (!imu.getSensorEvent()) {
        delay(1);
        return;
    }

    if (imu.wasReset()) {
        setReports();
        return;
    }

    switch (imu.getSensorEventID()) {

        case SENSOR_REPORTID_ROTATION_VECTOR: {
            // Use library Euler helpers — they read from rotationVector union
            roll  = imu.getRoll()  * RAD_TO_DEG;
            pitch = imu.getPitch() * RAD_TO_DEG;
            yaw   = imu.getYaw()   * RAD_TO_DEG;
            hasRotation = true;
            break;
        }

        case SENSOR_REPORTID_ACCELEROMETER: {
            accX = imu.getAccelX();
            accY = imu.getAccelY();
            accZ = imu.getAccelZ();
            hasAccel = true;
            break;
        }
    }

    // Emit one line only when both reports have been received at least once
    if (hasRotation && hasAccel) {
        Serial.print("TELEMETRY,");
        Serial.print(roll,  2); Serial.print(",");
        Serial.print(pitch, 2); Serial.print(",");
        Serial.print(yaw,   2); Serial.print(",");
        Serial.print(accX,  3); Serial.print(",");
        Serial.print(accY,  3); Serial.print(",");
        Serial.println(accZ, 3);

        // Clear flags so we wait for a fresh pair before printing again
        hasRotation = false;
        hasAccel    = false;
    }
}