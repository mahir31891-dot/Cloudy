# 🤖👀 Autonums Pathfinding Rover

A smart obstacle-avoiding rover built with an **Arduino Nano**, **Arduino Uno**, **HC-SR04 ultrasonic sensor**, **L298N motor driver**, DC motors, and a **TFT display used as the rover's animated eyes**.

The **Arduino Nano controls the rover movement**, while the **Arduino Uno controls the TFT eyes**, giving the rover a robot-like face and expressions.

---

# 🚗 Project Overview

The rover has two main control systems:

```text
                    🤖 EYE ROVER
                         │
             ┌───────────┴───────────┐
             │                       │
       Arduino Nano             Arduino Uno
       Rover Controller          Eye Controller
             │                       │
       ┌─────┴─────┐                 │
       │           │                 ▼
    HC-SR04      L298N          TFT DISPLAY
       │           │                 👀
       │        Motors
       │
   Obstacle
   Detection
```

---

# 🔧 Hardware

## Rover Control

* Arduino Nano
* HC-SR04 ultrasonic sensor
* L298N motor driver
* 2 × DC geared motors
* 2 × wheels
* Rover chassis
* External motor battery

## Eye System

* Arduino Uno
* TFT display
* Jumper wires
* Separate suitable power supply if required

---

# 🧠 Arduino Nano — Rover Controller

The Arduino Nano is responsible for:

* Reading the HC-SR04
* Detecting obstacles
* Moving forward
* Moving backward
* Turning right
* Turning left
* Performing fast full turns
* Checking the path again after turning

The obstacle detection distance is:

```text
10 CM
```

---

# 👀 Arduino Uno — TFT Eyes

The Arduino Uno controls the TFT display mounted on the front of the rover.

The TFT acts as the rover's **eyes**.

The eyes can be programmed to show different expressions depending on what the rover is doing.

### Example expressions

| Rover Action      | TFT Expression     |
| ----------------- | ------------------ |
| Starting          | 🤖 Normal eyes     |
| Moving forward    | 👀 Looking forward |
| Obstacle detected | 😮 Surprised eyes  |
| Moving backward   | 😳 Alert eyes      |
| Turning right     | 👀 Looking right   |
| Turning left      | 👀 Looking left    |
| Searching         | 👁️ Looking around |
| Stopped           | 😐 Normal eyes     |

---

# 👁️ Eye Animation

The TFT eye system can contain smooth animations such as:

```text
NORMAL
  👀

BLINK
  ── ──

LOOK LEFT
  ◉  ●

LOOK RIGHT
  ●  ◉

SURPRISED
  😮

ANGRY
  😡

HAPPY
  😊

SLEEPY
  😴
```

The eye animations are designed to make the rover look more like a **real interactive robot** rather than simply displaying static graphics.

---

# 📌 Arduino Nano Pin Connections

## HC-SR04

| HC-SR04 | Arduino Nano |
| ------- | ------------ |
| VCC     | 5V           |
| GND     | GND          |
| TRIG    | D2           |
| ECHO    | D3           |

## L298N

| L298N | Arduino Nano |
| ----- | ------------ |
| IN1   | D7           |
| IN2   | D8           |
| IN3   | D9           |
| IN4   | D10          |
| GND   | Nano GND     |

---

# ⚙️ L298N Enable Jumpers

This version keeps the L298N enable jumpers installed.

```text
ENA = Jumper ON
ENB = Jumper ON
```

No PWM speed-control wiring is required.

The rover uses full-power motor control for fast movement and pivot turns.

---

# 🔌 Motor Connections

The current motor configuration is:

```text
L298N OUT1 → Right motor -
L298N OUT2 → Right motor +

L298N OUT3 → Left motor +
L298N OUT4 → Left motor -
```

---

# 🔋 Motor Power

Use an external battery suitable for your motors and L298N.

```text
Battery + → L298N motor power +
Battery - → L298N GND
```

The Arduino Nano and L298N must share GND:

```text
Nano GND
   │
   └──── L298N GND
```

Do not power the DC motors directly from the Arduino.

---

# 📏 10 CM Obstacle Detection

The rover uses **10 cm** as its obstacle threshold.

```text
Distance > 10 cm
       ↓
    FORWARD
```

When an obstacle reaches 10 cm or closer:

```text
Distance ≤ 10 cm
       ↓
      STOP
       ↓
    BACKWARD
       ↓
      STOP
       ↓
 FULL RIGHT TURN
       ↓
     CHECK
```

---

# 🔄 Fast Full Right Turn

The rover performs a pivot turn by running the motors in opposite directions.

```text
RIGHT MOTOR → BACKWARD
LEFT MOTOR  → FORWARD
```

This produces a fast rotation:

```text
       ↺
   ┌────────┐
   │  ROVER │
   └────────┘
```

The turn duration can be adjusted in the code.

Example:

```cpp
_delay_ms(480);
```

Increase the value for a larger turn.

Decrease the value for a smaller turn.

---

# 🔄 Emergency Left Turn

If the rover turns right but still detects an obstacle, it can perform a full left turn.

```text
RIGHT MOTOR → FORWARD
LEFT MOTOR  → BACKWARD
```

Example:

```cpp
_delay_ms(600);
```

---

# ⚡ Fast Processing

The Nano uses direct AVR register control for fast motor and sensor processing.

The program uses:

```cpp
#include <avr/io.h>
#include <util/delay.h>
```

