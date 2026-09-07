/*
   AUTONOMOUS SENSOR-FUSION ROVER
   Arduino Nano

   FEATURES:
   - GPS navigation
   - GPS-loss autonomous fallback
   - HMC5883L / QMC5883L compass
   - MPU6050 gyro + tilt
   - ADXL345 acceleration + tilt
   - HC-SR04 obstacle detection
   - Obstacle heading memory
   - Automatic alternate direction
   - Stuck/movement checking
   - Severe tilt emergency stop
   - MPU6050 controlled turning
   - L298N ENA/ENB jumpers ON

   IMPORTANT:
   ADXL345 is NOT an odometer.
   Without wheel encoders, movement detection is approximate.
*/

#include <avr/io.h>
#include <util/delay.h>
#include <math.h>

/* =========================================================
   PIN DEFINITIONS
   ========================================================= */

// HC-SR04
#define TRIG_BIT  PD2
#define ECHO_BIT  PD3

// GPS RX
#define GPS_RX_BIT PD4

// L298N
#define IN1_BIT PD7
#define IN2_BIT PB0
#define IN3_BIT PB1
#define IN4_BIT PB2

// I2C
#define SDA_BIT PC4       // A4
#define SCL_BIT PC5       // A5

/* =========================================================
   I2C ADDRESSES
   ========================================================= */

#define MPU_ADDR   0x68
#define ADXL_ADDR  0x53

#define HMC_ADDR   0x1E
#define QMC_ADDR   0x0D

/* =========================================================
   GLOBAL SENSOR VALUES
   ========================================================= */

// MPU
int16_t mpuAx, mpuAy, mpuAz;
int16_t mpuGx, mpuGy, mpuGz;

// ADXL
int16_t adxlX, adxlY, adxlZ;

// Gyro calibration
long gyroBiasZ = 0;

// Compass
uint8_t compassType = 0; // 0 none, 1 HMC, 2 QMC

float compassHeading = 0.0;

/* =========================================================
   GPS
   ========================================================= */

float gpsLat = 0.0;
float gpsLon = 0.0;

float targetLat = 0.0;
float targetLon = 0.0;

bool gpsFix = false;
int gpsSatellites = 0;

/*
   CHANGE THESE TO YOUR WAYPOINT
*/
#define TARGET_LAT  22.3072
#define TARGET_LON  73.1812

/* =========================================================
   NAVIGATION
   ========================================================= */

float desiredHeading = 0.0;

bool gpsWasGood = false;

float lastGoodLat = 0;
float lastGoodLon = 0;

/* =========================================================
   OBSTACLE MEMORY
   ========================================================= */

#define MAX_BLOCKED 8

float blockedHeading[MAX_BLOCKED];
unsigned long blockedTime[MAX_BLOCKED];

int blockedCount = 0;

#define BLOCK_MEMORY_TIME 30000UL

/* =========================================================
   SAFETY
   ========================================================= */

bool emergencyStop = false;

#define MAX_TILT_DEG 38.0

/* =========================================================
   I2C LOW LEVEL
   ========================================================= */

void SDA_HIGH()
{
  DDRC &= ~(1 << SDA_BIT);
  PORTC |= (1 << SDA_BIT);
}

void SDA_LOW()
{
  DDRC |= (1 << SDA_BIT);
  PORTC &= ~(1 << SDA_BIT);
}

void SCL_HIGH()
{
  DDRC &= ~(1 << SCL_BIT);
  PORTC |= (1 << SCL_BIT);
}

void SCL_LOW()
{
  DDRC |= (1 << SCL_BIT);
  PORTC &= ~(1 << SCL_BIT);
}

uint8_t SDA_READ()
{
  return (PINC & (1 << SDA_BIT)) != 0;
}

void i2cDelay()
{
  _delay_us(4);
}

void i2cStart()
{
  SDA_HIGH();
  SCL_HIGH();
  i2cDelay();

  SDA_LOW();
  i2cDelay();

  SCL_LOW();
}

void i2cStop()
{
  SDA_LOW();
  i2cDelay();

  SCL_HIGH();
  i2cDelay();

  SDA_HIGH();
  i2cDelay();
}

