# 🤖 Autonomous Sensor-Fusion Rover

A smart autonomous rover based on **Arduino Nano** that combines GPS, compass, IMU sensors, ultrasonic obstacle detection, and motor control.

The rover is designed to continue operating even when the GPS signal becomes weak or completely unavailable.

---

## 🚀 Main Features

* 📍 GPS waypoint navigation
* 🛰️ GPS-loss autonomous fallback
* 🧭 Compass-based heading control
* 🌀 MPU6050 rotation and tilt detection
* 📐 ADXL345 acceleration and movement detection
* 📏 HC-SR04 obstacle detection
* 🧠 Temporary obstacle-direction memory
* 🔄 Automatic alternate-direction selection
* 🛑 Severe tilt / rollover protection
* 🚗 Automatic movement verification
* 🔧 L298N motor control
* 🔌 Works with ENA and ENB jumpers ON
* 🔁 Automatically resumes GPS navigation when GPS returns

---

# 🧠 System Concept

The rover does not depend on a single sensor.

All sensors work together continuously.

```text
                    GPS
                     │
              Position / Waypoint
                     │
                     ▼
             ┌───────────────┐
             │  NAVIGATION   │
             │    SYSTEM     │
             └───────┬───────┘
                     │
       ┌─────────────┼─────────────┐
       ▼             ▼             ▼
   COMPASS       MPU6050        ADXL345
   Heading        Rotation      Movement
                  + Tilt        + Accel
       │             │             │
       └─────────────┼─────────────┘
                     │
                     ▼
                  HC-SR04
                 Obstacles
                     │
                     ▼
              DECISION SYSTEM
                     │
                     ▼
                   L298N
                     │
                     ▼
                  MOTORS
```

---

# 📡 GPS Navigation

GPS provides the rover's geographical position.

It can be used for:

* Latitude
* Longitude
* Waypoints
* Distance to destination
* Direction toward destination
* Geofencing

Example:

```text
GPS Position
     ↓
Current latitude/longitude
     ↓
Target latitude/longitude
     ↓
Calculate bearing
     ↓
Compass checks heading
     ↓
Rover turns
     ↓
Rover moves toward target
```

## ⚠️ GPS Is Not Required

The rover does **not** completely stop when GPS is lost.

If GPS becomes weak or unavailable:

```text
GPS LOST
   ↓
Last reliable navigation information
   +
Compass
   +
MPU6050
   +
ADXL345
   +
HC-SR04
   ↓
Continue autonomous operation
```

When GPS becomes available again, the rover can resume GPS-based navigation.

---

# 🧭 Compass

The compass provides the rover's absolute heading.

For example:

```text
0°   = North
90°  = East
180° = South
270° = West
```

The compass is used to:

* Determine current direction
* Follow a GPS bearing
* Choose alternate directions
* Remember obstacle directions
* Prevent repeatedly returning toward blocked directions

---

# 🌀 MPU6050

The MPU6050 contains:

* 3-axis accelerometer
* 3-axis gyroscope

It is used for:

* Rotation detection
* Turn measurement
* Tilt detection
* Heading-change detection
* Movement confirmation
* Rover stability

The gyroscope is especially useful while turning.

Example:

```text
START TURN
    ↓
MPU6050 measures rotation
    ↓
Compass checks heading
    ↓
Desired heading reached
    ↓
STOP TURN
```

---

# 📐 ADXL345

The ADXL345 is a 3-axis accelerometer.

It is used for:

* Acceleration detection
* Bump detection
* Movement confirmation
* Tilt estimation
* Sudden impact detection

The ADXL345 works together with the MPU6050.

```text
ADXL345
   +
MPU6050
   ↓
Compare movement/tilt
   ↓
More reliable safety decision
```

### Important

The ADXL345 **cannot act as a true wheel odometer**.

Acceleration eventually returns close to zero when a rover moves at constant speed.

Therefore, for highly reliable distance/movement measurement, wheel encoders are recommended.

---

# 📏 HC-SR04

The HC-SR04 detects objects in front of the rover.

Current obstacle threshold:

```text
10 cm
```

If an obstacle is detected within approximately 10 cm:

