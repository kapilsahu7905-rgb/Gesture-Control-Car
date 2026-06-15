/*
 * TRANSMITTER CODE - ESP32 + MPU6050 + Headlight Button
 * ======================================================
 * MPU6050 --> ESP32
 *   VCC   --> 3.3V
 *   GND   --> GND
 *   SDA   --> G21
 *   SCL   --> G22
 *
 * Push Button --> GPIO19 and GND
 * Protocol  : ESP-NOW
 * Receiver  : 30:83:98:95:14:25
 * Core      : ESP32 Arduino Core 3.x
 */

#include <Wire.h>
#include <MPU6050.h>
#include <esp_now.h>
#include <WiFi.h>

uint8_t receiverMAC[] = {0x30, 0x83, 0x98, 0x95, 0x14, 0x25};

typedef struct {
  int leftSpeed;
  int rightSpeed;
  bool headlight;
} MotorData;

MotorData motorData;
MPU6050 mpu;

float filteredPitch = 0;
float filteredRoll  = 0;
float smoothLeft    = 0;
float smoothRight   = 0;

float pitchOffset = 3.7323;
float rollOffset  = 1.2550;

#define LPF             0.7f
#define SMOOTH          0.3f
#define PITCH_DEAD_ZONE 12.0f
#define ROLL_DEAD_ZONE  8.0f
#define MAX_ANGLE       35.0f
#define MIN_PWM         60

// Button
#define BUTTON_PIN      19
bool headlightState     = false;
bool lastButtonState    = HIGH;
unsigned long lastDebounceTime = 0;
#define DEBOUNCE_DELAY  50

void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}

int applyDeadZone(float angle, float deadZone) {
  if (abs(angle) < deadZone) return 0;
  float sign  = (angle > 0) ? 1.0f : -1.0f;
  float ratio = (abs(angle) - deadZone) / (MAX_ANGLE - deadZone);
  ratio = constrain(ratio, 0.0f, 1.0f);
  return (int)(sign * (MIN_PWM + ratio * (255 - MIN_PWM)));
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(21, 22);
  mpu.initialize();

  int16_t ax, ay, az, gx, gy, gz;
  for (int i = 0; i < 100; i++) {
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    delay(5);
  }

  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  float ax_g = ax / 16384.0f;
  float ay_g = ay / 16384.0f;
  float az_g = az / 16384.0f;
  filteredPitch = atan2(ax_g, sqrt(ay_g*ay_g + az_g*az_g)) * 180.0f / PI - pitchOffset;
  filteredRoll  = atan2(ay_g, sqrt(ax_g*ax_g + az_g*az_g)) * 180.0f / PI - rollOffset;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW FAILED!"); while(true); }
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) { Serial.println("Peer FAILED!"); while(true); }

  Serial.println("Transmitter ready!");
}

void loop() {
  // ── Button debounce & toggle ──────────────────────
  bool currentButton = digitalRead(BUTTON_PIN);
  if (currentButton == LOW && lastButtonState == HIGH) {
    if (millis() - lastDebounceTime > DEBOUNCE_DELAY) {
      headlightState = !headlightState;
      lastDebounceTime = millis();
      Serial.printf("Headlight: %s\n", headlightState ? "ON" : "OFF");
    }
  }
  lastButtonState = currentButton;
  // ── MPU6050 read ─────────────────────────────────
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  float ax_g = ax / 16384.0f;
  float ay_g = ay / 16384.0f;
  float az_g = az / 16384.0f;

  float rawPitch = atan2(ax_g, sqrt(ay_g*ay_g + az_g*az_g)) * 180.0f / PI - pitchOffset;
  float rawRoll  = atan2(ay_g, sqrt(ax_g*ax_g + az_g*az_g)) * 180.0f / PI - rollOffset;

  filteredPitch = LPF * filteredPitch + (1.0f - LPF) * rawPitch;
  filteredRoll  = LPF * filteredRoll  + (1.0f - LPF) * rawRoll;

  int base = applyDeadZone(-filteredPitch, PITCH_DEAD_ZONE);
  int turn = applyDeadZone(-filteredRoll,  ROLL_DEAD_ZONE);

  int targetLeft  = 0;
  int targetRight = 0;

  if (base == 0 && turn == 0) {
    targetLeft  = 0;
    targetRight = 0;
  } else if (base == 0 && turn != 0) {
    targetLeft  = -turn;
    targetRight =  turn;
  } else if (base > 0) {
    targetLeft  = constrain(base - turn, 0, 255);
    targetRight = constrain(base + turn, 0, 255);
  } else {
    targetLeft  = base;
    targetRight = base;
  }

  smoothLeft  = smoothLeft  + SMOOTH * (targetLeft  - smoothLeft);
  smoothRight = smoothRight + SMOOTH * (targetRight - smoothRight);

  if (base == 0 && turn == 0) {
    smoothLeft  = 0;
    smoothRight = 0;
  }

  motorData.leftSpeed  = (int)smoothLeft;
  motorData.rightSpeed = (int)smoothRight;
  motorData.headlight  = headlightState;

  esp_now_send(receiverMAC, (uint8_t *)&motorData, sizeof(motorData));

  Serial.printf("P:%6.1f  R:%6.1f  | base:%4d  turn:%4d  | L:%4d  R:%4d  | HDL:%s\n",
                filteredPitch, filteredRoll, base, turn,
                motorData.leftSpeed, motorData.rightSpeed,
                headlightState ? "ON" : "OFF");

  delay(20);
}