bool i2cWrite(uint8_t data)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (data & 0x80)
      SDA_HIGH();
    else
      SDA_LOW();

    SCL_HIGH();
    i2cDelay();
    SCL_LOW();

    data <<= 1;
  }

  SDA_HIGH();

  SCL_HIGH();
  i2cDelay();

  bool ack = !SDA_READ();

  SCL_LOW();

  return ack;
}

uint8_t i2cRead(bool ack)
{
  uint8_t data = 0;

  SDA_HIGH();

  for (uint8_t i = 0; i < 8; i++)
  {
    data <<= 1;

    SCL_HIGH();
    i2cDelay();

    if (SDA_READ())
      data |= 1;

    SCL_LOW();
    i2cDelay();
  }

  if (ack)
    SDA_LOW();
  else
    SDA_HIGH();

  SCL_HIGH();
  i2cDelay();
  SCL_LOW();

  SDA_HIGH();

  return data;
}

bool writeReg(uint8_t addr, uint8_t reg, uint8_t value)
{
  i2cStart();

  if (!i2cWrite((addr << 1) | 0))
  {
    i2cStop();
    return false;
  }

  if (!i2cWrite(reg))
  {
    i2cStop();
    return false;
  }

  if (!i2cWrite(value))
  {
    i2cStop();
    return false;
  }

  i2cStop();

  return true;
}

uint8_t readReg(uint8_t addr, uint8_t reg)
{
  uint8_t value;

  i2cStart();

  i2cWrite((addr << 1) | 0);
  i2cWrite(reg);

  i2cStart();

  i2cWrite((addr << 1) | 1);

  value = i2cRead(false);

  i2cStop();

  return value;
}

bool devicePresent(uint8_t addr)
{
  i2cStart();

  bool ok = i2cWrite((addr << 1) | 0);

  i2cStop();

  return ok;
}

/* =========================================================
   MPU6050
   ========================================================= */

bool initMPU()
{
  if (!devicePresent(MPU_ADDR))
    return false;

  // Wake up
  writeReg(MPU_ADDR, 0x6B, 0x00);

  // Accelerometer ±2g
  writeReg(MPU_ADDR, 0x1C, 0x00);

  // Gyroscope ±250 degrees/sec
  writeReg(MPU_ADDR, 0x1B, 0x00);

  _delay_ms(100);

  return true;
}

void readMPU()
{
  uint8_t h, l;

  h = readReg(MPU_ADDR, 0x3B);
  l = readReg(MPU_ADDR, 0x3C);
  mpuAx = ((int16_t)h << 8) | l;

  h = readReg(MPU_ADDR, 0x3D);
  l = readReg(MPU_ADDR, 0x3E);
  mpuAy = ((int16_t)h << 8) | l;

  h = readReg(MPU_ADDR, 0x3F);
  l = readReg(MPU_ADDR, 0x40);
  mpuAz = ((int16_t)h << 8) | l;

  h = readReg(MPU_ADDR, 0x47);
  l = readReg(MPU_ADDR, 0x48);
  mpuGz = ((int16_t)h << 8) | l;
}

/* =========================================================
   ADXL345
   ========================================================= */

bool initADXL()
{
  if (!devicePresent(ADXL_ADDR))
    return false;

  // Measurement mode
  writeReg(ADXL_ADDR, 0x2D, 0x08);

  // Full resolution ±2g
  writeReg(ADXL_ADDR, 0x31, 0x08);

  // 100 Hz
  writeReg(ADXL_ADDR, 0x2C, 0x0A);

  _delay_ms(100);

  return true;
}

void readADXL()
{
  uint8_t h, l;

  l = readReg(ADXL_ADDR, 0x32);
  h = readReg(ADXL_ADDR, 0x33);
  adxlX = ((int16_t)h << 8) | l;

  l = readReg(ADXL_ADDR, 0x34);
  h = readReg(ADXL_ADDR, 0x35);
  adxlY = ((int16_t)h << 8) | l;

  l = readReg(ADXL_ADDR, 0x36);
  h = readReg(ADXL_ADDR, 0x37);
  adxlZ = ((int16_t)h << 8) | l;
}

