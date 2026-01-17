#include <MX1508.h>
#include <EEPROM.h>

/* =====================  MOTORS & IO  ===================== */

MX1508 bodyMotor(5, 3);   
MX1508 mouthMotor(9, 6);

const int soundPin = A0;

int fishState = 0;
bool talking = false;
bool headIsIdle = true;

/* =====================  SIGNAL PROCESSING  ===================== */
int raw = 0;
float baseline = 577.6f;     // will be calibrated
float envelope = 0.0f;

const unsigned long CALIBRATION_MS = 1500;
const float BASELINE_ALPHA_SLOW = 0.002f;
const float BASELINE_ALPHA_FAST = 0.02f;
const float ATTACK_ALPHA = 0.6f;
const float RELEASE_ALPHA = 0.08f;   // faster decay, so silence is recognized quicker

// Thresholds
int TH_LOW  = 18;   // stop talking below this
int TH_HIGH = 27;   // start talking above this

// Map envelope → PWM speed
const int MOUTH_SPEED_MIN = 160;
const int MOUTH_SPEED_MAX = 255;

/* =====================  TIMERS / STATE  ===================== */
unsigned long currentTime;
unsigned long bootTime;
bool calibrated = false;
unsigned long silenceStartMs = 0;

/* =====================  BOREDOM FLAP (TAIL ONLY WHEN SILENT)  ===================== */
bool boredomFlapEnabled = true;   // set to false to disable tail flap completely
bool flapActive = false;
unsigned long flapEndMs = 0;
unsigned long nextBoredMs = 0;
unsigned long quietSince = 0;  // when quiet started
const unsigned long MIN_IDLE_BEFORE_FLAP_MS = 5000;  // require sustained quiet
const unsigned long FLAP_DURATION_MS        = 300;   // one flap duration

/* =====================  SETUP  ===================== */
void setup() {
  bodyMotor.setSpeed(0);
  mouthMotor.setSpeed(0);
  pinMode(soundPin, INPUT);
  Serial.begin(9600);

  calibrateBaseline();

  // schedule first boredom opportunity a little later
  nextBoredMs = bootTime + 3000;
}

/* =====================  LOOP  ===================== */
void loop() {
  currentTime = millis();
  raw = analogRead(soundPin); //read & process input

  calibrateUpdate(currentTime, raw);

  float level = fabs((float)raw - baseline);
  if (level > envelope) envelope += ATTACK_ALPHA  * (level - envelope);
  else                  envelope += RELEASE_ALPHA * (level - envelope);
  
  // ------- mouth/talking behavior only when not actively flapping -------
  SMBillyBass(envelope);

  // DEBUG
  if ((currentTime % 200) < 5) {
    Serial.print("raw=");   Serial.print(raw);
    Serial.print("  base="); Serial.print(baseline, 1);
    Serial.print("  env=");  Serial.print(envelope, 1);
    Serial.print("  talk="); Serial.print(talking ? "1" : "0");
    Serial.print("  flap="); Serial.println(flapActive ? "1" : "0");    
  }
}

/* =====================  BEHAVIOR  ===================== */
void SMBillyBass(float envelope) {
  // ------- talking decision -------
  static bool talkingPrev = false;
  talking = decideTalking(talking, envelope);

  if (talkingPrev && !talking) {
    silenceStartMs = currentTime;       // started a new pause
  } else if (talking) {
    silenceStartMs = 0;                 // talking again, discard any silence timer
  }
  talkingPrev = talking;

  // ------- non-blocking tail flap scheduler (runs ONLY when not talking) -------
  handleBoredomFlap();

  if (flapActive) {
    return;
  }

  switch (fishState) {
    case 0: // WAITING
      if (talking) {
        fishState = 1;
      } else {
        // Do NOT halt the body motor here — flap logic controls bodyMotor.
        mouthMotor.halt();
        moveHeadToRest(talkingPrev); 
      }
      break;

    case 1: // TALKING
      {
        moveHeadForward();
        openMouth();
        if (!talking) {
          closeMouth(); // phrase ended: close mouth a bit
          fishState = 0;
        }
      }
      break;
  }
}

