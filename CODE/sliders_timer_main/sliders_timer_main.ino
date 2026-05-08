#include "LedControl.h"
#include "OneButton.h"
#include "Adafruit_NeoPixel.h"
#include "DFPlayer_Mini_Mp3.h"
#include <avr/wdt.h>
#include <EEPROM.h>

#define BP_START A0
#define BP_UP A1
#define BP_DOWN A2
#define ADAFRUITPIN 4
#define BP_VORTEX A5
#define BATTERYPIN A7

// --- Parametres audio / animation ---
const uint8_t  DFPLAYER_VOLUME       = 28;     // 0-30
const uint16_t START_INTRO_DELAY_MS  = 2500;   // ecrans noirs au demarrage
const uint16_t START_OUTRO_DELAY_MS  = 2500;   // attente apres l'animation pour synchro fin du clip
const uint16_t VORTEX_MP3_DURATION_MS = 8000;  // duree du 0001.mp3 (vortex)
const uint16_t COUNTDOWN_BIP_FREQ    = 3820;   // freq bip countdown (mesuree sur clip)
const uint16_t START_LOCK_TONE_FREQ  = 3000;   // freq bip de fin (3 sec / 1.5 sec)

// --- Parametres timer ---
const unsigned long SECONDS_IN_DAY    = 86400UL;
const unsigned long SECONDS_IN_HOUR   = 3600UL;
const unsigned long SECONDS_IN_MINUTE = 60UL;
const unsigned long VORTEX_RANDOM_MAX = 16756131UL;  // ~193 jours

// --- EEPROM ---
const int EEPROM_MAGIC_ADDR = 0;
const uint8_t EEPROM_MAGIC_VALUE = 0xA5;
const int EEPROM_TOTALSECTIME_SLIDE_ADDR = 1;

struct LED {
  int pinState;
  unsigned long previousMillis;
  unsigned long interval;
  int matrixIndex;
  bool isColon;
  int colIndex;
  bool isArduinoLED;
  int arduinoPin;
};

struct Bargraphe {
  const uint64_t *images;
  int length;
  int index;
  int delayTime;
};

// Format generique pour les animations 7-segments stockees en PROGMEM:
// 6 bytes matrix 0 (rows 0-5) + 5 bytes matrix 1 (rows 0-4)
// + tone optionnel (freq=0 -> pas de tone) + delay apres affichage.
struct AnimFrame {
  uint8_t r0[6];
  uint8_t r1[5];
  uint16_t tone_freq;  // 0 = pas de tone
  uint8_t tone_dur;    // duree du tone en ms
  uint8_t delay_ms;    // delay apres affichage
};

// Format pour genserSequence: pattern affiche-bip-attend-clear-attend.
struct GenserFrame {
  uint8_t r0[6];
  uint8_t r1[5];
  uint16_t hold_ms;        // delay avec frame affichee + tone
  uint8_t clear_pause_ms;  // delay apres clear
};

// Prototypes (necessaire car les arguments par defaut ne sont pas captures
// dans la forward declaration auto-generee par Arduino).
void reveille(bool fadeIn = true);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(7, ADAFRUITPIN, NEO_GRB + NEO_KHZ800);
int prevVal = 0;

// Lissage de la valeur du potentiomètre
const int numReadings = 10;
int readings[numReadings];
int readIndex = 0;
int total = 0;
int average = 0;

uint32_t Red = strip.Color(255, 0, 0);
uint32_t Off = strip.Color(0, 0, 0);

LedControl lc = LedControl(12, 11, 10, 2);

const int pot1 = A6;
const int speaker = 5;
const int etatarduino = 13;

OneButton button1(A4, true);
OneButton button2(A3, true);

volatile byte running = false;

unsigned long totalsectime_slide = 0;
unsigned long totalsectime = 0;

bool BP_START_STATUS = false;
bool OLD_BP_START_STATUS = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

bool BP_VORTEX_STATUS = false;
bool OLD_BP_VORTEX_STATUS = false;
unsigned long lastDebounceTime1 = 0;
unsigned long debounceDelay1 = 50;

bool BP_POWER_STATUS = false;

const float TensionMin = 3.2;
const float TensionMax = 4.2;

bool firstStartup = true;     // flag du 1er passage du double-click power (init musique + screens)
bool wrapPlayedAt10s = false; // flag pour rejouer displayWrap une seule fois quand burnout=10s
int menu = 0;
int buzzerState = 1;

void setup() {
  Serial.begin(9600);
  mp3_set_serial(Serial);
  mp3_set_volume(28);
  mp3_set_EQ(0);

  button1.attachClick(click1);
  button1.attachDoubleClick(doubleclick1);
  button1.attachDuringLongPress(longPress1);

  button2.attachClick(click2);
  button2.attachDoubleClick(doubleclick2);
  button2.attachDuringLongPress(longPress2);

  int devices = lc.getDeviceCount();
  for (int address = 0; address < devices; address++) {
    lc.shutdown(address, false);
    lc.setIntensity(address, 15);
    lc.clearDisplay(address);
    lc.setScanLimit(address, 7);
  }

  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }

  pinMode(speaker, OUTPUT);
  pinMode(etatarduino, OUTPUT);
  pinMode(pot1, INPUT);
  randomSeed(analogRead(pot1));

  pinMode(BP_UP, INPUT_PULLUP);
  pinMode(BP_DOWN, INPUT_PULLUP);
  pinMode(BP_START, INPUT_PULLUP);
  pinMode(BP_VORTEX, INPUT_PULLUP);
  attachInterrupt(1, buttonStart, FALLING);

  strip.begin();
  strip.show();

  // Restaure le dernier temps lock depuis EEPROM (si valide).
  // On ne restaure QUE totalsectime_slide; totalsectime reste a 0 sinon
  // le compteur considere le countdown comme deja termine.
  if (EEPROM.read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC_VALUE) {
    EEPROM.get(EEPROM_TOTALSECTIME_SLIDE_ADDR, totalsectime_slide);
  }

  // Disable watchdog (au cas ou on viendrait d'un reboot watchdog)
  wdt_disable();
}

LED redLED = { HIGH, 0, 600, 4, false, 0, false, -1 };
LED yellowLED = { HIGH, 0, 250, 3, false, 0, false, -1 };
LED greenLED = { HIGH, 0, 200, 5, false, 0, false, -1 };
LED colonLED = { HIGH, 0, 150, 0, true, 6, false, -1 };
LED etatArduinoLED = { HIGH, 0, 100, -1, false, 0, true, etatarduino };

void blinkLEDUnified(LED &led) {
  unsigned long currentMillis = millis();

  if (currentMillis - led.previousMillis >= led.interval) {
    led.previousMillis = currentMillis;

    led.pinState = !led.pinState;

    if (led.isColon) {
      lc.setLed(0, led.colIndex, 1, led.pinState);
      lc.setLed(0, led.colIndex, 2, led.pinState);
      lc.setLed(0, led.colIndex, 3, led.pinState);
      lc.setLed(0, led.colIndex, 4, led.pinState);
    } else if (!led.isColon && !led.isArduinoLED) {
      lc.setLed(1, 5, led.matrixIndex, led.pinState);
    } else if (led.isArduinoLED) {
      digitalWrite(led.arduinoPin, led.pinState);
    }
  }
}

void ecranBlinkUnified(bool allSegments, int bloc = 0, int ecran = 0) {
  static int stateecran = 0;
  static unsigned int ecrantime = 250;
  static unsigned int ecraninterval = 500;
  static unsigned long lasttimeecran = 0;
  unsigned long mecran = millis();

  if (stateecran == 0) {
    if (mecran >= lasttimeecran + ecraninterval) {
      stateecran = 1;
      showTime();
      lasttimeecran = mecran;
    }
    return;
  }
  if (stateecran == 1) {
    if (mecran >= lasttimeecran + ecrantime) {
      if (allSegments) {
        for (int i = 0; i < 8; i++) {
          lc.setRow(0, i, false);
          lc.setRow(1, i, false);
        }
      } else {
        lc.setRow(bloc, ecran, false);
      }

      stateecran = 0;
      ecrantime = 250;
    }
    return;
  }
}