/* =========================================================
   COMPASS
   Supports HMC5883L and QMC5883L
   ========================================================= */

bool initCompass()
{
  if (devicePresent(HMC_ADDR))
  {
    compassType = 1;

    // HMC5883L
    writeReg(HMC_ADDR, 0x00, 0x70);
    writeReg(HMC_ADDR, 0x01, 0x20);
    writeReg(HMC_ADDR, 0x02, 0x00);

    _delay_ms(100);

    return true;
  }

  if (devicePresent(QMC_ADDR))
  {
    compassType = 2;

    // QMC5883L
    writeReg(QMC_ADDR, 0x0B, 0x01);
    writeReg(QMC_ADDR, 0x09, 0x1D);

    _delay_ms(100);

    return true;
  }

  return false;
}

float readCompass()
{
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;

  uint8_t xl, xh;
  uint8_t yl, yh;
  uint8_t zl, zh;

  if (compassType == 1)
  {
    /*
       HMC5883L order:
       X MSB/LSB
       Z MSB/LSB
       Y MSB/LSB
    */

    xh = readReg(HMC_ADDR, 0x03);
    xl = readReg(HMC_ADDR, 0x04);

    zh = readReg(HMC_ADDR, 0x05);
    zl = readReg(HMC_ADDR, 0x06);

    yh = readReg(HMC_ADDR, 0x07);
    yl = readReg(HMC_ADDR, 0x08);

    x = ((int16_t)xh << 8) | xl;
    y = ((int16_t)yh << 8) | yl;
    z = ((int16_t)zh << 8) | zl;
  }
  else if (compassType == 2)
  {
    xl = readReg(QMC_ADDR, 0x00);
    xh = readReg(QMC_ADDR, 0x01);

    yl = readReg(QMC_ADDR, 0x02);
    yh = readReg(QMC_ADDR, 0x03);

    zl = readReg(QMC_ADDR, 0x04);
    zh = readReg(QMC_ADDR, 0x05);

    x = ((int16_t)xh << 8) | xl;
    y = ((int16_t)yh << 8) | yl;
    z = ((int16_t)zh << 8) | zl;
  }

  float heading = atan2((float)y, (float)x) * 180.0 / 3.14159265;

  if (heading < 0)
    heading += 360.0;

  if (heading >= 360)
    heading -= 360;

  return heading;
}

/* =========================================================
   TILT CALCULATION
   ========================================================= */

float getMPUTilt()
{
  float ax = (float)mpuAx;
  float ay = (float)mpuAy;
  float az = (float)mpuAz;

  float roll =
      atan2(ay, sqrt(ax * ax + az * az))
      * 180.0 / 3.14159265;

  float pitch =
      atan2(-ax, sqrt(ay * ay + az * az))
      * 180.0 / 3.14159265;

  float maxTilt = fabs(roll);

  if (fabs(pitch) > maxTilt)
    maxTilt = fabs(pitch);

  return maxTilt;
}

float getADXLTilt()
{
  float ax = (float)adxlX;
  float ay = (float)adxlY;
  float az = (float)adxlZ;

  float roll =
      atan2(ay, sqrt(ax * ax + az * az))
      * 180.0 / 3.14159265;

  float pitch =
      atan2(-ax, sqrt(ay * ay + az * az))
      * 180.0 / 3.14159265;

  float maxTilt = fabs(roll);

  if (fabs(pitch) > maxTilt)
    maxTilt = fabs(pitch);

  return maxTilt;
}

/* =========================================================
   MOTOR CONTROL
   ========================================================= */

void stopMotors()
{
  PORTD &= ~((1 << IN1_BIT));
  PORTB &= ~((1 << IN2_BIT) |
             (1 << IN3_BIT) |
             (1 << IN4_BIT));
}

void forward()
{
  if (emergencyStop)
  {
    stopMotors();
    return;
  }

  // Right motor forward
  PORTD |= (1 << IN1_BIT);
  PORTB &= ~(1 << IN2_BIT);

  // Left motor forward
  PORTB |= (1 << IN3_BIT);
  PORTB &= ~(1 << IN4_BIT);
}

