// Ramsey 250QM - motosiklet elektrik tesisatı simülasyonu
// Kart: ESP32 DevKit V1 (Wokwi)
//
// Kontak, marş, motor durdurma (kill), far (kısa/uzun), stop/arka lamba,
// sol/sağ sinyal, dörtlü flaşör, korna ve OLED gösterge paneli.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Çıkışlar ----------
const int PIN_LOW_BEAM  = 18;  // kısa far (PWM)
const int PIN_HIGH_BEAM = 19;  // uzun far
const int PIN_TAIL      = 23;  // arka lamba + stop (PWM: sönük / tam parlak)
const int PIN_LEFT      = 4;   // sol ön + sol arka sinyal
const int PIN_RIGHT     = 5;   // sağ ön + sağ arka sinyal
const int PIN_HORN      = 15;  // korna (buzzer)
const int PIN_STARTER   = 2;   // marş rölesi

// ---------- Girişler (anahtar GND'ye çeker, aktif LOW) ----------
const int PIN_KEY        = 13;  // kontak anahtarı
const int PIN_KILL       = 14;  // motor durdurma anahtarı (LOW = RUN)
const int PIN_START_BTN  = 27;  // marş butonu
const int PIN_HORN_BTN   = 26;  // korna butonu
const int PIN_LEFT_BTN   = 25;  // sol sinyal
const int PIN_RIGHT_BTN  = 33;  // sağ sinyal
const int PIN_HAZARD_BTN = 16;  // dörtlü flaşör
const int PIN_HIGH_SW    = 17;  // uzun far anahtarı
const int PIN_BRAKE      = 36;  // fren müşiri (harici 10k pull-up)
const int PIN_NEUTRAL    = 39;  // boş vites müşiri (harici 10k pull-up)

// ---------- Analog sensörler ----------
const int PIN_THROTTLE = 34;  // gaz kolu (TPS)
const int PIN_FUEL     = 35;  // yakıt şamandırası
const int PIN_TEMP     = 32;  // motor sıcaklığı

// ---------- Ayarlar ----------
const int IDLE_RPM          = 1400;
const int MAX_RPM           = 10500;
const int MAX_SPEED_KMH     = 140;
const unsigned long CRANK_MS    = 1200;  // motorun ilk çalışması için marş süresi
const unsigned long BLINK_MS    = 330;   // ~90 flaş/dk
const unsigned long DEBOUNCE_MS = 30;
const int TAIL_DIM    = 40;
const int HORN_FREQ   = 420;
const int LOW_FUEL_PCT = 15;
const int HOT_TEMP_C   = 105;

Adafruit_SSD1306 display(128, 64, &Wire, -1);

enum EngineState { ENGINE_OFF, ENGINE_CRANKING, ENGINE_RUNNING };
EngineState engine = ENGINE_OFF;
unsigned long crankStart = 0;

struct Button {
  int pin;
  bool stable;
  bool lastRead;
  unsigned long changedAt;
};

Button leftBtn   = {PIN_LEFT_BTN, true, true, 0};
Button rightBtn  = {PIN_RIGHT_BTN, true, true, 0};
Button hazardBtn = {PIN_HAZARD_BTN, true, true, 0};

bool leftOn = false;
bool rightOn = false;
bool hazardOn = false;
bool hornOn = false;
bool lastKeyOn = false;

float rpm = 0;
float speedKmh = 0;
unsigned long lastUpdate = 0;
unsigned long lastDisplay = 0;

// Butonun basıldığı ana (düşen kenar) true döner
bool pressed(Button &b) {
  bool r = digitalRead(b.pin);
  unsigned long now = millis();
  if (r != b.lastRead) {
    b.lastRead = r;
    b.changedAt = now;
  }
  if (now - b.changedAt > DEBOUNCE_MS && r != b.stable) {
    b.stable = r;
    return b.stable == LOW;
  }
  return false;
}

bool isLow(int pin) { return digitalRead(pin) == LOW; }

int readPercent(int pin) { return map(analogRead(pin), 0, 4095, 0, 100); }