void customTone(int frequency, int duration = 0) {
  if (buzzerState == 1) {
    if (duration > 0) {
      tone(speaker, frequency, duration);
    } else {
      tone(speaker, frequency);
    }
  } else {
    noTone(speaker);
  }
}

// Attente non-bloquante pour les boutons: equivalent a delay(ms) mais en
// continuant a appeler button.tick() pour ne pas perdre les clics utilisateur.
void waitMs(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    button1.tick();
    button2.tick();
  }
}

void updatespeaker_pattern(int interval, int numBips, int pause) {
  static int state = 0;
  static unsigned int beeptime = 50;
  static unsigned long lasttimeon = 0;
  static unsigned int bipCount = 0;
  static int lastInterval = -1;
  static int lastNumBips = -1;
  static int lastPause = -1;
  unsigned long m = millis();

  // Reset l'etat si le pattern change (sinon le bipCount/state d'un pattern
  // precedent peut donner un rythme bizarre quand on saute des paliers).
  if (interval != lastInterval || numBips != lastNumBips || pause != lastPause) {
    state = 0;
    bipCount = 0;
    lasttimeon = m;
    lastInterval = interval;
    lastNumBips = numBips;
    lastPause = pause;
  }

  if (bipCount >= numBips) {
    if (m >= lasttimeon + pause) {
      bipCount = 0;
      state = 0;
    }
    return;
  }

  if (state == 0) {
    if (m >= lasttimeon + interval) {
      state = 1;
      customTone(COUNTDOWN_BIP_FREQ);
      lasttimeon = m;
    }
    return;
  }

  if (state == 1) {
    if (m >= lasttimeon + beeptime) {
      noTone(speaker);
      state = 0;
      lasttimeon = m;
      bipCount++;
    }
    return;
  }
}

// Texte de version qui defile sur les ecrans 7-segments au demarrage (1er
// double-click power). Padding gauche/droite pour entree/sortie smooth.
const char VERSION_TEXT[] PROGMEM = "         sliders timer replica by kenny v1.2         ";

// Conversion d'un caractere ASCII en bitmap 7-segments (A-G, bit 6 = top).
byte charToSeg(char c) {
  switch (c) {
    case ' ': return 0x00;
    case '0': return 0x7E;
    case '1': return 0x30;
    case '2': return 0x6D;
    case '3': return 0x79;
    case '4': return 0x33;
    case '5': return 0x5B;
    case '6': return 0x5F;
    case '7': return 0x70;
    case '8': return 0x7F;
    case '9': return 0x7B;
    case 'a': return 0x77;
    case 'b': return 0x1F;
    case 'c': return 0x0D;
    case 'd': return 0x3D;
    case 'e': return 0x4F;
    case 'f': return 0x47;
    case 'h': return 0x17;
    case 'i': return 0x10;
    case 'k': return 0x37;
    case 'l': return 0x0E;
    case 'm': return 0x15;  // approximation (n minuscule)
    case 'n': return 0x15;
    case 'o': return 0x1D;
    case 'p': return 0x67;
    case 'r': return 0x05;
    case 's': return 0x5B;
    case 't': return 0x0F;
    case 'u': return 0x1C;
    case 'v': return 0x1C;  // approximation (u)
    case 'y': return 0x3B;
    case '.': return 0x80;  // decimal point seul
    case '-': return 0x01;
    default:  return 0x00;
  }
}

// Defile le texte VERSION_TEXT sur les 9 ecrans (6 sur matrix 0 + 3 sur matrix 1).
void scrollVersion() {
  int len = strlen_P(VERSION_TEXT);
  for (int pos = 0; pos <= len - 9; pos++) {
    for (int i = 0; i < 6; i++) {
      char c = pgm_read_byte(&VERSION_TEXT[pos + i]);
      lc.setRow(0, i, charToSeg(c));
    }
    for (int i = 0; i < 3; i++) {
      char c = pgm_read_byte(&VERSION_TEXT[pos + 6 + i]);
      lc.setRow(1, i, charToSeg(c));
    }
    delay(150);
  }
  lc.clearDisplay(0);
  lc.clearDisplay(1);
}

void startMusique() {
  // Joue le clip d'activation Sliders Timer via le DFPlayer Mini.
  // Fichier 0003.mp3 sur la carte SD.
  mp3_play_physical(3);  // 0003.mp3 = clip d'activation Sliders Timer
  waitMs(START_INTRO_DELAY_MS);  // ecrans noirs au debut du clip (effet d'intro)
}

// Frames de genserSequence: 8 ecrans textes/scrambled successifs avec bip.
// r1 dans l'ordre [0,1,2,3,4]. Tous les bips sont 5500 Hz / 50 ms.
const GenserFrame GENSER_FRAMES[] PROGMEM = {
  // GEnSEr
  {{0x5E, 0x4F, 0x15, 0x5B, 0x4F, 0x05}, {0x49, 0x03, 0x5E, 0x88, 0x88}, 400, 40},
  // random 1
  {{0x49, 0x15, 0x23, 0x03, 0x54, 0x43}, {0x05, 0x40, 0x40, 0xAA, 0x8A}, 200, 40},
  // CAluri
  {{0x4E, 0x77, 0x30, 0x1C, 0x05, 0x10}, {0x43, 0x08, 0x54, 0x82, 0x0A}, 200, 40},
  // 4th
  {{0x23, 0x03, 0x54, 0x43, 0x13, 0x19}, {0x30, 0x41, 0x4E, 0x89, 0x4A}, 200, 40},
  // Second CAluri
  {{0x4E, 0x77, 0x30, 0x1C, 0x05, 0x10}, {0x2A, 0x1A, 0x1A, 0x29, 0x0A}, 200, 40},
  // 6th
  {{0x2A, 0x03, 0x1A, 0x48, 0x41, 0x49}, {0x19, 0x09, 0x13, 0x29, 0x0A}, 200, 40},
  // 7th
  {{0x13, 0x19, 0x22, 0x18, 0x41, 0x25}, {0x33, 0x37, 0x89, 0x95, 0x24}, 200, 40},
  // KEnnY
  {{0x37, 0x4F, 0x15, 0x15, 0x33, 0x01}, {0x21, 0x11, 0x41, 0x89, 0x89}, 350, 75}
};

void genserSequence() {
  GenserFrame f;
  const uint8_t N = sizeof(GENSER_FRAMES) / sizeof(GENSER_FRAMES[0]);
  for (uint8_t i = 0; i < N; i++) {
    memcpy_P(&f, &GENSER_FRAMES[i], sizeof(GenserFrame));
    for (uint8_t r = 0; r < 6; r++) lc.setRow(0, r, f.r0[r]);
    for (uint8_t r = 0; r < 5; r++) lc.setRow(1, r, f.r1[r]);
    customTone(5500, 50);
    delay(f.hold_ms);
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    noTone(speaker);
    delay(f.clear_pause_ms);
  }
}

void renderAnimFrame(const AnimFrame *frame_pgm) {
  AnimFrame f;
  memcpy_P(&f, frame_pgm, sizeof(AnimFrame));
  if (f.tone_freq) customTone(f.tone_freq, f.tone_dur);
  for (uint8_t r = 0; r < 6; r++) lc.setRow(0, r, f.r0[r]);
  for (uint8_t r = 0; r < 5; r++) lc.setRow(1, r, f.r1[r]);
  if (f.delay_ms) delay(f.delay_ms);
}

