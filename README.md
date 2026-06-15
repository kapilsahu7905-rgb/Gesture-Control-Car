# Gesture-Control-Car

A fully wireless RC drift car controlled by hand gestures using an MPU6050 IMU glove.

## Hardware Used
- MPU6050 IMU
- ESP32 (Transmitter)
- ESP8266 (Receiver)
- L298N Motor Driver
- 4x BO Motors
- White LEDs (Headlights)
- Red LEDs (Brake/Reverse lights)

## How It Works
Tilt hand forward → car moves forward
Tilt hand backward → car moves backward
Tilt left/right → car turns
Push button on glove → toggles headlights
Red lights glow automatically on brake/reverse

## Files
- transmitter_esp32.ino → Upload to ESP32
- receiver_esp8266.ino → Upload to ESP8266
- calibration_mpu6050.ino → Run once to get MPU offsets
- mac_address.ino → Upload to ESP8266 to get MAC address.

## Protocol
ESP-NOW — no WiFi router needed, peer to peer, ~50Hz update rate

## Complete Working & Logic

The project works by continuously reading the tilt angle of a hand-mounted MPU6050 IMU sensor on the transmitter side. The MPU6050 measures raw accelerometer values on three axes (X, Y, Z) which are converted into two meaningful angles — pitch (forward/backward tilt) and roll (left/right tilt) — using trigonometry. Before these angles are used, pre-measured calibration offsets are subtracted so that the sensor reads exactly zero when the hand is held flat. The raw angles are then smoothed using a low-pass filter to eliminate noise and vibrations. A dead zone threshold is applied next — any tilt below 12° for pitch and 8° for roll is completely ignored, preventing accidental movement from tiny hand tremors. Beyond the dead zone, the tilt angle is mapped proportionally to a PWM speed value between 60 and 255, where 60 is the minimum power needed to actually spin the BO motors against friction and 255 is full speed. The pitch value becomes the base forward/backward speed and the roll value becomes a turn offset — when going forward, the turn offset is added to the left motor and subtracted from the right motor (or vice versa) creating a speed differential between both sides which steers the car. When the car is reversing, turning is disabled so both motors receive equal speed and the car goes straight back. These two motor speed values (left and right) are packed into a small data structure and transmitted wirelessly from the ESP32 to the ESP8266 using ESP-NOW protocol — a lightweight peer-to-peer protocol that requires no WiFi router and operates at approximately 50 updates per second. On the receiver side, the ESP8266 unpacks the received values and feeds them to the L298N motor driver which converts the PWM and direction signals into actual motor movement. Simultaneously, the lighting logic runs in parallel — the ESP8266 monitors the received speed values and automatically switches the red LEDs on whenever the car is braking or reversing, while the white headlights are toggled independently by a push button signal sent from the glove through the same ESP-NOW data packet.