void backward()
{
  if (emergencyStop)
  {
    stopMotors();
    return;
  }

  // Right motor backward
  PORTD &= ~(1 << IN1_BIT);
  PORTB |= (1 << IN2_BIT);

  // Left motor backward
  PORTB &= ~(1 << IN3_BIT);
  PORTB |= (1 << IN4_BIT);
}

void rightTurn()
{
  if (emergencyStop)
  {
    stopMotors();
    return;
  }

  // Right motor backward
  PORTD &= ~(1 << IN1_BIT);
  PORTB |= (1 << IN2_BIT);

  // Left motor forward
  PORTB |= (1 << IN3_BIT);
  PORTB &= ~(1 << IN4_BIT);
}

void leftTurn()
{
  if (emergencyStop)
  {
    stopMotors();
    return;
  }

  // Right motor forward
  PORTD |= (1 << IN1_BIT);
  PORTB &= ~(1 << IN2_BIT);

  // Left motor backward
  PORTB &= ~(1 << IN3_BIT);
  PORTB |= (1 << IN4_BIT);
}

/* =========================================================
   ULTRASONIC
   ========================================================= */

unsigned int getDistance()
{
  unsigned long count = 0;

  PORTD &= ~(1 << TRIG_BIT);
  _delay_us(3);

  PORTD |= (1 << TRIG_BIT);
  _delay_us(10);
  PORTD &= ~(1 << TRIG_BIT);

  // Wait for echo HIGH
  count = 0;

  while (!(PIND & (1 << ECHO_BIT)))
  {
    _delay_us(1);

    count++;

    if (count > 20000)
      return 0;
  }

  count = 0;

  while (PIND & (1 << ECHO_BIT))
  {
    _delay_us(1);

    count++;

    if (count > 20000)
      return 0;
  }

  return count / 58;
}

/* =========================================================
   GPS SOFTWARE SERIAL
   GPS TX -> D4
   9600 baud
   ========================================================= */

char gpsReadByte()
{
  char c = 0;

  while (PIND & (1 << GPS_RX_BIT))
  {
    // wait for start bit
  }

  _delay_us(52);

  for (uint8_t i = 0; i < 8; i++)
  {
    _delay_us(104);

    if (PIND & (1 << GPS_RX_BIT))
      c |= (1 << i);
  }

  _delay_us(104);

  return c;
}

/*
   This parser searches GPRMC / GNRMC.
*/

bool parseGPS()
{
  char buffer[100];

  uint8_t index = 0;

  unsigned long start = 0;

  /*
     Read for approximately 1 second.
  */

  while (index < 98)
  {
    char c;

    /*
       This implementation waits for '$'.
    */

    c = gpsReadByte();

    if (c == '$')
    {
      index = 0;
      buffer[index++] = c;

      while (index < 98)
      {
        c = gpsReadByte();

        if (c == '\n')
        {
          buffer[index] = '\0';
          break;
        }

        buffer[index++] = c;
      }

      /*
         Check RMC sentence.
      */

      if (
        buffer[1] == 'G' &&
        (buffer[2] == 'P' || buffer[2] == 'N') &&
        buffer[3] == 'R' &&
        buffer[4] == 'M' &&
        buffer[5] == 'C'
      )
      {
        return parseRMC(buffer);
      }
    }

    start++;

    if (start > 1200)
      break;
  }

  return false;
}

/* =========================================================
   GPS RMC PARSER
   ========================================================= */

float convertGPSCoordinate(
    char *value,
    char direction)
{
  float raw = atof(value);

  int degrees = (int)(raw / 100.0);

  float minutes =
      raw - (degrees * 100.0);

  float decimal =
      degrees + minutes / 60.0;

  if (direction == 'S' ||
      direction == 'W')
  {
    decimal = -decimal;
  }

  return decimal;
}