const AnimFrame DISPLAY_FADE_FRAMES[] PROGMEM = {
  {{0x78, 0x7E, 0x7E, 0x7E, 0x7E, 0x1E}, {0x7F, 0x7F, 0x7F, 0xFF, 0xFF}, 0, 0, 30},
  {{0x30, 0x7E, 0x7E, 0x7E, 0x80, 0x06}, {0x3F, 0x3F, 0x3F, 0xEF, 0xEF}, 0, 0, 30},
  {{0x00, 0x7E, 0x7E, 0x7E, 0x7E, 0x00}, {0x2F, 0x2F, 0x2F, 0xE7, 0xE7}, 0, 0, 30},
  {{0x00, 0x78, 0x7E, 0x7E, 0x40, 0x00}, {0x2B, 0x2B, 0x2B, 0xC7, 0xC7}, 0, 0, 30},
  {{0x00, 0x30, 0x7E, 0x7E, 0x06, 0x00}, {0x0B, 0x0B, 0x0B, 0xC3, 0xC3}, 0, 0, 30},
  {{0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00}, {0x09, 0x09, 0x09, 0x83, 0x83}, 0, 0, 55},
  {{0x00, 0x00, 0x78, 0x40, 0x00, 0x00}, {0x01, 0x01, 0x01, 0x81, 0x81}, 0, 0, 30},
  {{0x00, 0x00, 0x30, 0x06, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x01, 0x01}, 0, 0, 30},
  {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00}, 0, 0,  0}
};

void displayFade() {
  delay(100);
  const uint8_t N = sizeof(DISPLAY_FADE_FRAMES) / sizeof(DISPLAY_FADE_FRAMES[0]);
  for (uint8_t i = 0; i < N; i++) {
    renderAnimFrame(&DISPLAY_FADE_FRAMES[i]);
  }
  delay(1000);
}