Instead of relying on functions such as:

```cpp
pinMode()
digitalWrite()
delay()
```

This keeps the control code lightweight and suitable for the custom mBlock C++ environment previously used for this project.

---

# 🤖 Rover Behavior

The complete rover behavior is:

```text
                START
                  │
                  ▼
             TFT EYES ON
                  │
                  ▼
           READ DISTANCE
                  │
                  ▼
          Distance > 10cm?
             /         \
           YES          NO
            │            │
            ▼            ▼
        👀 FORWARD     😮 ALERT
            │            │
            │            ▼
            │          STOP
            │            │
            │            ▼
            │        BACKWARD
            │            │
            │            ▼
            │       TURN RIGHT
            │            │
            │            ▼
            │         CHECK
            │            │
            │      ┌─────┴─────┐
            │      │           │
            │    CLEAR       BLOCKED
            │      │           │
            │      ▼           ▼
            │   FORWARD     TURN LEFT
            │                  │
            └──────────────────┘
```

---

# 👀 TFT Eye States

The TFT can display different eye states:

### Normal

```text
    👀
```

Used while the rover is moving normally.

### Obstacle

```text
    😮
```

Used when the HC-SR04 detects an obstacle.

### Right Turn

```text
    👀 →
```

The eyes look toward the direction of the turn.

### Left Turn

```text
    ← 👀
```

### Backward

```text
    😳
```

### Idle

```text
    😐
```

---

# 🔄 Arduino Communication

The Nano and Uno can communicate with each other.

The Nano can send simple commands such as:

```text
FORWARD
OBSTACLE
BACKWARD
TURN_RIGHT
TURN_LEFT
STOP
```

The Uno receives the command and changes the TFT eyes.

Example:

```text
Nano                         Uno
 │                            │
 │──── "FORWARD" ────────────►│
 │                            │
 │                         👀 Normal
 │                            │
 │──── "OBSTACLE" ───────────►│
 │                            │
 │                         😮 Alert
 │                            │
 │──── "TURN_RIGHT" ─────────►│
 │                            │
 │                         👀 →
```

---

# ✨ Main Features

* 🤖 Arduino Nano rover controller
* 👀 Arduino Uno TFT eye controller
* 📺 TFT animated robot eyes
* 📡 Arduino-to-Arduino communication
* 📏 10 cm obstacle detection
* ⚡ Fast ultrasonic processing
* 🚗 Automatic forward movement
* 🔙 Automatic backward escape
* 🔄 Fast full right turn
* 🔄 Full left emergency turn
* 👁️ Directional eye expressions
* 😮 Obstacle expression
* 😐 Idle expression
* 🔋 External motor power
* ⚙️ L298N motor driver
* 🔌 ENA jumper ON
* 🔌 ENB jumper ON

---

# 🛠️ Tuning

## Obstacle distance

The current threshold is:

```cpp
10
```

This means:

```text
≤ 10 cm = obstacle
> 10 cm = clear
```

## Right turn

Current value:

```cpp
_delay_ms(480);
```

## Left turn

Current value:

```cpp
_delay_ms(600);
```

## Reverse

Current value:

```cpp
_delay_ms(180);
```

These values can be adjusted according to the rover's wheels, motors, battery and floor.

---

# ⚠️ Testing

Before testing the complete rover:

1. Lift the rover so the wheels are off the ground.
2. Test forward movement.
3. Test backward movement.
4. Test right turn.
5. Test left turn.
6. Test HC-SR04 detection.
7. Test the TFT eyes.
8. Finally place the rover on the floor.

Always keep the rover away from people, cables and objects that could get caught in the wheels.

---

# 🚀 Project Goal

The goal of the Eye Rover is to combine:

```text
ROBOTICS
   +
OBSTACLE AVOIDANCE
   +
ANIMATED TFT EYES
   +
AUTOMATIC MOVEMENT
```

to create an interactive autonomous rover that **moves, detects obstacles, turns, and visually reacts through its TFT eyes**.

---

# 📄 Project Specifications

| Component         | Specification                |
| ----------------- | ---------------------------- |
| Main Controller   | Arduino Nano                 |
| Eye Controller    | Arduino Uno                  |
| Display           | TFT                          |
| Distance Sensor   | HC-SR04                      |
| Motor Driver      | L298N                        |
| Motors            | 2 × DC motors                |
| Obstacle Distance | 10 cm                        |
| ENA Jumper        | ON                           |
| ENB Jumper        | ON                           |
| Turning           | Full-power pivot             |
| Control Method    | AVR register based           |
| Rover Type        | Autonomous obstacle avoiding |
| Display Function  | Animated robot eyes          |

---

# 🤖👀 Final

**Eye Rover** is an autonomous Arduino rover with a TFT-based animated face.

The Nano handles the **brain and movement**, while the Uno and TFT provide the **eyes and expressions**.

```text
       ┌─────────────────┐
       │    TFT EYES     │
       │      👀         │
       └────────┬────────┘
                │
          Arduino Uno
                │
        ┌───────┴───────┐
        │               │
   Arduino Nano      Eye System
        │
   ┌────┴─────┐
   │          │
HC-SR04     L298N
              │
           🚗 Motors
```

**Project:** Eye Rover
**Obstacle Detection:** 10 cm
**Turning:** Fast Full Pivot
**Eyes:** TFT Animated Display
**Main Controller:** Arduino Nano
**Eye Controller:** Arduino Uno