/* =====================  HELPERS  ===================== */
void openMouth() {
  int mouthSpeed = fmap(envelope, TH_LOW, TH_HIGH * 5, MOUTH_SPEED_MIN, MOUTH_SPEED_MAX);
  mouthSpeed = constrain(mouthSpeed, MOUTH_SPEED_MIN, MOUTH_SPEED_MAX);

  mouthSpeed = constrain(mouthSpeed, MOUTH_SPEED_MIN, MOUTH_SPEED_MAX);
  mouthMotor.halt();
  mouthMotor.setSpeed(mouthSpeed);
  mouthMotor.forward();
}

void closeMouth() {
  mouthMotor.halt();
  mouthMotor.setSpeed(180);
  mouthMotor.backward();
}

void moveHeadForward() {
  bodyMotor.halt();
  bodyMotor.setSpeed(180);
  bodyMotor.forward();

  headIsIdle = false;
}

void moveHeadToRest(bool talkingPrev) {
  // If we're talking, don't recenter
  if (talking) return;

  // If silence hasn't been started or it's not long enough, skip
  if (silenceStartMs == 0) return;
  if ((currentTime - silenceStartMs) < 700UL) return;
  
  // --- Perform "return to rest" movement here ---
  if (!headIsIdle) {
    headIsIdle = true;

    bodyMotor.halt();
    bodyMotor.setSpeed(190);
    bodyMotor.backward(); 
    bodyMotor.halt();
  }
}

bool decideTalking(bool prevTalking, float env) {
  if (!prevTalking && env > TH_HIGH) return true;
  if (prevTalking && env < TH_LOW) return false;
  return prevTalking;
}

void handleBoredomFlap() {
  // ------- quiet tracking for boredom (only when silent) -------
  if (!talking && envelope < TH_LOW) {
    if (quietSince == 0) quietSince = currentTime;
  } else {
    quietSince = 0; // reset on any noise/talking
  }
  
  delay(1); // the flap only works properly with this small delay
  
  // ------- non-blocking tail flap scheduler (runs ONLY when not talking) -------
  if (boredomFlapEnabled &&
      !talking && !flapActive && quietSince != 0 &&
      (currentTime - quietSince) > MIN_IDLE_BEFORE_FLAP_MS &&
      currentTime > nextBoredMs) {

    flapActive  = true;
    flapEndMs   = currentTime + FLAP_DURATION_MS;
    nextBoredMs = currentTime + (unsigned long)random(6000, 11000);
    Serial.println("[CMD] Start FLAP");

    bodyMotor.halt();
    bodyMotor.setSpeed(180);
    bodyMotor.backward();
  }

  // ------- while a flap is active, keep mouth closed and stop body at end -------
  if (flapActive) {
    closeMouth();  // reduce load during tail flap

    if ((long)(currentTime - flapEndMs) >= 0) {
      flapActive = false;
      bodyMotor.halt();
      Serial.println("[CMD] End FLAP");
    }
  }
}

void calibrateUpdate(unsigned long now, int raw) {
  if (!calibrated) {
    // During calibration window: fast baseline lock-in
    float alpha = BASELINE_ALPHA_FAST;
    baseline = (1.0f - alpha) * baseline + alpha * raw;

    if (now - bootTime > CALIBRATION_MS) {
      calibrated = true;
      Serial.print(F("Calibration done. Baseline = "));
      Serial.println(baseline, 1);
      delay(100); // (kept from your original code; can be removed if you want fully non-blocking)
    }
  } else {
    // After calibration: adaptive baseline (fast when quiet, slow when noisy)
    float quietAlpha = (envelope < TH_LOW * 0.7f) ? BASELINE_ALPHA_FAST : BASELINE_ALPHA_SLOW;
    baseline = (1.0f - quietAlpha) * baseline + quietAlpha * raw;
  }
}

void calibrateBaseline() {
  bootTime = millis();

  // Take an initial average to seed the baseline
  long sum = 0;
  for (int i = 0; i < 64; i++) {
    sum += analogRead(soundPin);
    delay(2); // small spacing to avoid instant burst noise bias
  }
  baseline  = sum / 64.0f;
  envelope  = 0.0f;   // start fresh
  calibrated = false;

  Serial.println(F("Booting… calibrating baseline."));
}

// helper
int fmap(float x, float in_min, float in_max, int out_min, int out_max) {
  if (x <= in_min) return out_min;
  if (x >= in_max) return out_max;
  float t = (x - in_min) / (in_max - in_min);
  return (int)(out_min + t * (out_max - out_min));
}

// best version