// Frames de displayWrap (effet "wrap" sci-fi). Reordonnees: r1 dans l'ordre [0,1,2,3,4].
// Pour les frames partielles du code original:
//   Frame 0 -> r1[0..2] = 0 (etat initial)
//   Frame 7 -> r1[0..2] herites de frame 6 (=0x40)
//   Frame 13 -> r0[0..5] herites de frame 12 (=0x02)
const AnimFrame DISPLAY_WRAP_FRAMES[] PROGMEM = {
  {{0x02, 0x02, 0x02, 0x02, 0x02, 0x02}, {0x00, 0x00, 0x00, 0x53, 0x5A}, 5500, 100, 30},
  {{0x40, 0x40, 0x40, 0x40, 0x40, 0x40}, {0x02, 0x02, 0x02, 0x69, 0x40}, 5500, 100, 30},
  {{0x20, 0x20, 0x20, 0x20, 0x20, 0x20}, {0x04, 0x04, 0x04, 0x25, 0x48}, 5500, 100, 30},
  {{0x10, 0x10, 0x10, 0x10, 0x10, 0x10}, {0x08, 0x08, 0x08, 0xA5, 0xD8}, 5500, 100, 30},
  {{0x08, 0x08, 0x08, 0x08, 0x08, 0x08}, {0x10, 0x10, 0x10, 0xA5, 0xDA}, 5500, 100, 30},
  {{0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, {0x20, 0x20, 0x20, 0x8D, 0xAA}, 5500, 100, 30},
  {{0x02, 0x02, 0x02, 0x02, 0x02, 0x02}, {0x40, 0x40, 0x40, 0x57, 0x36}, 5500, 100, 30},
  {{0x40, 0x40, 0x40, 0x40, 0x40, 0x40}, {0x40, 0x40, 0x40, 0x54, 0xB6}, 5500, 100, 30},
  {{0x20, 0x20, 0x20, 0x20, 0x20, 0x20}, {0x02, 0x02, 0x02, 0x44, 0x82}, 5500, 100, 30},
  {{0x10, 0x10, 0x10, 0x10, 0x10, 0x10}, {0x04, 0x04, 0x04, 0x55, 0x84}, 5500, 100, 30},
  {{0x08, 0x08, 0x08, 0x08, 0x08, 0x08}, {0x08, 0x08, 0x08, 0x25, 0x8D}, 2500, 100, 30},
  {{0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, {0x10, 0x10, 0x10, 0x57, 0xB6}, 2500, 100, 30},
  {{0x02, 0x02, 0x02, 0x02, 0x02, 0x02}, {0x20, 0x20, 0x20, 0x44, 0x48}, 2500, 100, 30},
  {{0x02, 0x02, 0x02, 0x02, 0x02, 0x02}, {0x40, 0x40, 0x40, 0x55, 0x84}, 2500, 100, 30}
};

void displayWrap() {
  delay(30);  // delay initial avant la 1ere frame
  const uint8_t N = sizeof(DISPLAY_WRAP_FRAMES) / sizeof(DISPLAY_WRAP_FRAMES[0]);
  for (uint8_t i = 0; i < N; i++) {
    renderAnimFrame(&DISPLAY_WRAP_FRAMES[i]);
  }
  showTime();
}

// Frames de wrapBurnout (matrix 0 rows 0-5 uniquement). Matrix 1 + row 6
// initialises une seule fois au debut de la fonction.
const uint8_t WRAP_BURNOUT_R0_FRAMES[][6] PROGMEM = {
  {0x40, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x40, 0x40, 0x00, 0x00, 0x00, 0x00},
  {0x40, 0x40, 0x40, 0x00, 0x00, 0x00},
  {0x40, 0x40, 0x40, 0x40, 0x00, 0x00},
  {0x40, 0x40, 0x40, 0x40, 0x40, 0x00},
  {0x40, 0x40, 0x40, 0x40, 0x40, 0x40},
  {0x40, 0x40, 0x40, 0x40, 0x40, 0x60},
  {0x40, 0x40, 0x40, 0x40, 0x40, 0x70},
  {0x40, 0x40, 0x40, 0x40, 0x40, 0x78},
  {0x40, 0x40, 0x40, 0x40, 0x48, 0x78},
  {0x40, 0x40, 0x40, 0x48, 0x48, 0x78},
  {0x40, 0x40, 0x48, 0x48, 0x48, 0x78},
  {0x40, 0x48, 0x48, 0x48, 0x48, 0x78},
  {0x48, 0x48, 0x48, 0x48, 0x48, 0x78},
  {0x4C, 0x48, 0x48, 0x48, 0x48, 0x78},
  {0x4E, 0x48, 0x48, 0x48, 0x48, 0x78},
  {0x0E, 0x48, 0x48, 0x48, 0x48, 0x78},
  {0x0E, 0x08, 0x48, 0x48, 0x48, 0x78},
  {0x0E, 0x08, 0x08, 0x48, 0x48, 0x78},
  {0x0E, 0x08, 0x08, 0x08, 0x48, 0x78},
  {0x0E, 0x08, 0x08, 0x08, 0x08, 0x78},
  {0x0E, 0x08, 0x08, 0x08, 0x08, 0x38},
  {0x0E, 0x08, 0x08, 0x08, 0x08, 0x18},
  {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08},
  {0x0E, 0x08, 0x08, 0x08, 0x08, 0x00},
  {0x0E, 0x08, 0x08, 0x08, 0x00, 0x00},
  {0x0E, 0x08, 0x08, 0x00, 0x00, 0x00},
  {0x0E, 0x08, 0x00, 0x00, 0x00, 0x00},
  {0x0E, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x06, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x02, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

void wrapBurnout() {
  // Init matrix 1 + row 6 (etat constant pour toutes les frames)
  lc.setRow(1, 0, 0x6D);
  lc.setRow(1, 1, 0xFB);
  lc.setRow(1, 2, 0x3B);
  lc.setRow(1, 6, 0x78);
  lc.setRow(1, 3, 0x00);
  lc.setRow(1, 4, 0x00);

  const uint8_t N = sizeof(WRAP_BURNOUT_R0_FRAMES) / sizeof(WRAP_BURNOUT_R0_FRAMES[0]);
  uint8_t r0[6];
  for (uint8_t i = 0; i < N; i++) {
    memcpy_P(r0, WRAP_BURNOUT_R0_FRAMES[i], 6);
    for (uint8_t r = 0; r < 6; r++) lc.setRow(0, r, r0[r]);
    delay(50);
  }
}

// arg 1 : couleur, arg 2 : Vitesse, arg 3 : une seule led allumée, arg 4 : direction (true pour normal, false pour inverse)
void colorWipeUnified(uint32_t color, int speed = 100, bool single = false, bool forward = true) {
  int start = forward ? 0 : strip.numPixels() - 1;
  int end = forward ? strip.numPixels() : -1;
  int step = forward ? 1 : -1;

  for (int i = start; i != end; i += step) {
    if (single)
      strip.clear();
    strip.setPixelColor(i, color);
    strip.show();
    delay(speed);
  }
}

void loop() {
  button1.tick();
  button2.tick();

  debounceSTART();
  debounceVORTEX();

  delay(10);

  strip.show();
  blinkLEDUnified(etatArduinoLED);
  normal();

  if (digitalRead(BP_VORTEX) == LOW) {
    BP_VORTEX_STATUS = true;
  }
  if ((digitalRead(BP_UP) == LOW) && (digitalRead(BP_DOWN) == LOW)) {
    mode_test();
  }
  if (digitalRead(BP_START) == true) {
    BP_START_STATUS = true;
  }
}

void reboot() {
  // Reset propre via le watchdog (vs jump a 0 qui ne reset pas les peripheriques)
  wdt_enable(WDTO_15MS);
  while (1) {}
}

void click1() {
  veille();
}
void click2() {
  menu_up();
  buzzer_menu();
}

void longPress1() {
  reveille();
}
void longPress2() {
  reboot();
}

void doubleclick1() {
  zap();
}
void doubleclick2() {
  BP_POWER_STATUS = true;
}

const uint64_t IMAGES_POST_FIN[] PROGMEM = {
  0x0000000000000000, 0x0000000102000000, 0x0000008106000000, 0x000000c10e000000,
  0x000000e11e000000, 0x000000f13e000000, 0x000000f97e000000, 0x000000fdfe000000,
  0x000000ffff000000, 0x000000fdfe000000, 0x000000f97e000000, 0x000000f13e000000,
  0x000000e11e000000, 0x000000c10e000000, 0x0000008106000000, 0x0000000102000000
};

const uint64_t IMAGES_NORMAL[] PROGMEM = {
  0x0000000000000000, 0x0000000201000000, 0x0000000681000000, 0x0000000ec1000000,
  0x0000001ee1000000, 0x0000003ef1000000, 0x0000001ee1000000, 0x0000000ec1000000,
  0x0000001ee1000000, 0x0000003ef1000000, 0x0000007ef9000000, 0x000000fefd000000,
  0x000000ffff000000, 0x000000fefd000000, 0x0000007ef9000000, 0x0000003ef1000000,
  0x0000001ee1000000, 0x0000000ec1000000, 0x0000001ee1000000, 0x0000003ef1000000,
  0x0000001ee1000000, 0x0000000ec1000000, 0x0000000681000000, 0x0000000201000000
};

const uint64_t IMAGES_FIN[] PROGMEM = {
  0x0000000000000000, 0x0000000101000000, 0x0000008181000000, 0x0000008383000000,
  0x000000c3c3000000, 0x000000c7c7000000, 0x000000e7e7000000, 0x000000f7f7000000,
  0x000000ffff000000, 0x000000f7f7000000, 0x000000e7e7000000, 0x000000c7c7000000,
  0x000000c3c3000000, 0x0000008383000000, 0x0000008181000000, 0x0000000101000000,
  0x0000000000000000, 0x0000000101000000, 0x0000008181000000, 0x0000008383000000,
  0x000000c3c3000000, 0x000000c7c7000000, 0x000000e7e7000000, 0x000000f7f7000000,
  0x000000ffff000000, 0x000000f7f7000000, 0x000000e7e7000000, 0x000000c7c7000000,
  0x000000c3c3000000, 0x0000008383000000, 0x0000008181000000, 0x0000000101000000,
  0x0000000000000000, 0x0000000000000000, 0x0000000101000000, 0x0000008181000000,
  0x0000008383000000, 0x000000c3c3000000, 0x000000c7c7000000, 0x000000e7e7000000,
  0x000000e7cf000000, 0x000000f39f000000, 0x000000f93f000000, 0x000000fd7f000000,
  0x000000f99f000000, 0x000000f3cf000000, 0x000000e7e7000000, 0x000000cff3000000,
  0x0000009ff9000000, 0x0000003ffd000000, 0x0000009ff9000000, 0x000000cff3000000,
  0x000000e7e7000000, 0x000000f3cf000000, 0x000000f99f000000, 0x000000fd7f000000,
  0x000000f99f000000, 0x000000f3cf000000, 0x000000e7e7000000, 0x000000cff3000000,
  0x0000009ff9000000, 0x0000003ffd000000, 0x0000007ffd000000, 0x0000009ff9000000,
  0x000000cff3000000, 0x000000e7e7000000, 0x000000f3cf000000, 0x000000f99f000000,
  0x000000fd3f000000, 0x000000f97f000000, 0x000000f33f000000, 0x000000e79f000000,
  0x000000cfcf000000, 0x0000009fe7000000, 0x0000003ff3000000, 0x0000007ff9000000,
  0x0000003ffc000000, 0x0000009ff9000000, 0x000000cff3000000, 0x000000e7e7000000
};

const uint64_t IMAGES_OFF[] PROGMEM = {
  0x0000000000000000
};

Bargraphe bargraphePostFin = { IMAGES_POST_FIN, sizeof(IMAGES_POST_FIN) / 8, 0, 15 };
Bargraphe bargrapheNormal = { IMAGES_NORMAL, sizeof(IMAGES_NORMAL) / 8, 0, 20 };
Bargraphe bargrapheFin = { IMAGES_FIN, sizeof(IMAGES_FIN) / 8, 0, 15 };
Bargraphe bargrapheOff = { IMAGES_OFF, sizeof(IMAGES_OFF) / 8, 0, 0 };

void displayImage(const uint64_t image) {
  for (int row = 3; row < 5; row++) {
    byte rowData = (image >> (row * 8)) & 0xFF;
    for (int col = 0; col < 8; col++) {
      lc.setLed(1, row, col, bitRead(rowData, col));
    }
  }
}

void animateBargraphe(Bargraphe &bargraphe) {
  uint64_t img;
  memcpy_P(&img, &bargraphe.images[bargraphe.index], sizeof(uint64_t));
  displayImage(img);
  if (++bargraphe.index >= bargraphe.length) {
    bargraphe.index = 0;
  }
  delay(bargraphe.delayTime);
}

#define secondsinaday 86400
#define secondsinhour 3600
#define secondsinminute 60

void showTime() {
  unsigned long days = totalsectime / secondsinaday;
  unsigned long hours = (totalsectime % secondsinaday) / secondsinhour;
  unsigned long minutes = (totalsectime % secondsinhour) / secondsinminute;
  unsigned long seconds = totalsectime % secondsinminute;

  // Days
  lc.setDigit(1, 0, days / 100, false);
  lc.setDigit(1, 1, (days % 100) / 10, false);
  lc.setDigit(1, 2, days % 10, false);

  // Hours
  lc.setDigit(0, 0, hours / 10, false);
  lc.setDigit(0, 1, hours % 10, false);

  // Minutes
  lc.setDigit(0, 2, minutes / 10, false);
  lc.setDigit(0, 3, minutes % 10, false);

  // Seconds
  lc.setDigit(0, 4, seconds / 10, false);
  lc.setDigit(0, 5, seconds % 10, false);
}

void delayIncrement(byte rst) {
  static unsigned long last_time3 = 0;
  unsigned long time_now3 = millis();

  if (rst) {
    last_time3 = time_now3;
    return;
  }

  if (time_now3 - last_time3 >= 1000) {
    delay(100);
  } else {
    delay(150);
  }
}

void handleMenu(int menu, int increment, int digitPosition, bool isDayMenu = false) {
  button1.tick();
  button2.tick();

  ecranBlinkUnified(false, isDayMenu ? 1 : 0, digitPosition);

  if (digitalRead(BP_UP) == LOW) {
    customTone(3000, 50);

    if (isDayMenu) {
      unsigned long currentDays = totalsectime / 86400;
      currentDays++;
      totalsectime = (currentDays * 86400) + (totalsectime % 86400);
    } else {
      totalsectime += increment;
    }

    delayIncrement(false);
    showTime();
  } else {
    delayIncrement(true);
  }

  if (digitalRead(BP_DOWN) == LOW) {
    customTone(2900, 50);

    if (isDayMenu) {
      unsigned long currentDays = totalsectime / 86400;
      if (currentDays > 0) {
        currentDays--;
      }
      totalsectime = (currentDays * 86400) + (totalsectime % 86400);
    } else if (totalsectime >= (unsigned long)increment) {
      totalsectime -= increment;
    } else {
      totalsectime = 0;
    }

    delayIncrement(false);
    showTime();
  } else {
    delayIncrement(true);
  }
}

void buttonStart() {
  running = !running;
}

// Effet "scramble" rapide sur les 2 digits secondes (matrix 0, rows 4-5)
// pour donner un tick visuel a chaque incrementation/decrementation.
// ~45ms total, non bloquant pour le timing 1Hz du countdown.
void animateSecondTick() {
  for (uint8_t i = 0; i < 3; i++) {
    lc.setDigit(0, 4, random(10), false);
    lc.setDigit(0, 5, random(10), false);
    delay(15);
  }
}

// Animation idle: pulse lent des NeoPixels (effet "respiration") quand le
// timer est allume mais ni en running ni en menu. Ne touche pas la couleur
// des pixels (adafruit gere ca via le potentiometre), juste la brightness.
void idleAnimation() {
  const uint16_t period = 3000;  // 3 sec / pulsation
  uint16_t phase = millis() % period;
  uint16_t halfPeriod = period / 2;
  uint8_t brightness;
  if (phase < halfPeriod) {
    brightness = 3 + (uint8_t)((12UL * phase) / halfPeriod);   // 3 -> 15
  } else {
    brightness = 15 - (uint8_t)((12UL * (phase - halfPeriod)) / halfPeriod);  // 15 -> 3
  }
  strip.setBrightness(brightness);
  strip.show();
}

// Pulse la brightness des NeoPixels en fonction du temps restant.
// Plus le temps restant est court, plus la pulsation est rapide.
// Force aussi la couleur des pixels (rouge selon prevVal) car adafruit() ne
// refresh que si le potentiometre a change.
void pulseNeoPixel(unsigned long reste) {
  uint16_t period;
  if (reste <= 5)        period = 200;   // 5 Hz - stress final
  else if (reste <= 10)  period = 350;   // ~3 Hz - panique
  else if (reste <= 20)  period = 600;   // ~1.6 Hz - urgence
  else if (reste <= 30)  period = 900;   // ~1 Hz - warning
  else                   period = 1800;  // 0.55 Hz - calme

  unsigned long t = millis() % period;
  uint8_t brightness;
  uint16_t halfPeriod = period / 2;
  if (t < halfPeriod) {
    brightness = 5 + (uint8_t)((45UL * t) / halfPeriod);   // 5 -> 50
  } else {
    brightness = 50 - (uint8_t)((45UL * (t - halfPeriod)) / halfPeriod);  // 50 -> 5
  }
  // Reapplique les couleurs (au cas ou un autre code les a effacees)
  for (uint8_t p = 0; p < 7; p++) {
    strip.setPixelColor(p, p < (uint8_t)prevVal ? Red : Off);
  }
  strip.setBrightness(brightness);
  strip.show();
}

// Animation "wormhole opening" - utilisee a la fin du burnout (totalsectime=0).
// Convergence visuelle puis flash intense puis fade-out.
void wormholeOpening() {
  // Phase 1: build-up des segments (top -> tout)
  static const uint8_t BUILD_UP[] PROGMEM = {0x40, 0x60, 0x70, 0x71, 0x77, 0x7F};
  for (uint8_t step = 0; step < 6; step++) {
    uint8_t pattern = pgm_read_byte(&BUILD_UP[step]);
    for (uint8_t r = 0; r < 6; r++) lc.setRow(0, r, pattern);
    for (uint8_t r = 0; r < 5; r++) lc.setRow(1, r, pattern);

    strip.setBrightness(20 + step * 10);
    for (uint8_t p = 0; p < 7; p++) strip.setPixelColor(p, Red);
    strip.show();
    delay(200);
  }

  // Phase 2: flash intense alterne
  for (uint8_t i = 0; i < 4; i++) {
    for (uint8_t r = 0; r < 6; r++) lc.setRow(0, r, 0xFF);
    for (uint8_t r = 0; r < 5; r++) lc.setRow(1, r, 0xFF);
    strip.setBrightness(100);
    strip.show();
    delay(80);
    for (uint8_t r = 0; r < 6; r++) lc.setRow(0, r, 0x00);
    for (uint8_t r = 0; r < 5; r++) lc.setRow(1, r, 0x00);
    strip.setBrightness(0);
    strip.show();
    delay(80);
  }

  // Phase 3: fade-out NeoPixel
  for (int b = 100; b >= 0; b -= 10) {
    strip.setBrightness(b);
    for (uint8_t p = 0; p < 7; p++) strip.setPixelColor(p, Red);
    strip.show();
    delay(50);
  }
  for (uint8_t p = 0; p < 7; p++) strip.setPixelColor(p, Off);
  strip.show();
}

// Animation "alarme" - utilisee a la fin du countdown normal.
// Flashes ON/OFF rapides avec bip + NeoPixel rouge.
void countdownEnd() {
  for (uint8_t i = 0; i < 8; i++) {
    // ON
    for (uint8_t r = 0; r < 6; r++) lc.setRow(0, r, 0x7F);
    for (uint8_t r = 0; r < 5; r++) lc.setRow(1, r, 0xFF);
    strip.setBrightness(80);
    for (uint8_t p = 0; p < 7; p++) strip.setPixelColor(p, Red);
    strip.show();
    customTone(2500, 100);
    delay(150);
    // OFF
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    strip.setBrightness(0);
    strip.show();
    noTone(speaker);
    delay(150);
  }
  for (uint8_t p = 0; p < 7; p++) strip.setPixelColor(p, Off);
  strip.show();
  noTone(speaker);
}

// Animation epique de ~8 secondes pour accompagner le mp3 vortex (0001.mp3).
// Phase 1 (0-6s): rotation des segments qui s'accelere + bargraphe qui se
// remplit + NeoPixels rouges progressifs.
// Phase 2 (6-8s): climax avec flash intense + tous les segments allumes.
void vortexAnimation() {
  static const uint8_t SEG_ROT[] PROGMEM = {0x40, 0x20, 0x10, 0x08, 0x04, 0x02};  // a, b, c, d, e, f

  unsigned long start = millis();
  uint16_t step = 0;

  while (millis() - start < VORTEX_MP3_DURATION_MS) {
    unsigned long elapsed = millis() - start;

    if (elapsed < 6000) {
      // Phase rotation acceleree
      uint8_t seg = pgm_read_byte(&SEG_ROT[step % 6]);
      for (uint8_t r = 0; r < 6; r++) lc.setRow(0, r, seg);
      for (uint8_t r = 0; r < 3; r++) lc.setRow(1, r, seg);

      // Bargraphe progressif (matrix 1 rows 3-4)
      uint8_t fillLevel = elapsed / 750;
      if (fillLevel > 8) fillLevel = 8;
      uint8_t bar = (fillLevel >= 8) ? 0xFF : ((uint8_t)((1 << fillLevel) - 1));
      lc.setRow(1, 3, bar);
      lc.setRow(1, 4, bar);

      // NeoPixels rouges progressifs
      uint8_t pixCount = (uint8_t)((elapsed / 800) + 1);
      if (pixCount > 7) pixCount = 7;
      for (uint8_t p = 0; p < 7; p++) {
        strip.setPixelColor(p, p < pixCount ? Red : Off);
      }
      uint8_t bright = 20 + (uint8_t)(elapsed / 200);
      if (bright > 60) bright = 60;
      strip.setBrightness(bright);
      strip.show();

      // Vitesse: 100ms -> 25ms (acceleration)
      int spd = 100 - (int)(elapsed / 80);
      if (spd < 25) spd = 25;
      delay(spd);
      step++;
    } else {
      // Phase climax: flash intense
      bool on = ((elapsed / 80) & 1) != 0;
      for (uint8_t r = 0; r < 6; r++) lc.setRow(0, r, on ? 0x7F : 0x00);
      for (uint8_t r = 0; r < 5; r++) lc.setRow(1, r, on ? 0xFF : 0x00);

      strip.setBrightness(80);
      for (uint8_t p = 0; p < 7; p++) strip.setPixelColor(p, on ? Red : Off);
      strip.show();
      delay(60);
    }
  }

  // Cleanup
  lc.clearDisplay(0);
  lc.clearDisplay(1);
  for (uint8_t p = 0; p < 7; p++) strip.setPixelColor(p, Off);
  strip.show();
}

void normal() {
  // mode 0=normal 1=burnout 2=foutu
  byte vortex = 0;

  // mise en route minuteur par double click BP_POWER
  if (BP_POWER_STATUS == true) {
    if (firstStartup) {
      strip.show();
      BP_VORTEX_STATUS = false;
      scrollVersion();  // Defilement "sliders timer replica by kenny v1.2"
      startMusique();
      genserSequence();
      displayWrap();
      waitMs(START_OUTRO_DELAY_MS);  // synchro fin du clip MP3 d'activation
      lc.clearDisplay(0);
      lc.clearDisplay(1);
      showTime();
      lc.setLed(0, 6, 1, true);
      lc.setLed(0, 6, 2, true);
      lc.setLed(0, 6, 3, true);
      lc.setLed(0, 6, 4, true);
      firstStartup = false;
    }

    if ((running) && (totalsectime_slide != 0)) {
      // Activation: LEDs blanches emetteur + bip avant le mp3
      lc.setLed(1, 7, 2, true);
      lc.setLed(1, 7, 3, true);
      customTone(3000, 1500);
      delay(1500);
      lc.setLed(1, 7, 2, false);
      lc.setLed(1, 7, 3, false);

      mp3_play_physical(1);  // 0001.mp3 = son de vortex
      // Animation custom epique synchro avec le mp3 (~8s)
      int prevBuzzerState = buzzerState;
      buzzerState = 0;
      vortexAnimation();
      buzzerState = prevBuzzerState;

      while ((vortex == 0) && (BP_START_STATUS == true) && (totalsectime_slide != 0)) {
        animation_normal();
        button1.tick();
        button2.tick();

        static unsigned long last_time = 0;
        unsigned long time_now = millis();

        unsigned long totalsectime_save;
        unsigned long totalsectime_reste = (totalsectime_slide > totalsectime)
                                             ? (totalsectime_slide - totalsectime)
                                             : 0;

        if (time_now - last_time >= 1000) {
          totalsectime++;
          last_time = time_now;
          animateSecondTick();
          showTime();
        }

        // End normal mode: animation alarme + cleanup
        if (totalsectime_reste == 0) {
          mp3_play_physical(1);  // 0001.mp3 = son de vortex
          totalsectime = 0;
          totalsectime_slide = 0;
          totalsectime_reste = 0;
          countdownEnd();  // animation alarme
          lc.setLed(1, 5, 5, false);
          lc.setLed(1, 5, 3, false);
          lc.setLed(1, 5, 4, false);
          lc.setLed(1, 7, 2, false);
          lc.setLed(1, 7, 3, false);
          showTime();
          running = false;
          BP_START_STATUS = false;
        }

        // Enter in butnout mode
        if (digitalRead(BP_VORTEX) == LOW) {
          mp3_play_physical(1);  // 0001.mp3 = son de vortex
          vortex = 1;
          customTone(3000, 1000);

          lc.setLed(1, 7, 4, true);
          lc.setLed(1, 7, 1, true);
          colorWipeUnified(Red, 100, false, true);
          colorWipeUnified(Off, 100, false, false);

          delay(1500);
          noTone(speaker);

          totalsectime = 90;
          totalsectime_slide = 0;
          totalsectime_reste = 0;

          displayFade();
          genserSequence();
          displayWrap();
          reveille(false);
        }

        // Time remain
        if (digitalRead(BP_UP) == LOW) {
          noTone(speaker);
          running = false;
          totalsectime_save = totalsectime;
          lc.setRow(0, 0, B00000101);
          lc.setRow(0, 1, B01101111);
          lc.setRow(0, 2, B00010101);
          lc.setRow(0, 3, B01110111);
          lc.setRow(0, 4, B00010000);
          lc.setRow(0, 5, B00010101);
          delay(1000);
          lc.setRow(0, 0, B00001111);
          lc.setRow(0, 1, B00010000);
          lc.setRow(0, 2, B00010101);
          lc.setRow(0, 3, B01101111);
          lc.setRow(0, 4, B00000000);
          lc.setRow(0, 5, B00000000);
          delay(1000);

          totalsectime = totalsectime_reste;
          showTime();
          delay(2000);

          totalsectime = totalsectime_save;
          showTime();
          running = true;
        }

        // Show batery
        if ((digitalRead(BP_DOWN) == LOW) && (running == true)) {
          batterie();
        }

        // Mute
        if ((digitalRead(BP_START) == LOW) && (running == true)) {
          buzzerState = !buzzerState;
        }
      }
    }

    if (vortex == 1) {
      while (vortex == 1) {
        showTime();
        animation_burnout();
        button1.tick();
        button2.tick();

        lc.setLed(1, 7, 1, false);
        lc.setLed(1, 7, 2, false);
        lc.setLed(1, 7, 3, false);
        lc.setLed(1, 7, 4, false);

        static unsigned long last_time1 = 0;
        unsigned long time_now1 = millis();

        if (time_now1 - last_time1 >= 1000) {
          totalsectime--;
          last_time1 = time_now1;
          animateSecondTick();
          showTime();
        }

        if ((totalsectime <= 1) && (digitalRead(BP_VORTEX) == LOW)) {
          mp3_play_physical(1);  // 0001.mp3 = son de vortex
          lc.setLed(1, 7, 1, true);
          lc.setLed(1, 7, 2, true);
          lc.setLed(1, 7, 3, true);
          lc.setLed(1, 7, 4, true);
          customTone(3000, 1000);
          colorWipeUnified(Red, 100, false, true);
          colorWipeUnified(Off, 100, false, false);
          delay(1500);
          vortex = 1;
          displayFade();
          genserSequence();
          displayWrap();
          totalsectime = random(16756131);  // random
          reveille(false);
        }

        if (totalsectime == 0) {
          showTime();
          vortex = 2;
          wormholeOpening();  // animation epique de fin burnout
          totalsectime = 0;
          startMusique();
        }

        if ((digitalRead(BP_DOWN) == LOW) && (running == true)) {
          batterie();
        }

        if (digitalRead(BP_START) == LOW) {
          buzzerState = !buzzerState;
        }
      }
    }

    if (vortex == 2) {
      while (vortex == 2) {
        button2.tick();
        noTone(speaker);
        wrapBurnout();
        animation_29();
        vortex = 2;

        if (digitalRead(BP_DOWN) == LOW) {
          batterie();
        }

        if ((digitalRead(BP_START) == LOW) && (running == true)) {
          buzzerState = !buzzerState;
        }
      }
    }

    else {
      adafruit();
      if (menu == 0) {
        idleAnimation();
      }
      lc.setRow(1, 6, B01111000);

      while (!running && menu == 1) {
        handleMenu(1, 1, 5);  // Incrémenter/décrémenter les secondes, digit des secondes
      }

      while (!running && menu == 2) {
        handleMenu(2, 60, 3);  // Incrémenter/décrémenter les minutes, digit des minutes
      }

      while (!running && menu == 3) {
        handleMenu(3, 3600, 1);  // Incrémenter/décrémenter les heures, digit des heures
      }

      while (!running && menu == 4) {
        handleMenu(4, 1, 2, true);  // Incrémenter/décrémenter les jours, digit des jours (isDayMenu = true)
      }

      while (!running && menu == 5) {
        button1.tick();
        button2.tick();
        ecranBlinkUnified(true);
        totalsectime_slide = totalsectime;
      }

      while (!running && menu == 6) {
        totalsectime = 0;
        delay(150);
        showTime();
        menu = 0;

        // Sauvegarde du temps lock en EEPROM (utilise put: ne re-ecrit pas
        // si la valeur est identique -> preserve la duree de vie de la flash).
        EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
        EEPROM.put(EEPROM_TOTALSECTIME_SLIDE_ADDR, totalsectime_slide);

        if (totalsectime_slide != 0) {
          lc.setRow(0, 0, B01011011);  // S
          lc.setRow(0, 1, B00001110);  // L
          lc.setRow(0, 2, B00110000);  // I
          lc.setRow(0, 3, B01111110);  // D
          lc.setRow(0, 4, B01001111);  // E
          lc.setRow(0, 5, B00000000);
        } else {
          lc.setRow(0, 0, B01001111);  // E
          lc.setRow(0, 1, B00010101);  // r
          lc.setRow(0, 2, B00010101);  // r
          lc.setRow(0, 3, B00011101);  // o
          lc.setRow(0, 4, B00010101);  // r
          lc.setRow(0, 5, B10000000);
        }
      }
    }
  }
}

void animation_burnout() {
  lc.setRow(1, 6, B01111000);
  adafruit();
  pulseNeoPixel(totalsectime);

  if (totalsectime <= 30) {
    animateBargraphe(bargrapheFin);
  } else if ((totalsectime >= 30 && totalsectime <= 59)) {
    animateBargraphe(bargraphePostFin);
  } else {
    animateBargraphe(bargrapheNormal);
  }

  if ((totalsectime >= 35 && totalsectime <= 59)) {
    lc.setLed(1, 5, 5, true);
  } else {
    blinkLEDUnified(greenLED);
  }

  if ((totalsectime >= 15 && totalsectime <= 35)) {
    lc.setLed(1, 5, 3, true);
  } else {
    blinkLEDUnified(yellowLED);
  }

  if (totalsectime <= 15) {
    lc.setLed(1, 5, 4, true);
  } else {
    blinkLEDUnified(redLED);
  }
  if (totalsectime == 10 && !wrapPlayedAt10s) {
    displayWrap();
    wrapPlayedAt10s = true;
  }

  if (totalsectime == 30) {
    reveille(false);
  }

  // Mode burnout: totalsectime descend de N -> 0
  if (totalsectime > 20 && totalsectime <= 30) {
    updatespeaker_pattern(25, 5, 500);    // warning
  } else if (totalsectime > 10 && totalsectime <= 20) {
    updatespeaker_pattern(15, 10, 330);   // urgence
  } else if (totalsectime > 5 && totalsectime <= 10) {
    updatespeaker_pattern(10, 10, 200);   // panique
  } else if (totalsectime > 0 && totalsectime <= 5) {
    updatespeaker_pattern(5, 15, 100);    // stress final
  } else if (totalsectime == 0) {
    customTone(3000, 200);
  } else {
    updatespeaker_pattern(500, 2, 0);     // calme (>30)
  }

  if ((totalsectime <= 5)) {
    lc.setLed(0, 6, 1, true);
    lc.setLed(0, 6, 2, true);
    lc.setLed(0, 6, 3, true);
    lc.setLed(0, 6, 4, true);
  }

  else {
    blinkLEDUnified(colonLED);
  }
}

void animation_normal() {
  unsigned long totalsectime_reste = totalsectime_slide - totalsectime;
  animateBargraphe(bargraphePostFin);
  blinkLEDUnified(greenLED);
  blinkLEDUnified(yellowLED);
  blinkLEDUnified(redLED);
  blinkLEDUnified(colonLED);
  adafruit();
  pulseNeoPixel(totalsectime_reste);
  lc.setRow(1, 6, B01111000);

  if (totalsectime == 30) {
    reveille(false);
  }

  // Mode normal: totalsectime_reste = temps restant avant fin du countdown
  if ((totalsectime_reste > 20) && (totalsectime_reste <= 30)) {
    updatespeaker_pattern(25, 5, 500);    // warning
  } else if ((totalsectime_reste > 10) && (totalsectime_reste <= 20)) {
    updatespeaker_pattern(15, 10, 330);   // urgence
  } else if ((totalsectime_reste > 5) && (totalsectime_reste <= 10)) {
    updatespeaker_pattern(10, 10, 200);   // panique
  } else if ((totalsectime_reste > 1) && (totalsectime_reste <= 5)) {
    updatespeaker_pattern(5, 15, 100);    // stress final
  } else if (totalsectime_reste <= 1) {
    customTone(3000, 1500);
  } else {
    updatespeaker_pattern(500, 2, 0);     // calme (>30)
  }

  if (totalsectime_reste == 6) {
    lc.setLed(1, 7, 2, true);
    lc.setLed(1, 7, 3, false);
  }
  if (totalsectime_reste == 5) {
    lc.setLed(1, 7, 2, false);
    lc.setLed(1, 7, 3, true);
  }
  if (totalsectime_reste == 4) {
    lc.setLed(1, 7, 2, true);
    lc.setLed(1, 7, 3, false);
  }
  if (totalsectime_reste == 3) {
    lc.setLed(1, 7, 2, false);
    lc.setLed(1, 7, 3, true);
  }
  if (totalsectime_reste <= 2) {
    lc.setLed(1, 7, 2, true);
    lc.setLed(1, 7, 3, true);
  }
  if (totalsectime_reste > 6) {
    lc.setLed(1, 7, 2, false);
    lc.setLed(1, 7, 3, false);
  }
  // Le bloc d'activation (LEDs + bip 1.5s a totalsectime==0) a ete deplace
  // dans normal() pour s'executer AVANT le mp3 d'activation.
}

void animation_29() {
  lc.setLed(1, 7, 1, false);
  lc.setLed(1, 7, 2, false);
  lc.setLed(1, 7, 3, false);
  lc.setLed(1, 7, 4, false);
  lc.setLed(1, 5, 4, false);
  lc.setLed(1, 5, 3, false);
  lc.setLed(1, 5, 5, false);
  lc.setLed(0, 6, 1, false);
  lc.setLed(0, 6, 2, false);
  lc.setLed(0, 6, 3, false);
  lc.setLed(0, 6, 4, false);
  animateBargraphe(bargrapheOff);
}

void adafruit() {
  total = total - readings[readIndex];
  readings[readIndex] = analogRead(pot1);
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;
  average = total / numReadings;

  int newVal = map(average, 0, 1023, 0, 8);
  newVal = 8 - newVal;

  if (newVal != prevVal) {
    strip.setBrightness(10);

    for (int x = 0; x < newVal; x++) {
      strip.setPixelColor(x, strip.Color(255, 0, 0));
    }

    for (int x = newVal; x < strip.numPixels(); x++) {
      strip.setPixelColor(x, 0, 0, 0);
    }

    strip.show();
    prevVal = newVal;
  }
}

void offadafruit() {
  strip.fill(strip.Color(0, 0, 0), 0, strip.numPixels());
  strip.show();
}

void onadafruit() {
  for (int x = 0; x < prevVal; x++) {
    strip.setPixelColor(x, strip.Color(255, 0, 0));
  }

  for (int x = prevVal; x < strip.numPixels(); x++) {
    strip.setPixelColor(x, strip.Color(0, 0, 0));
  }

  strip.show();
}

void veille() {
  lc.shutdown(1, HIGH);
  lc.shutdown(0, HIGH);
  offadafruit();
  buzzerState = 0;
}

void reveille(bool fadeIn = true) {
  // Fade-in: on rallume avec intensite 0 puis on monte progressivement.
  // Avec fadeIn=false: rallumage instantane (utilise dans les boucles countdown
  // ou un delay de 640ms casserait le timing 1Hz du compteur).
  if (fadeIn) {
    lc.setIntensity(0, 0);
    lc.setIntensity(1, 0);
  }
  lc.shutdown(1, LOW);
  lc.shutdown(0, LOW);
  if (fadeIn) {
    for (uint8_t level = 0; level <= 15; level++) {
      lc.setIntensity(0, level);
      lc.setIntensity(1, level);
      delay(40);
    }
  } else {
    lc.setIntensity(0, 15);
    lc.setIntensity(1, 15);
  }
  onadafruit();
  buzzerState = 1;
}

void debounceSTART() {
  int reading = digitalRead(BP_START);
  if (reading != OLD_BP_START_STATUS) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) >= debounceDelay) {
    if (reading != BP_START_STATUS) {
      BP_START_STATUS = reading;
    }
  }
  OLD_BP_START_STATUS = reading;
}

// antirebond bp vortex
void debounceVORTEX() {
  int reading1 = digitalRead(BP_VORTEX);
  if (reading1 != OLD_BP_VORTEX_STATUS) {
    lastDebounceTime1 = millis();
  }
  if ((millis() - lastDebounceTime1) >= debounceDelay1) {
    if (reading1 != BP_VORTEX_STATUS) {
      BP_VORTEX_STATUS = reading1;
    }
  }
  OLD_BP_VORTEX_STATUS = reading1;
}

void modeTestSetAll(byte segments, byte chenillard, bool ledsOn) {
  // ecrans hhmmss/ddd + bargraphe
  for (int row = 0; row < 6; row++) lc.setRow(0, row, segments);
  for (int row = 0; row < 5; row++) lc.setRow(1, row, segments);
  // colons
  for (int col = 1; col <= 4; col++) lc.setLed(0, 6, col, ledsOn);
  // chenillard extremites
  lc.setRow(1, 6, chenillard);
  // status LEDs (jaune/rouge/verte)
  lc.setLed(1, 5, 3, ledsOn);
  lc.setLed(1, 5, 4, ledsOn);
  lc.setLed(1, 5, 5, ledsOn);
  // emetteur
  for (int col = 1; col <= 4; col++) lc.setLed(1, 7, col, ledsOn);
}

void mode_test() {
  strip.show();
  modeTestSetAll(B11111111, B01111000, true);

  customTone(3000, 5000);
  strip.fill(strip.Color(255, 0, 0), 0, 7);

  delay(10000);

  modeTestSetAll(B00000000, B00000000, false);
  noTone(speaker);
  strip.fill(strip.Color(0, 0, 0), 0, 7);
}

void buzzer_menu() {
  if ((!running) && (BP_POWER_STATUS == true)) {
    customTone(3100, 200);
  }
}

void menu_up() {
  if (BP_POWER_STATUS == true) {
    menu++;
  }
}

// Lit Vcc en mV via la reference interne 1.1V (sans hardware externe).
// Si Arduino est alimente direct par batterie LiPo, retourne la tension batterie.
// Si alim USB ou regulateur 5V, retourne ~5000.
long readVccMv() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);  // 1.1V vs AVcc
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC)) {}
  long result = ADCL;
  result |= ADCH << 8;
  if (result == 0) return -1;
  return 1125300L / result;  // 1.1 * 1023 * 1000
}

