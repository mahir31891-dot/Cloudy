#include <MCUFRIEND_kbv.h>

MCUFRIEND_kbv tft;

// ============================================================
// SMOOTH ROBOT EYES
// Arduino UNO + 2.4" MCUFRIEND TFT
// No full-screen redraw
// ============================================================

#define BLACK  0x0000
#define CYAN   0x07FF
#define WHITE  0xFFFF

// ------------------------------------------------------------
// Screen
// ------------------------------------------------------------

int W, H;

// ------------------------------------------------------------
// Eye settings
// ------------------------------------------------------------

int leftX;
int rightX;
int eyeY;

const int EYE_W = 72;
const int EYE_H = 100;

const int PUPIL_R = 20;

// ------------------------------------------------------------
// Pupil positions
// ------------------------------------------------------------

int pupilX = 0;
int targetX = 0;

// Previous pupil positions
int oldPupilX = 0;

// ------------------------------------------------------------
// Timing
// ------------------------------------------------------------

unsigned long lastLook = 0;
unsigned long nextLook = 1800;

unsigned long lastBlink = 0;
unsigned long nextBlink = 4000;

// ------------------------------------------------------------
// Blink
// ------------------------------------------------------------

bool blinking = false;
int blinkStep = 0;

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(9600);

  uint16_t ID = tft.readID();

  Serial.print("TFT ID: 0x");
  Serial.println(ID, HEX);

  if (ID == 0xD3D3 || ID == 0xFFFF || ID == 0x0000)
  {
    ID = 0x9341;
  }

  tft.begin(ID);

  // Landscape
  tft.setRotation(1);

  W = tft.width();
  H = tft.height();

  leftX  = W / 2 - 55;
  rightX = W / 2 + 55;

  eyeY = H / 2;

  randomSeed(analogRead(A5));

  // Black background ONCE
  tft.fillScreen(BLACK);

  // Draw eyes ONCE
  drawEye(leftX);
  drawEye(rightX);

  // Draw pupils
  drawPupil(leftX, pupilX);
  drawPupil(rightX, pupilX);
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  unsigned long now = millis();

  // ==========================================================
  // CHOOSE NEW LOOK DIRECTION
  // ==========================================================

  if (!blinking && now - lastLook >= nextLook)
  {
    lastLook = now;

    int r = random(0, 3);

    if (r == 0)
      targetX = -18;

    else if (r == 1)
      targetX = 0;

    else
      targetX = 18;

    nextLook = random(1000, 2500);
  }

  // ==========================================================
  // SMOOTH PUPIL MOVEMENT
  // ==========================================================

  if (!blinking)
  {
    if (pupilX < targetX)
      pupilX++;

    if (pupilX > targetX)
      pupilX--;

    if (pupilX != oldPupilX)
    {
      movePupils(oldPupilX, pupilX);

      oldPupilX = pupilX;
    }
  }

  // ==========================================================
  // BLINK START
  // ==========================================================

  if (!blinking && now - lastBlink >= nextBlink)
  {
    blinking = true;
    blinkStep = 0;

    lastBlink = now;
  }

  // ==========================================================
  // BLINK ANIMATION
  // ==========================================================

  if (blinking)
  {
    blinkStep += 8;

    if (blinkStep >= EYE_H)
    {
      blinkStep = EYE_H;

      delay(60);

      // Open eye again
      blinkStep = 0;

      blinking = false;

      // Restore eyes
      drawEye(leftX);
      drawEye(rightX);

      drawPupil(leftX, pupilX);
      drawPupil(rightX, pupilX);

      nextBlink = random(3000, 7000);
      lastBlink = millis();
    }
    else
    {
      drawBlink(leftX, blinkStep);
      drawBlink(rightX, blinkStep);
    }
  }

  // Small delay gives smooth motion
  delay(12);
}

// ============================================================
// DRAW STATIC EYE
// ============================================================

void drawEye(int x)
{
  int top = eyeY - EYE_H / 2;

  // Outer cyan eye
  tft.fillRoundRect(
    x - EYE_W / 2,
    top,
    EYE_W,
    EYE_H,
    17,
    CYAN
  );

  // Black inside
  tft.fillRoundRect(
    x - EYE_W / 2 + 8,
    top + 8,
    EYE_W - 16,
    EYE_H - 16,
    12,
    BLACK
  );
}

// ============================================================
// DRAW PUPIL
// ============================================================

void drawPupil(int x, int offset)
{
  tft.fillCircle(
    x + offset,
    eyeY,
    PUPIL_R,
    CYAN
  );

  // Black center
  tft.fillCircle(
    x + offset,
    eyeY,
    10,
    BLACK
  );

  // Small white highlight
  tft.fillCircle(
    x + offset - 5,
    eyeY - 7,
    3,
    WHITE
  );
}

// ============================================================
// MOVE PUPILS
// Only redraw small pupil areas
// ============================================================

void movePupils(int oldPos, int newPos)
{
  // Erase old pupil area by restoring eye background
  restorePupilArea(leftX, oldPos);
  restorePupilArea(rightX, oldPos);

  // Draw new pupils
  drawPupil(leftX, newPos);
  drawPupil(rightX, newPos);
}

// ============================================================
// RESTORE AREA BEHIND PUPIL
// ============================================================

void restorePupilArea(int x, int offset)
{
  // Restore black area around old pupil
  tft.fillCircle(
    x + offset,
    eyeY,
    PUPIL_R + 2,
    BLACK
  );
}

// ============================================================
// BLINK
// ============================================================

void drawBlink(int x, int amount)
{
  int top = eyeY - EYE_H / 2;
  int bottom = eyeY + EYE_H / 2;

  // Black out upper/lower portions gradually
  int half = amount / 2;

  if (half > 0)
  {
    tft.fillRect(
      x - EYE_W / 2,
      top,
      EYE_W,
      half,
      BLACK
    );

    tft.fillRect(
      x - EYE_W / 2,
      bottom - half,
      EYE_W,
      half,
      BLACK
    );
  }

  // When nearly closed, draw a thin cyan line
  if (amount > EYE_H - 20)
  {
    tft.fillRect(
      x - EYE_W / 2 + 5,
      eyeY - 3,
      EYE_W - 10,
      6,
      CYAN
    );
  }
}