void setHorn(bool on) {
  if (on == hornOn) return;
  hornOn = on;
  if (on) tone(PIN_HORN, HORN_FREQ);
  else noTone(PIN_HORN);
}

void allOff() {
  analogWrite(PIN_LOW_BEAM, 0);
  digitalWrite(PIN_HIGH_BEAM, LOW);
  analogWrite(PIN_TAIL, 0);
  digitalWrite(PIN_LEFT, LOW);
  digitalWrite(PIN_RIGHT, LOW);
  digitalWrite(PIN_STARTER, LOW);
  setHorn(false);
}

void updateEngine(bool keyOn, bool killRun, bool startHeld, bool canStart) {
  unsigned long now = millis();
  if (!keyOn || !killRun) {
    engine = ENGINE_OFF;
    return;
  }
  switch (engine) {
    case ENGINE_OFF:
      if (startHeld && canStart) {
        engine = ENGINE_CRANKING;
        crankStart = now;
        Serial.println("Mars basladi");
      }
      break;
    case ENGINE_CRANKING:
      if (!startHeld) {
        engine = ENGINE_OFF;
        Serial.println("Mars birakildi, motor calismadi");
      } else if (now - crankStart >= CRANK_MS) {
        engine = ENGINE_RUNNING;
        Serial.println("Motor calisiyor");
      }
      break;
    case ENGINE_RUNNING:
      break;
  }
}

void updateRpmAndSpeed(int throttlePct, bool neutral, bool brake) {
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 1000.0;
  lastUpdate = now;

  float targetRpm = 0;
  if (engine == ENGINE_CRANKING) targetRpm = 300;
  if (engine == ENGINE_RUNNING) targetRpm = IDLE_RPM + (MAX_RPM - IDLE_RPM) * throttlePct / 100.0;
  rpm += (targetRpm - rpm) * min(1.0f, dt * 4);

  float targetSpeed = 0;
  if (engine == ENGINE_RUNNING && !neutral) targetSpeed = MAX_SPEED_KMH * throttlePct / 100.0;
  float rate = brake ? 3.0 : (targetSpeed > speedKmh ? 0.8 : 0.4);
  speedKmh += (targetSpeed - speedKmh) * min(1.0f, dt * rate);
  if (speedKmh < 0.5) speedKmh = 0;
}

void updateSignals(bool keyOn) {
  if (pressed(leftBtn) && keyOn) {
    leftOn = !leftOn;
    rightOn = false;
  }
  if (pressed(rightBtn) && keyOn) {
    rightOn = !rightOn;
    leftOn = false;
  }
  if (pressed(hazardBtn) && keyOn) hazardOn = !hazardOn;

  if (!keyOn) leftOn = rightOn = hazardOn = false;

  bool phase = (millis() / BLINK_MS) % 2 == 0;
  digitalWrite(PIN_LEFT, (leftOn || hazardOn) && phase);
  digitalWrite(PIN_RIGHT, (rightOn || hazardOn) && phase);
}

void drawDashboard(int throttlePct, int fuelPct, int tempC, bool neutral, bool highBeam, bool killRun, bool needNeutral) {
  bool phase = (millis() / BLINK_MS) % 2 == 0;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Üst satır: sinyaller, uzun far, vites
  display.setTextSize(1);
  if ((leftOn || hazardOn) && phase) {
    display.fillTriangle(0, 4, 8, 0, 8, 8, SSD1306_WHITE);
  }
  if ((rightOn || hazardOn) && phase) {
    display.fillTriangle(127, 4, 119, 0, 119, 8, SSD1306_WHITE);
  }
  if (highBeam) {
    display.setCursor(16, 1);
    display.print("HI");
  }
  if (neutral) {
    display.setCursor(40, 1);
    display.print("N");
  }
  display.setCursor(56, 1);
  if (!killRun) display.print("STOP");
  else if (engine == ENGINE_CRANKING) display.print("MARS");
  else if (engine == ENGINE_RUNNING) display.print("RUN");
  else display.print("OFF");

  // Hız
  display.setTextSize(3);
  display.setCursor(0, 14);
  display.printf("%3d", (int)speedKmh);
  display.setTextSize(1);
  display.setCursor(56, 28);
  display.print("km/h");

  // Sıcaklık ve yakıt
  display.setCursor(90, 14);
  display.printf("%3dC", tempC);
  display.setCursor(90, 26);
  display.printf("F%3d%%", fuelPct);

  // Devir göstergesi
  int barW = map((int)rpm, 0, 11000, 0, 126);
  display.drawRect(0, 40, 128, 8, SSD1306_WHITE);
  display.fillRect(1, 41, constrain(barW, 0, 126), 6, SSD1306_WHITE);
  display.setCursor(0, 50);
  display.printf("%5d rpm", (int)rpm);

  // Uyarılar
  display.setCursor(64, 56);
  if (needNeutral && phase) display.print("BOSA AL!");
  else if (fuelPct < LOW_FUEL_PCT && phase) display.print("YAKIT!");
  else if (tempC > HOT_TEMP_C && phase) display.print("SICAK!");

  display.display();
}

