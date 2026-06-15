/*
 * RECEIVER CODE - ESP8266 + L298N + Headlight + Brakelight
 * ==========================================================
 * L298N   --> ESP8266
 *   ENA   --> D1 (GPIO5)
 *   IN1   --> D2 (GPIO4)
 *   IN2   --> D5 (GPIO14)
 *   ENB   --> D6 (GPIO12)
 *   IN3   --> D7 (GPIO13)
 *   IN4   --> D3 (GPIO0)
 *
 * Headlight (2N2222 base) --> D8 (GPIO15)
 * Brakelight (BC547 base) --> D4 (GPIO2)
 */

#include <ESP8266WiFi.h>
#include <espnow.h>

// Motor A — Left wheels
#define ENA   5    // D1
#define IN1   4    // D2
#define IN2   14   // D5

// Motor B — Right wheels
#define ENB   12   // D6
#define IN3   13   // D7
#define IN4   0    // D3

// Lights
#define HEADLIGHT_PIN  15   // D8 — 2N2222 base
#define BRAKELIGHT_PIN 16    // D4 — BC547 base

// Brakelight auto-off timer
unsigned long lastBrakeTime = 0;
#define BRAKE_TIMEOUT  2000   // 2 seconds

typedef struct {
  int leftSpeed;
  int rightSpeed;
  bool headlight;
} MotorData;

MotorData motorData;

void driveMotorA(int speed) {
  if (speed > 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, constrain(speed, 0, 255));
  } else if (speed < 0) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    analogWrite(ENA, constrain(-speed, 0, 255));
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, 0);
  }
}

void driveMotorB(int speed) {
  if (speed > 0) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    analogWrite(ENB, constrain(speed, 0, 255));
  } else if (speed < 0) {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    analogWrite(ENB, constrain(-speed, 0, 255));
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    analogWrite(ENB, 0);
  }
}

void stopAll() {
  driveMotorA(0);
  driveMotorB(0);
}

bool isForward(int left, int right) {
  // Pure forward or forward+turn (both non-negative, at least one positive)
  return (left >= 0 && right >= 0 && (left > 0 || right > 0));
}

bool isBackward(int left, int right) {
  return (left < 0 && right < 0);
}

bool isStopped(int left, int right) {
  return (left == 0 && right == 0);
}

bool isSpotTurn(int left, int right) {
  // One positive one negative = spot turn in place
  return (left > 0 && right < 0) || (left < 0 && right > 0);
}

void updateBrakelight(int left, int right) {
  if (isBackward(left, right) || isSpotTurn(left, right)) {
    // Backward or spot turn → brakelight ON, reset timer
    digitalWrite(BRAKELIGHT_PIN, HIGH);
    lastBrakeTime = millis();

  } else if (isStopped(left, right)) {
    // Stopped → brakelight ON but start/keep timer
    digitalWrite(BRAKELIGHT_PIN, HIGH);
    // Auto-off after 2 seconds
    if (millis() - lastBrakeTime >= BRAKE_TIMEOUT) {
      digitalWrite(BRAKELIGHT_PIN, LOW);
    }

  } else if (isForward(left, right)) {
    // Forward or forward turn → brakelight OFF always
    digitalWrite(BRAKELIGHT_PIN, LOW);
    lastBrakeTime = millis(); // keep resetting so timer doesn't fire
  }
}

void onReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len == sizeof(MotorData)) {
    memcpy(&motorData, data, sizeof(MotorData));

    driveMotorA(motorData.leftSpeed);
    driveMotorB(motorData.rightSpeed);

    // Headlight
    digitalWrite(HEADLIGHT_PIN, motorData.headlight ? HIGH : LOW);

    // Brakelight logic
    updateBrakelight(motorData.leftSpeed, motorData.rightSpeed);

    Serial.printf("L:%4d  R:%4d  | HDL:%s\n",
                  motorData.leftSpeed, motorData.rightSpeed,
                  motorData.headlight ? "ON" : "OFF");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(HEADLIGHT_PIN, OUTPUT);
  pinMode(BRAKELIGHT_PIN, OUTPUT);

  digitalWrite(HEADLIGHT_PIN, LOW);
  digitalWrite(BRAKELIGHT_PIN, LOW);
  stopAll();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init FAILED!");
    while (true);
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onReceive);

  Serial.println("Receiver ready!");
}

void loop() {
  // Brakelight auto-off check runs continuously
  // even between ESP-NOW packets
  if (isStopped(motorData.leftSpeed, motorData.rightSpeed)) {
    if (millis() - lastBrakeTime >= BRAKE_TIMEOUT) {
      digitalWrite(BRAKELIGHT_PIN, LOW);
    }
  }
  delay(10);
}