```text
STOP
 ↓
Remember obstacle heading
 ↓
Reverse
 ↓
Choose another heading
 ↓
Turn
 ↓
Check again
 ↓
Continue
```

---

# 🧠 Obstacle Memory

The rover temporarily remembers directions where obstacles were detected.

Example:

```text
Current heading = 90°

HC-SR04 detects obstacle

Remember:

90° = BLOCKED
```

The rover then selects another direction.

Example:

```text
90°   → BLOCKED
135°  → BLOCKED
180°  → TRY
270°  → CLEAR
```

This prevents the rover from repeatedly choosing the same blocked path.

---

# 🔄 Automatic Alternate Direction

When an obstacle is detected:

```text
Obstacle
   ↓
Save current compass heading
   ↓
Check previously blocked headings
   ↓
Choose a new direction
   ↓
Turn using MPU6050 + compass
   ↓
Check HC-SR04
   ↓
Move
```

If the new direction is also blocked:

```text
TRY DIRECTION 1
       ↓
   BLOCKED
       ↓
TRY DIRECTION 2
       ↓
   BLOCKED
       ↓
TRY DIRECTION 3
       ↓
   CLEAR
       ↓
CONTINUE
```

---

# 🚗 Movement Verification

The rover also checks whether it appears to be moving after a motor command.

Example:

```text
MOTOR = FORWARD
       ↓
ADXL345 + MPU6050
       ↓
Movement detected?
```

### Movement detected

```text
YES
 ↓
Continue
```

### No significant movement

```text
NO
 ↓
Possible stuck condition
 ↓
STOP
 ↓
Remember current direction
 ↓
Choose another direction
 ↓
Try again
```

This feature works even without GPS.

---

# 🛑 Stability and Crash Protection

The MPU6050 and ADXL345 continuously monitor rover tilt.

The system uses both sensors rather than relying on only one.

```text
Small tilt
    ↓
Continue

Medium tilt
    ↓
Monitor / stabilize movement

Severe sustained tilt
    ↓
EMERGENCY STOP
```

The purpose is to prevent a small bump from being incorrectly treated as a crash.

---

# 🌊 Water / Lake / River Safety

GPS can be used to create known danger zones.

For example:

```text
Lake boundary
     ↓
GPS geofence
     ↓
Rover approaches boundary
     ↓
STOP
```

However, GPS cannot identify every unknown lake or river by itself.

For physical protection near water, additional downward/edge/water sensors are recommended.

A safer system is:

```text
GPS danger zone
       +
HC-SR04
       +
Tilt sensors
       +
Physical water/edge detection
       ↓
SAFETY DECISION
       ↓
STOP
```

---

# 🔌 Hardware

## Required

* Arduino Nano
* GPS module
* HMC5883L or QMC5883L compass
* MPU6050
* ADXL345
* HC-SR04
* L298N motor driver
* 2 DC motors / rover chassis
* Battery
* Jumper wires

## Recommended Future Upgrade

* 2 wheel encoders
* Downward-facing distance sensors
* Water detection sensors
* Buzzer
* Emergency-stop switch

---

# 🔧 Pin Configuration

## Arduino Nano

| Device       | Pin |
| ------------ | --- |
| HC-SR04 TRIG | D2  |
| HC-SR04 ECHO | D3  |
| GPS TX       | D4  |
| L298N IN1    | D7  |
| L298N IN2    | D8  |
| L298N IN3    | D9  |
| L298N IN4    | D10 |
| I2C SDA      | A4  |
| I2C SCL      | A5  |

---

# ⚙️ L298N Motor Wiring

```text
L298N OUT1 → Right motor negative
L298N OUT2 → Right motor positive

L298N OUT3 → Left motor positive
L298N OUT4 → Left motor negative
```

Motor control:

```text
IN1 + IN2 → Right motor
IN3 + IN4 → Left motor
```

### ENA / ENB

The rover is designed with:

```text
ENA jumper = ON
ENB jumper = ON
```

Therefore the rover does not use PWM speed control through ENA/ENB.

Motor turns are performed using:

* Forward
* Reverse
* Pivot right
* Pivot left
* Timed corrections
* Compass/gyro-based heading control

---

# 🔗 I2C Bus

The MPU6050, ADXL345, and compass can share the same I2C bus.