void setup() {
  Serial.begin(115200);

  int outputs[] = {PIN_HIGH_BEAM, PIN_LEFT, PIN_RIGHT, PIN_STARTER};
  for (int p : outputs) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
  int pullups[] = {PIN_KEY, PIN_KILL, PIN_START_BTN, PIN_HORN_BTN, PIN_LEFT_BTN,
                   PIN_RIGHT_BTN, PIN_HAZARD_BTN, PIN_HIGH_SW};
  for (int p : pullups) pinMode(p, INPUT_PULLUP);
  pinMode(PIN_BRAKE, INPUT);    // GPIO36/39 dahili pull-up desteklemez
  pinMode(PIN_NEUTRAL, INPUT);

  analogReadResolution(12);
  allOff();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 bulunamadi");
  }
  display.clearDisplay();
  display.display();

  lastUpdate = millis();
  Serial.println("Ramsey 250QM elektrik sistemi hazir");
}

void loop() {
  bool keyOn     = isLow(PIN_KEY);
  bool killRun   = isLow(PIN_KILL);
  bool startHeld = isLow(PIN_START_BTN);
  bool hornHeld  = isLow(PIN_HORN_BTN);
  bool highBeam  = isLow(PIN_HIGH_SW);
  bool brake     = isLow(PIN_BRAKE);
  bool neutral   = isLow(PIN_NEUTRAL);

  int throttlePct = readPercent(PIN_THROTTLE);
  int fuelPct     = readPercent(PIN_FUEL);
  int tempC       = map(analogRead(PIN_TEMP), 0, 4095, 0, 130);

  if (keyOn != lastKeyOn) {
    lastKeyOn = keyOn;
    Serial.println(keyOn ? "Kontak ACIK" : "Kontak KAPALI");
    if (!keyOn) {
      engine = ENGINE_OFF;
      allOff();
      display.clearDisplay();
      display.display();
    }
  }

  // Marş yalnızca boş viteste (gerçek motorda: boşta veya debriyaj çekili)
  bool needNeutral = startHeld && !neutral && engine == ENGINE_OFF;
  updateEngine(keyOn, killRun, startHeld, neutral);
  updateRpmAndSpeed(throttlePct, neutral, brake);
  updateSignals(keyOn);

  if (!keyOn) return;

  // Marş rölesi
  digitalWrite(PIN_STARTER, engine == ENGINE_CRANKING);

  // Farlar: motor çalışırken otomatik yanar, marş sırasında söner
  bool headlight = engine == ENGINE_RUNNING;
  analogWrite(PIN_LOW_BEAM, headlight && !highBeam ? 255 : 0);
  digitalWrite(PIN_HIGH_BEAM, headlight && highBeam);

  // Arka lamba / stop
  analogWrite(PIN_TAIL, brake ? 255 : TAIL_DIM);

  // Korna
  setHorn(hornHeld);

  if (millis() - lastDisplay >= 100) {
    lastDisplay = millis();
    drawDashboard(throttlePct, fuelPct, tempC, neutral, highBeam, killRun, needNeutral);
  }
}