// Lit le niveau de batterie. Strategie:
// 1) Si Vcc < 4.8V -> alim direct par batterie, on lit Vcc.
// 2) Sinon, on tente BATTERYPIN avec un diviseur de tension externe.
// Retourne 0-100 (%) ou -1 si pas de mesure fiable (USB regule).
int getBattery() {
  long vccMv = readVccMv();

  // Cas 1: alim direct batterie (Vcc varie avec la batterie)
  if (vccMv > 0 && vccMv < 4800) {
    float tension = vccMv / 1000.0f;
    float pct = ((tension - TensionMin) / (TensionMax - TensionMin)) * 100;
    return (int)constrain(pct, 0, 100);
  }

  // Cas 2: Vcc regule (USB / boost 5V) -> on essaie BATTERYPIN avec diviseur
  int reading = analogRead(BATTERYPIN);
  if (reading <= 5 || reading >= 1020) {
    // pin flottant ou sature: pas de mesure fiable
    return -1;
  }
  float tension = (reading / 1023.0f) * 5.0f;
  float pct = ((tension - TensionMin) / (TensionMax - TensionMin)) * 100;
  return (int)constrain(pct, 0, 100);
}

void batterie() {
  noTone(speaker);
  running = false;
  unsigned long totalsectime_save = totalsectime;

  lc.clearDisplay(0);
  lc.setRow(0, 0, B00011111);  // 'b'
  lc.setRow(0, 1, B01111101);  // 'a'
  lc.setRow(0, 2, B00000111);  // 't'
  lc.setRow(0, 3, B01101111);  // 'e'
  lc.setRow(0, 4, B00000101);  // 'r'
  lc.setRow(0, 5, B00111011);  // 'y'
  delay(1500);

  int batteryLevel = getBattery();
  lc.setRow(0, 0, B00000000);
  lc.setRow(0, 1, B00000000);
  lc.setRow(0, 2, B00000000);
  if (batteryLevel < 0) {
    // Pas de mesure fiable (alim USB sans diviseur) -> "USb"
    lc.setRow(0, 3, B00111110);  // U
    lc.setRow(0, 4, B01011011);  // S
    lc.setRow(0, 5, B00011111);  // b
  } else {
    lc.setDigit(0, 3, batteryLevel / 100, false);
    lc.setDigit(0, 4, (batteryLevel % 100) / 10, false);
    lc.setDigit(0, 5, batteryLevel % 10, false);
  }
  lc.setRow(1, 0, B00000000);
  lc.setRow(1, 1, B00000000);
  lc.setRow(1, 2, B00000000);
  delay(2500);

  totalsectime = totalsectime_save;
  lc.clearDisplay(0);
  lc.clearDisplay(1);
  running = true;
}

void zap() {
  menu = 5;
}