bool parseRMC(char *s)
{
  char field[20];

  int fieldNumber = 0;

  int fieldIndex = 0;

  char status = 'V';

  char latString[15];
  char lonString[15];

  char latDir = 'N';
  char lonDir = 'E';

  for (uint8_t i = 0; i < 100; i++)
  {
    char c = s[i];

    if (c == ',' || c == '\0')
    {
      field[fieldIndex] = '\0';

      if (fieldNumber == 2)
      {
        status = field[0];
      }

      if (fieldNumber == 3)
      {
        for (uint8_t j = 0; j < 14; j++)
          latString[j] = field[j];
      }

      if (fieldNumber == 4)
        latDir = field[0];

      if (fieldNumber == 5)
      {
        for (uint8_t j = 0; j < 14; j++)
          lonString[j] = field[j];
      }

      if (fieldNumber == 6)
        lonDir = field[0];

      fieldNumber++;
      fieldIndex = 0;
    }
    else
    {
      if (fieldIndex < 19)
        field[fieldIndex++] = c;
    }

    if (c == '\0')
      break;
  }

  if (status != 'A')
  {
    gpsFix = false;
    return false;
  }

  gpsLat =
      convertGPSCoordinate(latString, latDir);

  gpsLon =
      convertGPSCoordinate(lonString, lonDir);

  gpsFix = true;

  lastGoodLat = gpsLat;
  lastGoodLon = gpsLon;

  return true;
}

/* =========================================================
   GPS DISTANCE
   ========================================================= */

float distanceMeters(
    float lat1,
    float lon1,
    float lat2,
    float lon2)
{
  float dLat =
      (lat2 - lat1) * 111320.0;

  float dLon =
      (lon2 - lon1) *
      111320.0 *
      cos(lat1 * 3.14159265 / 180.0);

  return sqrt(
      dLat * dLat +
      dLon * dLon
  );
}

/* =========================================================
   GPS BEARING
   ========================================================= */

float bearingTo(
    float lat1,
    float lon1,
    float lat2,
    float lon2)
{
  float y =
      sin((lon2 - lon1) *
          3.14159265 / 180.0) *
      cos(lat2 *
          3.14159265 / 180.0);

  float x =
      cos(lat1 *
          3.14159265 / 180.0) *
      sin(lat2 *
          3.14159265 / 180.0)
      -
      sin(lat1 *
          3.14159265 / 180.0) *
      cos(lat2 *
          3.14159265 / 180.0) *
      cos((lon2 - lon1) *
          3.14159265 / 180.0);

  float brng =
      atan2(y, x) *
      180.0 / 3.14159265;

  if (brng < 0)
    brng += 360.0;

  return brng;
}

/* =========================================================
   ANGLE DIFFERENCE
   ========================================================= */

float angleDifference(
    float target,
    float current)
{
  float diff = target - current;

  while (diff > 180)
    diff -= 360;

  while (diff < -180)
    diff += 360;

  return diff;
}

/* =========================================================
   OBSTACLE MEMORY
   ========================================================= */

void cleanBlockedMemory()
{
  unsigned long now = 0;

  /*
     AVR uptime approximation.
     This is intentionally simple.
  */

  for (int i = 0; i < blockedCount;)
  {
    if (now - blockedTime[i] >
        BLOCK_MEMORY_TIME)
    {
      for (int j = i; j < blockedCount - 1; j++)
      {
        blockedHeading[j] =
            blockedHeading[j + 1];

        blockedTime[j] =
            blockedTime[j + 1];
      }

      blockedCount--;
    }
    else
    {
      i++;
    }

    now += 100;
  }
}

void rememberBlocked(float heading)
{
  if (blockedCount >= MAX_BLOCKED)
    return;

  /*
     Don't duplicate nearly identical headings.
  */

  for (int i = 0; i < blockedCount; i++)
  {
    if (fabs(
          angleDifference(
              heading,
              blockedHeading[i])
        ) < 20)
    {
      return;
    }
  }

  blockedHeading[blockedCount] = heading;
  blockedTime[blockedCount] = 0;

  blockedCount++;
}

