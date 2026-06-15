/*
 * MPU6050 CALIBRATION CODE
 * =========================
 * 1. Upload this code
 * 2. Keep MPU6050 FLAT and STILL on a level surface
 * 3. Open Serial Monitor at 115200 baud
 * 4. Wait for "CALIBRATION DONE" message
 * 5. Note down the 4 values shown
 */

#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // SDA=G21, SCL=G22
  mpu.initialize();

  Serial.println("==============================");
  Serial.println("   MPU6050 CALIBRATION TOOL   ");
  Serial.println("==============================");
  Serial.println("Keep MPU FLAT and STILL...");
  Serial.println("Sampling 100000 readings...");
  Serial.println();

  delay(2000);  // Give time to place flat

  long long axSum = 0, aySum = 0, azSum = 0;
  long long gxSum = 0, gySum = 0, gzSum = 0;
  int samples = 100000;

  for (int i = 0; i < samples; i++) {
    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    axSum += ax;
    aySum += ay;
    azSum += az;
    gxSum += gx;
    gySum += gy;
    gzSum += gz;

    if (i % 1000 == 0) {
      Serial.print("Progress: ");
      Serial.print(i / 10);
      Serial.println("%");
    }

    delay(3);
  }

  // ---------- Average raw values ----------
  float ax_avg = axSum / (float)samples;
  float ay_avg = aySum / (float)samples;
  float az_avg = azSum / (float)samples;
  float gx_avg = gxSum / (float)samples;
  float gy_avg = gySum / (float)samples;
  float gz_avg = gzSum / (float)samples;

  // ---------- Convert to angles ----------
  float ax_g = ax_avg / 16384.0f;
  float ay_g = ay_avg / 16384.0f;
  float az_g = az_avg / 16384.0f;

  float pitchOffset = atan2(ax_g, sqrt(ay_g*ay_g + az_g*az_g)) * 180.0f / PI;
  float rollOffset  = atan2(ay_g, sqrt(ax_g*ax_g + az_g*az_g)) * 180.0f / PI;

  Serial.println();
  Serial.println("==============================");
  Serial.println("      CALIBRATION DONE        ");
  Serial.println("==============================");
  Serial.println();
  Serial.println("-- Raw Averages (for reference) --");
  Serial.print("AX avg: "); Serial.println(ax_avg);
  Serial.print("AY avg: "); Serial.println(ay_avg);
  Serial.print("AZ avg: "); Serial.println(az_avg);
  Serial.print("GX avg: "); Serial.println(gx_avg);
  Serial.print("GY avg: "); Serial.println(gy_avg);
  Serial.print("GZ avg: "); Serial.println(gz_avg);
  Serial.println();
  Serial.println("-- Copy these into transmitter code --");
  Serial.println("--------------------------------------");
  Serial.print("pitchOffset = "); Serial.print(pitchOffset, 4); Serial.println(";");
  Serial.print("rollOffset  = "); Serial.print(rollOffset,  4); Serial.println(";");
  Serial.println("--------------------------------------");
  Serial.println();
  Serial.println("Now upload transmitter code and");
  Serial.println("paste these values where indicated.");
}

void loop() {
  // Nothing here — calibration is one-time in setup()
}