```text
Arduino Nano A4
      │
      ├── MPU6050 SDA
      ├── ADXL345 SDA
      └── Compass SDA

Arduino Nano A5
      │
      ├── MPU6050 SCL
      ├── ADXL345 SCL
      └── Compass SCL
```

All devices share:

```text
GND → GND
```

Make sure the voltage requirements of your individual breakout boards are respected.

---

# 🧠 Sensor Priority

The rover should prioritize decisions approximately like this:

```text
1. EMERGENCY / WATER / EDGE SAFETY
             ↓
2. SEVERE TILT / ROLLOVER
             ↓
3. HC-SR04 OBSTACLE
             ↓
4. STUCK / NO MOVEMENT
             ↓
5. GPS NAVIGATION
             ↓
6. COMPASS HEADING
             ↓
7. MPU6050 TURN CONTROL
             ↓
8. ADXL345 MOVEMENT CONFIRMATION
             ↓
9. MOTOR CONTROL
```

Safety conditions should always be able to override navigation.

---

# 🔁 Complete Decision Flow

```text
              START
                │
                ▼
        Initialize sensors
                │
                ▼
        Calibrate MPU6050
                │
                ▼
          Read all sensors
                │
                ▼
       ┌──────────────────┐
       │ Severe tilt?     │
       └───────┬──────────┘
               │
          YES  │  NO
           ↓   │
          STOP │
               │
               ▼
       HC-SR04 obstacle?
               │
          ┌────┴────┐
         YES        NO
          │          │
          ▼          ▼
       Remember    GPS fix?
       heading        │
          │       ┌───┴───┐
          │      YES     NO
          │       │       │
          │       ▼       ▼
          │     GPS     Compass +
          │   navigation MPU6050 +
          │               ADXL345
          │                 │
          └──────┬──────────┘
                 ▼
          Check movement
                 │
          ┌──────┴──────┐
         MOVING       STUCK
            │             │
            ▼             ▼
         Continue      New heading
                          │
                          ▼
                       Try again
```

---

# 🛰️ GPS + Sensor Fusion

The goal is not:

```text
GPS → Rover
```

The goal is:

```text
GPS
 +
Compass
 +
MPU6050
 +
ADXL345
 +
HC-SR04
 +
Motor feedback
 ↓
Sensor Fusion
 ↓
Autonomous Decision
 ↓
Rover
```

This makes GPS an important navigation sensor without making it the **only thing keeping the rover operational**.

---

# 🚀 Future Improvements

The rover can later be upgraded with:

### 1. Wheel Encoders

For accurate:

* Distance traveled
* Wheel movement
* Stuck detection
* Dead reckoning

### 2. Better Obstacle Detection

Add:

* Front ultrasonic
* Left ultrasonic
* Right ultrasonic

This allows the rover to compare all three directions before turning.

### 3. Water Detection

Add downward-facing:

* Water sensor
* IR/ToF distance sensor
* Edge detector

### 4. Better GPS

Use a higher-quality GNSS module with:

* More satellite systems
* Better antenna
* Faster fix
* Better accuracy

### 5. Sensor Fusion Position Estimation

Eventually:

```text
GPS
 +
Compass
 +
MPU6050
 +
Wheel encoders
 ↓
Estimated position
```

The rover can then continue estimating its position during short GPS outages.

---

# ⚠️ Important Limitations

This project is an autonomous rover prototype and should be tested in an open, safe area.

Do not initially test it near:

* Deep water
* Roads
* Cliffs
* Dams
* Moving traffic
* People
* Other dangerous areas

GPS can have errors, and inexpensive compass/IMU sensors require calibration.

The rover should always have a physical emergency-stop method.

---

# 📜 License

This project is intended for educational and experimental robotics use.

---

# 🤖 Project Goal

The final goal is to build a rover that can:

```text
Navigate
   +
Avoid obstacles
   +
Remember blocked directions
   +
Detect movement
   +
Detect abnormal tilt
   +
Use GPS when available
   +
Continue when GPS is unavailable
   +
Use compass for direction
   +
Use MPU6050 for rotation
   +
Use ADXL345 for movement/acceleration
   +
Protect itself near dangerous areas
```

**GPS should help the rover navigate — not be the only thing keeping the rover alive.**