bool headingBlocked(float heading)
{
  for (int i = 0; i < blockedCount; i++)
  {
    if (
      fabs(
        angleDifference(
          heading,
          blockedHeading[i])
      ) < 30
    )
    {
      return true;
    }
  }

  return false;
}

/* =========================================================
   CHOOSE ALTERNATE HEADING
   ========================================================= */

float chooseAlternateHeading()
{
  float candidates[8];

  candidates[0] =
      compassHeading + 90;

  candidates[1] =
      compassHeading - 90;

  candidates[2] =
      compassHeading + 135;

  candidates[3] =
      compassHeading - 135;

  candidates[4] =
      compassHeading + 180;

  candidates[5] =
      compassHeading + 45;

  candidates[6] =
      compassHeading - 45;

  candidates[7] =
      compassHeading;

  for (int i = 0; i < 8; i++)
  {
    while (candidates[i] < 0)
      candidates[i] += 360;

    while (candidates[i] >= 360)
      candidates[i] -= 360;

    if (!headingBlocked(candidates[i]))
      return candidates[i];
  }

  return compassHeading + 180;
}

/* =========================================================
   MPU6050 GYRO CALIBRATION
   ========================================================= */

void calibrateGyro()
{
  long total = 0;

  for (int i = 0; i < 100; i++)
  {
    readMPU();

    total += mpuGz;

    _delay_ms(5);
  }

  gyroBiasZ =
      total / 100;
}

/* =========================================================
   MPU6050 CONTROLLED TURN
   ========================================================= */

void turnToHeading(
    float target)
{
  float startHeading =
      compassHeading;

  float angleTurned = 0;

  unsigned long timeout = 0;

  float previousHeading =
      compassHeading;

  float gyroAngle = 0;

  /*
     Decide direction from shortest path.
  */

  float diff =
      angleDifference(
          target,
          compassHeading);

  if (diff > 0)
    rightTurn();
  else
    leftTurn();

  while (fabs(diff) > 7 &&
         timeout < 1800)
  {
    readMPU();

    float gyroRate =
        ((float)mpuGz -
         (float)gyroBiasZ) / 131.0;

    /*
       10 ms approximately.
    */

    gyroAngle +=
        gyroRate * 0.01;

    _delay_ms(10);

    if (compassType != 0)
      compassHeading =
          readCompass();

    diff =
        angleDifference(
            target,
            compassHeading);

    timeout += 10;

    /*
       Safety tilt
    */

    readADXL();

    float adxlTilt =
        getADXLTilt();

    float mpuTilt =
        getMPUTilt();

    if (
      adxlTilt > MAX_TILT_DEG ||
      mpuTilt > MAX_TILT_DEG
    )
    {
      stopMotors();
      emergencyStop = true;
      return;
    }
  }

  stopMotors();

  _delay_ms(100);
}

/* =========================================================
   MOVEMENT CHECK
   ========================================================= */

bool movementCheck()
{
  /*
     This does NOT claim to be an odometer.

     It checks whether acceleration/rotation changes
     after the rover is commanded to move.
  */

  readMPU();
  readADXL();

  int16_t oldAx = adxlX;
  int16_t oldAy = adxlY;
  int16_t oldAz = adxlZ;

  int16_t oldGz = mpuGz;

  _delay_ms(400);

  readMPU();
  readADXL();

  long accelChange =
      labs((long)adxlX - oldAx) +
      labs((long)adxlY - oldAy) +
      labs((long)adxlZ - oldAz);

  long gyroChange =
      labs((long)mpuGz - oldGz);

  /*
     Very small change may mean:
     - rover stuck
     - rover blocked
     - rover on a surface with little acceleration

     Therefore this is only a warning-level detector.
  */

  if (
    accelChange < 35 &&
    gyroChange < 30
  )
  {
    return false;
  }

  return true;
}

/* =========================================================
   STABILITY CHECK
   ========================================================= */

bool stabilityOK()
{
  readMPU();
  readADXL();

  float mpuTilt =
      getMPUTilt();

  float adxlTilt =
      getADXLTilt();

  /*
     Both sensors must agree that the rover is
     in a dangerous tilt before emergency stop.
  */

  if (
    mpuTilt > MAX_TILT_DEG &&
    adxlTilt > MAX_TILT_DEG
  )
  {
    return false;
  }

  return true;
}

/* =========================================================
   OBSTACLE AVOIDANCE
   ========================================================= */

void avoidObstacle()
{
  stopMotors();

  _delay_ms(80);

  /*
     Save direction where obstacle was found.
  */

  if (compassType != 0)
  {
    compassHeading =
        readCompass();

    rememberBlocked(
        compassHeading);
  }

  /*
     Reverse away from obstacle.
  */

  backward();

  _delay_ms(180);

  stopMotors();

  _delay_ms(70);

  /*
     Choose another direction.
  */

  float newHeading =
      chooseAlternateHeading();

  /*
     Make sure we don't turn toward
     a remembered blocked direction.
  */

  if (headingBlocked(newHeading))
  {
    newHeading += 90;

    while (newHeading >= 360)
      newHeading -= 360;
  }

  /*
     Turn using compass + MPU6050.
  */

  if (compassType != 0)
  {
    turnToHeading(newHeading);
  }
  else
  {
    /*
       No compass fallback.
    */

    rightTurn();

    _delay_ms(450);

    stopMotors();
  }

  /*
     Recheck obstacle.
  */

  unsigned int d =
      getDistance();

  if (d != 0 && d <= 10)
  {
    /*
       Current direction is also blocked.
    */

    if (compassType != 0)
    {
      rememberBlocked(
          compassHeading);

      float secondHeading =
          chooseAlternateHeading();

      turnToHeading(secondHeading);
    }
    else
    {
      leftTurn();

      _delay_ms(550);

      stopMotors();
    }
  }
}

/* =========================================================
   GPS NAVIGATION
   ========================================================= */

void gpsNavigation()
{
  if (!gpsFix)
    return;

  float distance =
      distanceMeters(
          gpsLat,
          gpsLon,
          TARGET_LAT,
          TARGET_LON);

  if (distance < 3.0)
  {
    stopMotors();
    return;
  }

  float targetBearing =
      bearingTo(
          gpsLat,
          gpsLon,
          TARGET_LAT,
          TARGET_LON);

  desiredHeading =
      targetBearing;

  if (compassType == 0)
  {
    /*
       No compass:
       cannot accurately steer to GPS bearing.
    */

    forward();
    return;
  }

  compassHeading =
      readCompass();

  float difference =
      angleDifference(
          desiredHeading,
          compassHeading);

  /*
     Large heading error
     → turn.
  */

  if (difference > 18)
  {
    rightTurn();
    _delay_ms(70);
    stopMotors();
  }
  else if (difference < -18)
  {
    leftTurn();
    _delay_ms(70);
    stopMotors();
  }
  else
  {
    forward();
  }
}

/* =========================================================
   GPS-LOSS NAVIGATION
   ========================================================= */

void fallbackNavigation()
{
  /*
     GPS unavailable.

     Use compass to maintain the last desired heading.
  */

  if (compassType != 0)
  {
    compassHeading =
        readCompass();

    float difference =
        angleDifference(
            desiredHeading,
            compassHeading);

    if (difference > 20)
    {
      rightTurn();
      _delay_ms(60);
      stopMotors();
    }
    else if (difference < -20)
    {
      leftTurn();
      _delay_ms(60);
      stopMotors();
    }
    else
    {
      forward();
    }
  }
  else
  {
    /*
       No GPS + no compass:
       safest fallback is forward obstacle avoidance.
    */

    forward();
  }
}

/* =========================================================
   SETUP PINS
   ========================================================= */

void setupPins()
{
  /*
     HC-SR04
  */

  DDRD |= (1 << TRIG_BIT);
  DDRD &= ~(1 << ECHO_BIT);

  /*
     GPS RX
  */

  DDRD &= ~(1 << GPS_RX_BIT);
  PORTD |= (1 << GPS_RX_BIT);

  /*
     Motor pins
  */

  DDRD |= (1 << IN1_BIT);

  DDRB |=
      (1 << IN2_BIT) |
      (1 << IN3_BIT) |
      (1 << IN4_BIT);

  /*
     I2C pull-ups
  */

  SDA_HIGH();
  SCL_HIGH();

  stopMotors();
}

/* =========================================================
   MAIN
   ========================================================= */

int main()
{
  setupPins();

  _delay_ms(1000);

  /*
     Initialize sensors
  */

  bool mpuOK =
      initMPU();

  bool adxlOK =
      initADXL();

  bool compassOK =
      initCompass();

  /*
     GPS starts automatically.
  */

  /*
     Calibrate MPU6050 while rover is stationary.
     DO NOT MOVE ROVER DURING STARTUP.
  */

  if (mpuOK)
    calibrateGyro();

  /*
     Initial compass
  */

  if (compassOK)
    compassHeading =
        readCompass();

  /*
     Initial desired direction.
     If GPS has not yet fixed, use compass direction.
  */

  desiredHeading =
      compassHeading;

  /*
     Main autonomous loop
  */

  while (1)
  {
    /* ==========================================
       READ ALL SENSORS
       ========================================== */

    readMPU();
    readADXL();

    if (compassType != 0)
      compassHeading =
          readCompass();

    unsigned int distance =
        getDistance();

    /* ==========================================
       SAFETY CHECK
       ========================================== */

    float mpuTilt =
        getMPUTilt();

    float adxlTilt =
        getADXLTilt();

    /*
       Require both sensors to report severe tilt.
    */

    if (
      mpuTilt > MAX_TILT_DEG &&
      adxlTilt > MAX_TILT_DEG
    )
    {
      stopMotors();

      emergencyStop = true;

      /*
         Stay stopped.
      */

      while (1)
      {
        readMPU();
        readADXL();

        /*
           Only recover after rover becomes
           reasonably level again.
        */

        float mTilt =
            getMPUTilt();

        float aTilt =
            getADXLTilt();

        if (
          mTilt < 25 &&
          aTilt < 25
        )
        {
          emergencyStop = false;
          break;
        }

        _delay_ms(100);
      }
    }

    /* ==========================================
       OBSTACLE CHECK
       ========================================== */

    if (
      distance != 0 &&
      distance <= 10
    )
    {
      avoidObstacle();

      continue;
    }

    /* ==========================================
       GPS
       ========================================== */

    /*
       GPS parser may take time depending on
       the GPS sentence timing.
    */

    bool newGPS =
        parseGPS();

    if (newGPS)
    {
      gpsFix = true;

      gpsWasGood = true;

      lastGoodLat =
          gpsLat;

      lastGoodLon =
          gpsLon;

      /*
         Recalculate desired GPS heading.
      */

      desiredHeading =
          bearingTo(
              gpsLat,
              gpsLon,
              TARGET_LAT,
              TARGET_LON);
    }
    else
    {
      gpsFix = false;
    }

    /* ==========================================
       NAVIGATION
       ========================================== */

    if (gpsFix)
    {
      gpsNavigation();
    }
    else
    {
      /*
         GPS lost.

         DO NOT STOP.

         Use compass + MPU6050 +
         ADXL345 + HC-SR04.
      */

      fallbackNavigation();
    }

    /* ==========================================
       MOVEMENT VERIFICATION
       ========================================== */

    /*
       Only perform movement check when
       rover is supposed to be moving.
    */

    if (!emergencyStop)
    {
      /*
         Short verification.
      */

      bool moving =
          movementCheck();

      if (!moving)
      {
        /*
           Rover may be stuck.

           Stop and try a different direction.
        */

        stopMotors();

        _delay_ms(100);

        if (compassType != 0)
        {
          /*
             Remember current heading as
             temporarily unsuccessful.
          */

          rememberBlocked(
              compassHeading);

          float alternative =
              chooseAlternateHeading();

          turnToHeading(
              alternative);
        }
        else
        {
          /*
             No compass fallback.
          */

          rightTurn();

          _delay_ms(400);

          stopMotors();
        }
      }
    }

    /*
       Small delay.
    */

    _delay_ms(30);
  }

  return 0;
}
