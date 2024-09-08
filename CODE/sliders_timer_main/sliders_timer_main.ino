#include "LedControl.h"
#include "OneButton.h"
#include "Adafruit_NeoPixel.h"
#include "DFPlayer_Mini_Mp3.h"

#define BP_START A0
#define BP_UP A1
#define BP_DOWN A2
#define ADAFRUITPIN 4
#define BP_VORTEX A5
#define BATTERYPIN A7

struct LED
{
    int pinState;
    unsigned long previousMillis;
    unsigned long interval;
    int matrixIndex;
    bool isColon;
    int colIndex;
    bool isArduinoLED;
    int arduinoPin;
};

struct Bargraphe
{
    const uint64_t *images;
    int length;
    int index;
    int delayTime;
};

Adafruit_NeoPixel strip = Adafruit_NeoPixel(7, ADAFRUITPIN, NEO_GRB + NEO_KHZ800);
int val = 0;
int colorVal = 0;
int reading = 0;
int x;
int prevVal = 0;
boolean lastBtn = LOW;
boolean NeopixelColor = false;
boolean lastButton = LOW;

// Lissage de la valeur du potentiomètre
const int numReadings = 10;
int readings[numReadings];
int readIndex = 0;
int total = 0;
int average = 0;

uint32_t Red = strip.Color(255, 0, 0);
uint32_t Off = strip.Color(0, 0, 0);

#define TEMPS_ANTI_REBOND_MS 30
LedControl lc = LedControl(12, 11, 10, 2);

const int pot1 = A6;
const int speaker = 5;
const int etatarduino = 13;

OneButton button1(A4, true);
OneButton button2(A3, true);

volatile byte running = false;

unsigned long totalsectime_slide = 0;
unsigned long totalsectime_reste = 0;
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
float b = 0;

int i = 0;
int k1 = 0;
int menu = 0;
int buzzerState = 1;

void setup()
{
    Serial.begin(9600);
    mp3_set_serial(Serial);
    mp3_set_volume(30);
    mp3_set_EQ(0);

    button1.attachClick(click1);
    button1.attachDoubleClick(doubleclick1);
    button1.attachDuringLongPress(longPress1);

    button2.attachClick(click2);
    button2.attachDoubleClick(doubleclick2);
    button2.attachDuringLongPress(longPress2);

    int devices = lc.getDeviceCount();
    for (int address = 0; address < devices; address++)
    {
        lc.shutdown(address, false);
        lc.setIntensity(0, 15);
        lc.setIntensity(1, 15);
        lc.clearDisplay(address);
        lc.setScanLimit(0, 7);
        lc.setScanLimit(1, 7);
    }

    for (int i = 0; i < numReadings; i++)
    {
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
}

LED redLED = {HIGH, 0, 600, 4, false, 0, false, -1};
LED yellowLED = {HIGH, 0, 250, 3, false, 0, false, -1};
LED greenLED = {HIGH, 0, 200, 5, false, 0, false, -1};
LED colonLED = {HIGH, 0, 150, 0, true, 6, false, -1};
LED etatArduinoLED = {HIGH, 0, 100, -1, false, 0, true, etatarduino};

void blinkLEDUnified(LED &led)
{
    unsigned long currentMillis = millis();

    if (currentMillis - led.previousMillis >= led.interval)
    {
        led.previousMillis = currentMillis;

        led.pinState = !led.pinState;

        if (led.isColon)
        {
            lc.setLed(0, led.colIndex, 1, led.pinState);
            lc.setLed(0, led.colIndex, 2, led.pinState);
            lc.setLed(0, led.colIndex, 3, led.pinState);
            lc.setLed(0, led.colIndex, 4, led.pinState);
        }
        else if (!led.isColon && !led.isArduinoLED)
        {
            lc.setLed(1, 5, led.matrixIndex, led.pinState);
        }
        else if (led.isArduinoLED)
        {
            digitalWrite(led.arduinoPin, led.pinState);
        }
    }
}

void ecranBlinkUnified(bool allSegments, int bloc = 0, int ecran = 0)
{
    static int stateecran = 0;
    static unsigned int ecrantime = 250;
    static unsigned int ecraninterval = 500;
    static unsigned long lasttimeecran = 0;
    unsigned long mecran = millis();

    if (stateecran == 0)
    {
        if (mecran >= lasttimeecran + ecraninterval)
        {
            stateecran = 1;
            showTime();
            lasttimeecran = mecran;
        }
        return;
    }
    if (stateecran == 1)
    {
        if (mecran >= lasttimeecran + ecrantime)
        {
            if (allSegments)
            {
                for (int i = 0; i < 8; i++)
                {
                    lc.setRow(0, i, false);
                    lc.setRow(1, i, false);
                }
            }
            else
            {
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

void updatespeaker_pattern(int interval, int numBips, int pause)
{
    static int state = 0;
    static unsigned int beeptime = 50;
    static unsigned long lasttimeon = 0;
    static unsigned int bipCount = 0;
    unsigned long m = millis();

    int timeForBips = numBips * interval;
    int totalTime = timeForBips + pause;

    if (bipCount >= numBips)
    {
        if (m >= lasttimeon + pause)
        {
            bipCount = 0;
            state = 0;
        }
        return;
    }

    if (state == 0)
    {
        if (m >= lasttimeon + interval)
        {
            state = 1;
            customTone(2700);
            lasttimeon = m;
        }
        return;
    }

    if (state == 1)
    {
        if (m >= lasttimeon + beeptime)
        {
            noTone(speaker);
            state = 0;
            lasttimeon = m;
            bipCount++;
        }
        return;
    }
}

void startMusique()
{
    int frequencies[] = {5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 3500, 3500, 2500, 1500};
    int durations[] = {150, 150, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 100, 500, 500, 500};
    int delays[] =    {200, 200, 10,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 100, 250, 250, 250};

    int sequenceLength = sizeof(frequencies) / sizeof(frequencies[0]);

    for (int i = 0; i < sequenceLength; i++) {
        customTone(frequencies[i], durations[i]);
        delay(delays[i]);
    }

    noTone(speaker);
    delay(250);  
}

void genserSequence()
{
    // GEnSEr
    // G
    lc.setRow(0, 0, B01011110);
    // E
    lc.setChar(0, 1, 'E', false);
    // n
    lc.setRow(0, 2, 0x15);
    // S(5)
    lc.setChar(0, 3, '5', false);
    // E
    lc.setChar(0, 4, 'E', false);
    // r
    lc.setRow(0, 5, 0x05);
    // bug
    lc.setRow(1, 3, B10001000);
    lc.setRow(1, 4, B10001000);
    lc.setRow(1, 0, B01001001);
    lc.setRow(1, 1, B00000011);
    lc.setRow(1, 2, B01011110);
    // bip
    customTone(5500, 50);

    delay(400);
    // clear
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    noTone(speaker);
    delay(40);

    // random 1
    lc.setRow(0, 0, B01001001);
    lc.setRow(0, 1, B00010101);
    lc.setRow(0, 2, B00100011);
    lc.setRow(0, 3, B00000011);
    lc.setRow(0, 4, B01010100);
    lc.setRow(0, 5, B01000011);
    lc.setRow(1, 3, B10101010);
    lc.setRow(1, 4, B10001010);
    lc.setRow(1, 0, B00000101);
    lc.setRow(1, 1, B01000000);
    lc.setRow(1, 2, B01000000);
    customTone(5500, 50);

    delay(200);
    // clear
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    noTone(speaker);
    delay(40);

    // CAluri
    // C
    lc.setRow(0, 0, B01001110);
    // A
    lc.setChar(0, 1, 'A', false);
    // l(lowercase "L")
    lc.setRow(0, 2, B00110000);
    // u
    lc.setRow(0, 3, B00011100);
    // r
    lc.setRow(0, 4, B00000101);
    // i
    lc.setRow(0, 5, B00010000);
    lc.setRow(1, 3, B10000010);
    lc.setRow(1, 4, B00001010);
    lc.setRow(1, 0, B01000011);
    lc.setRow(1, 1, B00001000);
    lc.setRow(1, 2, B01010100);
    // bip
    customTone(5500, 50);

    delay(200);
    // clear
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    noTone(speaker);
    delay(40);

    // 4th Sequence
    lc.setRow(0, 0, B00100011);
    lc.setRow(0, 1, B00000011);
    lc.setRow(0, 2, B01010100);
    lc.setRow(0, 3, B01000011);
    lc.setRow(0, 4, B00010011);
    lc.setRow(0, 5, B00011001);
    lc.setRow(1, 3, B10001001);
    lc.setRow(1, 4, B01001010);
    lc.setRow(1, 0, B00110000);
    lc.setRow(1, 1, B01000001);
    lc.setRow(1, 2, B01001110);
    customTone(5500, 50);

    delay(200);
    // clear
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    noTone(speaker);
    delay(40);

    // Second CAluri sequence
    // C
    lc.setRow(0, 0, B01001110);
    // A
    lc.setChar(0, 1, 'A', false);
    // l(lowercase "L")
    lc.setRow(0, 2, B00110000);
    // u
    lc.setRow(0, 3, B00011100);
    // r
    lc.setRow(0, 4, B00000101);
    // i
    lc.setRow(0, 5, B00010000);
    lc.setRow(1, 3, B00101001);
    lc.setRow(1, 4, B00001010);
    lc.setRow(1, 0, B00101010);
    lc.setRow(1, 1, B00011010);
    lc.setRow(1, 2, B00011010);
    // bip
    customTone(5500, 50);

    delay(200);
    // clear
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    noTone(speaker);
    delay(40);

    // 6th Sequence
    lc.setRow(0, 0, B00101010);
    lc.setRow(0, 1, B00000011);
    lc.setRow(0, 2, B00011010);
    lc.setRow(0, 3, B01001000);
    lc.setRow(0, 4, B01000001);
    lc.setRow(0, 5, B01001001);
    lc.setRow(1, 3, B00101001);
    lc.setRow(1, 4, B00001010);
    lc.setRow(1, 0, B00011001);
    lc.setRow(1, 1, B00001001);
    lc.setRow(1, 2, B00010011);
    customTone(5500, 50);

    delay(200);
    // clear
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    noTone(speaker);
    delay(40);

    // 6th Sequence
    lc.setRow(0, 0, B00010011);
    lc.setRow(0, 1, B00011001);
    lc.setRow(0, 2, B00100010);
    lc.setRow(0, 3, B00011000);
    lc.setRow(0, 4, B01000001);
    lc.setRow(0, 5, B00100101);
    lc.setRow(1, 3, B10010101);
    lc.setRow(1, 4, B00100100);
    lc.setRow(1, 0, B00110011);
    lc.setRow(1, 1, B00110111);
    lc.setRow(1, 2, B10001001);
    customTone(5500, 50);

    delay(200);
    // clear
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    noTone(speaker);
    delay(40);

    // KEnnY
    // K
    lc.setRow(0, 0, B00110111);
    // E
    lc.setChar(0, 1, 'E', false);
    // n
    lc.setRow(0, 2, 0x15);
    // n
    lc.setRow(0, 3, 0x15);
    // Y
    lc.setRow(0, 4, B00110011);
    //-
    lc.setRow(0, 5, B00000001);
    // bug
    lc.setRow(1, 3, B10001001);
    lc.setRow(1, 4, B10001001);
    lc.setRow(1, 0, B00100001);
    lc.setRow(1, 1, B00010001);
    lc.setRow(1, 2, B01000001);
    // bip
    customTone(5500, 50);

    delay(350);
    // clear
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    noTone(speaker);
    delay(75);
}

void displayFade()
{
    delay(100);
    lc.setRow(0, 0, B01111000);
    lc.setRow(0, 1, B01111110);
    lc.setRow(0, 2, B01111110);
    lc.setRow(0, 3, B01111110);
    lc.setRow(0, 4, B01111110);
    lc.setRow(0, 5, B00011110);
    lc.setRow(1, 3, B11111111);
    lc.setRow(1, 4, B11111111);
    lc.setRow(1, 0, B01111111);
    lc.setRow(1, 1, B01111111);
    lc.setRow(1, 2, B01111111);
    delay(30);
    lc.setRow(0, 0, B00110000);
    lc.setRow(0, 1, B01111110);
    lc.setRow(0, 2, B01111110);
    lc.setRow(0, 3, B01111110);
    lc.setRow(0, 4, B10000000);
    lc.setRow(0, 5, B00000110);
    lc.setRow(1, 3, B11101111);
    lc.setRow(1, 4, B11101111);
    lc.setRow(1, 0, B00111111);
    lc.setRow(1, 1, B00111111);
    lc.setRow(1, 2, B00111111);
    delay(30);
    lc.setRow(0, 0, B00000000);
    lc.setRow(0, 1, B01111110);
    lc.setRow(0, 2, B01111110);
    lc.setRow(0, 3, B01111110);
    lc.setRow(0, 4, B01111110);
    lc.setRow(0, 5, B00000000);
    lc.setRow(1, 3, B11100111);
    lc.setRow(1, 4, B11100111);
    lc.setRow(1, 0, B00101111);
    lc.setRow(1, 1, B00101111);
    lc.setRow(1, 2, B00101111);
    delay(30);
    lc.setRow(0, 0, B00000000);
    lc.setRow(0, 1, B01111000);
    lc.setRow(0, 2, B01111110);
    lc.setRow(0, 3, B01111110);
    lc.setRow(0, 4, B01000000);
    lc.setRow(0, 5, B00000000);
    lc.setRow(1, 3, B11000111);
    lc.setRow(1, 4, B11000111);
    lc.setRow(1, 0, B00101011);
    lc.setRow(1, 1, B00101011);
    lc.setRow(1, 2, B00101011);
    delay(30);
    lc.setRow(0, 0, B00000000);
    lc.setRow(0, 1, B00110000);
    lc.setRow(0, 2, B01111110);
    lc.setRow(0, 3, B01111110);
    lc.setRow(0, 4, B00000110);
    lc.setRow(0, 5, B00000000);
    lc.setRow(1, 3, B11000011);
    lc.setRow(1, 4, B11000011);
    lc.setRow(1, 0, B00001011);
    lc.setRow(1, 1, B00001011);
    lc.setRow(1, 2, B00001011);
    delay(30);
    lc.setRow(0, 0, B00000000);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B01111110);
    lc.setRow(0, 3, B01111110);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    lc.setRow(1, 3, B10000011);
    lc.setRow(1, 4, B10000011);
    lc.setRow(1, 0, B00001001);
    lc.setRow(1, 1, B00001001);
    lc.setRow(1, 2, B00001001);
    delay(55);
    lc.setRow(0, 0, B00000000);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B01111000);
    lc.setRow(0, 3, B01000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    lc.setRow(1, 3, B10000001);
    lc.setRow(1, 4, B10000001);
    lc.setRow(1, 0, B00000001);
    lc.setRow(1, 1, B00000001);
    lc.setRow(1, 2, B00000001);
    delay(30);
    lc.setRow(0, 0, B00000000);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B00110000);
    lc.setRow(0, 3, B00000110);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    lc.setRow(1, 3, B00000001);
    lc.setRow(1, 4, B00000001);
    lc.setRow(1, 0, B00000000);
    lc.setRow(1, 1, B00000000);
    lc.setRow(1, 2, B00000000);
    delay(30);
    lc.setRow(0, 0, B00000000);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B00000000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    lc.setRow(1, 3, B00000000);
    lc.setRow(1, 4, B00000000);
    lc.setRow(1, 0, B00000000);
    lc.setRow(1, 1, B00000000);
    lc.setRow(1, 2, B00000000);
    delay(1000);
}

void displayWrap()
{
    delay(30);
    customTone(5500, 100);
    lc.setRow(0, 0, B00000010);
    lc.setRow(0, 1, B00000010);
    lc.setRow(0, 2, B00000010);
    lc.setRow(0, 3, B00000010);
    lc.setRow(0, 4, B00000010);
    lc.setRow(0, 5, B00000010);
    lc.setRow(1, 3, B01010011);
    lc.setRow(1, 4, B01011010);
    noTone(speaker);
    delay(30);
    customTone(5500, 100);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B01000000);
    lc.setRow(0, 4, B01000000);
    lc.setRow(0, 5, B01000000);
    lc.setRow(1, 3, B01101001);
    lc.setRow(1, 4, B01000000);
    lc.setRow(1, 0, B00000010);
    lc.setRow(1, 1, B00000010);
    lc.setRow(1, 2, B00000010);
    noTone(speaker);
    delay(30);
    customTone(5500, 100);
    lc.setRow(0, 0, B00100000);
    lc.setRow(0, 1, B00100000);
    lc.setRow(0, 2, B00100000);
    lc.setRow(0, 3, B00100000);
    lc.setRow(0, 4, B00100000);
    lc.setRow(0, 5, B00100000);
    lc.setRow(1, 3, B00100101);
    lc.setRow(1, 4, B01001000);
    lc.setRow(1, 0, B00000100);
    lc.setRow(1, 1, B00000100);
    lc.setRow(1, 2, B00000100);
    noTone(speaker);
    delay(30);
    customTone(5500, 100);
    lc.setRow(0, 0, B00010000);
    lc.setRow(0, 1, B00010000);
    lc.setRow(0, 2, B00010000);
    lc.setRow(0, 3, B00010000);
    lc.setRow(0, 4, B00010000);
    lc.setRow(0, 5, B00010000);
    lc.setRow(1, 3, B10100101);
    lc.setRow(1, 4, B11011000);
    lc.setRow(1, 0, B00001000);
    lc.setRow(1, 1, B00001000);
    lc.setRow(1, 2, B00001000);
    noTone(speaker);
    delay(30);
    customTone(5500, 100);
    lc.setRow(0, 0, B00001000);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B00001000);
    lc.setRow(0, 4, B00001000);
    lc.setRow(0, 5, B00001000);
    lc.setRow(1, 3, B10100101);
    lc.setRow(1, 4, B11011010);
    lc.setRow(1, 0, B00010000);
    lc.setRow(1, 1, B00010000);
    lc.setRow(1, 2, B00010000);
    noTone(speaker);
    delay(30);
    customTone(5500, 100);
    lc.setRow(0, 0, B00000100);
    lc.setRow(0, 1, B00000100);
    lc.setRow(0, 2, B00000100);
    lc.setRow(0, 3, B00000100);
    lc.setRow(0, 4, B00000100);
    lc.setRow(0, 5, B00000100);
    lc.setRow(1, 3, B10001101);
    lc.setRow(1, 4, B10101010);
    lc.setRow(1, 0, B00100000);
    lc.setRow(1, 1, B00100000);
    lc.setRow(1, 2, B00100000);
    noTone(speaker);
    delay(30);
    customTone(5500, 100);
    lc.setRow(0, 0, B00000010);
    lc.setRow(0, 1, B00000010);
    lc.setRow(0, 2, B00000010);
    lc.setRow(0, 3, B00000010);
    lc.setRow(0, 4, B00000010);
    lc.setRow(0, 5, B00000010);
    lc.setRow(1, 3, B01010111);
    lc.setRow(1, 4, B00110110);
    lc.setRow(1, 0, B01000000);
    lc.setRow(1, 1, B01000000);
    lc.setRow(1, 2, B01000000);
    noTone(speaker);
    delay(30);
    customTone(5500, 100);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B01000000);
    lc.setRow(0, 4, B01000000);
    lc.setRow(0, 5, B01000000);
    lc.setRow(1, 3, B01010100);
    lc.setRow(1, 4, B10110110);
    noTone(speaker);
    delay(30);
    customTone(5500, 100);
    lc.setRow(0, 0, B00100000);
    lc.setRow(0, 1, B00100000);
    lc.setRow(0, 2, B00100000);
    lc.setRow(0, 3, B00100000);
    lc.setRow(0, 4, B00100000);
    lc.setRow(0, 5, B00100000);
    lc.setRow(1, 3, B01000100);
    lc.setRow(1, 4, B10000010);
    lc.setRow(1, 0, B00000010);
    lc.setRow(1, 1, B00000010);
    lc.setRow(1, 2, B00000010);
    noTone(speaker);
    delay(30);
    customTone(5500, 100);
    lc.setRow(0, 0, B00010000);
    lc.setRow(0, 1, B00010000);
    lc.setRow(0, 2, B00010000);
    lc.setRow(0, 3, B00010000);
    lc.setRow(0, 4, B00010000);
    lc.setRow(0, 5, B00010000);
    lc.setRow(1, 3, B01010101);
    lc.setRow(1, 4, B10000100);
    lc.setRow(1, 0, B00000100);
    lc.setRow(1, 1, B00000100);
    lc.setRow(1, 2, B00000100);
    noTone(speaker);
    delay(30);
    customTone(2500, 100);
    lc.setRow(0, 0, B00001000);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B00001000);
    lc.setRow(0, 4, B00001000);
    lc.setRow(0, 5, B00001000);
    lc.setRow(1, 3, B00100101);
    lc.setRow(1, 4, B10001101);
    lc.setRow(1, 0, B00001000);
    lc.setRow(1, 1, B00001000);
    lc.setRow(1, 2, B00001000);
    noTone(speaker);
    delay(30);
    customTone(2500, 100);
    lc.setRow(0, 0, B00000100);
    lc.setRow(0, 1, B00000100);
    lc.setRow(0, 2, B00000100);
    lc.setRow(0, 3, B00000100);
    lc.setRow(0, 4, B00000100);
    lc.setRow(0, 5, B00000100);
    lc.setRow(1, 3, B01010111);
    lc.setRow(1, 4, B10110110);
    lc.setRow(1, 0, B00010000);
    lc.setRow(1, 1, B00010000);
    lc.setRow(1, 2, B00010000);
    noTone(speaker);
    delay(30);
    customTone(2500, 100);
    lc.setRow(0, 0, B00000010);
    lc.setRow(0, 1, B00000010);
    lc.setRow(0, 2, B00000010);
    lc.setRow(0, 3, B00000010);
    lc.setRow(0, 4, B00000010);
    lc.setRow(0, 5, B00000010);
    lc.setRow(1, 3, B01000100);
    lc.setRow(1, 4, B01001000);
    lc.setRow(1, 0, B00100000);
    lc.setRow(1, 1, B00100000);
    lc.setRow(1, 2, B00100000);
    noTone(speaker);
    delay(30);
    customTone(2500, 100);
    lc.setRow(1, 3, B01010101);
    lc.setRow(1, 4, B10000100);
    lc.setRow(1, 0, B01000000);
    lc.setRow(1, 1, B01000000);
    lc.setRow(1, 2, B01000000);
    noTone(speaker);
    delay(30);
    showTime();
}

void wrapBurnout()
{
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B00000000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    lc.setRow(1, 0, B01101101);
    lc.setRow(1, 1, B11111011);
    lc.setRow(1, 2, B00111011);
    lc.setRow(1, 6, B01111000);
    lc.setRow(1, 3, B00000000);
    lc.setRow(1, 4, B00000000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B00000000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B01000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B01000000);
    lc.setRow(0, 4, B01000000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B01000000);
    lc.setRow(0, 4, B01000000);
    lc.setRow(0, 5, B01000000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B01000000);
    lc.setRow(0, 4, B01000000);
    lc.setRow(0, 5, B01100000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B01000000);
    lc.setRow(0, 4, B01000000);
    lc.setRow(0, 5, B01110000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B01000000);
    lc.setRow(0, 4, B01000000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B01000000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01000000);
    lc.setRow(0, 3, B01001000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01000000);
    lc.setRow(0, 2, B01001000);
    lc.setRow(0, 3, B01001000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B01000000);
    lc.setRow(0, 1, B01001000);
    lc.setRow(0, 2, B01001000);
    lc.setRow(0, 3, B01001000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B01001000);
    lc.setRow(0, 1, B01001000);
    lc.setRow(0, 2, B01001000);
    lc.setRow(0, 3, B01001000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B01001100);
    lc.setRow(0, 1, B01001000);
    lc.setRow(0, 2, B01001000);
    lc.setRow(0, 3, B01001000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B01001110);
    lc.setRow(0, 1, B01001000);
    lc.setRow(0, 2, B01001000);
    lc.setRow(0, 3, B01001000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B01001000);
    lc.setRow(0, 2, B01001000);
    lc.setRow(0, 3, B01001000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B01001000);
    lc.setRow(0, 3, B01001000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B01001000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B00001000);
    lc.setRow(0, 4, B01001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B00001000);
    lc.setRow(0, 4, B00001000);
    lc.setRow(0, 5, B01111000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B00001000);
    lc.setRow(0, 4, B00001000);
    lc.setRow(0, 5, B00111000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B00001000);
    lc.setRow(0, 4, B00001000);
    lc.setRow(0, 5, B00011000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B00001000);
    lc.setRow(0, 4, B00001000);
    lc.setRow(0, 5, B00001000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B00001000);
    lc.setRow(0, 4, B00001000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B00001000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00001000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00001000);
    lc.setRow(0, 2, B00000000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B00001110);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B00000000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B00000110);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B00000000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B00000010);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B00000000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    delay(50);
    lc.setRow(0, 0, B00000000);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B00000000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
}

// arg 1 : couleur, arg 2 : Vitesse, arg 3 : une seule led allumée, arg 4 : direction (true pour normal, false pour inverse)
void colorWipeUnified(uint32_t color, int speed = 100, bool single = false, bool forward = true)
{
    int start = forward ? 0 : strip.numPixels() - 1;
    int end = forward ? strip.numPixels() : -1;
    int step = forward ? 1 : -1;

    for (int i = start; i != end; i += step)
    {
        if (single)
            strip.clear();
        strip.setPixelColor(i, color);
        strip.show();
        delay(speed);
    }
}

void loop()
{
    button1.tick();
    button2.tick();

    debounceSTART();
    debounceVORTEX();

    delay(10);

    strip.show();
    blinkLEDUnified(etatArduinoLED);
    normal();

    if (digitalRead(BP_VORTEX) == LOW)
    {
        BP_VORTEX_STATUS = true;
    }
    if ((digitalRead(BP_UP) == LOW) && (digitalRead(BP_DOWN) == LOW))
    {
        mode_test();
    }
    if (digitalRead(BP_START) == true)
    {
        BP_START_STATUS = true;
    }
}

void (*reboot)(void) = 0;

void click1() { veille(); }
void click2()
{
    menu_up();
    buzzer_menu();
}

void longPress1() { reveille(); }
void longPress2() { reboot(); }

void doubleclick1() { zap(); }
void doubleclick2() { BP_POWER_STATUS = true; }

const uint64_t IMAGES_POST_FIN[] = {
    0x0000000000000000, 0x0000000102000000, 0x0000008106000000, 0x000000c10e000000,
    0x000000e11e000000, 0x000000f13e000000, 0x000000f97e000000, 0x000000fdfe000000,
    0x000000ffff000000, 0x000000fdfe000000, 0x000000f97e000000, 0x000000f13e000000,
    0x000000e11e000000, 0x000000c10e000000, 0x0000008106000000, 0x0000000102000000};

const uint64_t IMAGES_NORMAL[] = {
    0x0000000000000000, 0x0000000201000000, 0x0000000681000000, 0x0000000ec1000000,
    0x0000001ee1000000, 0x0000003ef1000000, 0x0000001ee1000000, 0x0000000ec1000000,
    0x0000001ee1000000, 0x0000003ef1000000, 0x0000007ef9000000, 0x000000fefd000000,
    0x000000ffff000000, 0x000000fefd000000, 0x0000007ef9000000, 0x0000003ef1000000,
    0x0000001ee1000000, 0x0000000ec1000000, 0x0000001ee1000000, 0x0000003ef1000000,
    0x0000001ee1000000, 0x0000000ec1000000, 0x0000000681000000, 0x0000000201000000};

const uint64_t IMAGES_FIN[] = {
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
    0x0000003ffc000000, 0x0000009ff9000000, 0x000000cff3000000, 0x000000e7e7000000};

const uint64_t IMAGES_OFF[] = {
    0x0000000000000000};

Bargraphe bargraphePostFin = {IMAGES_POST_FIN, sizeof(IMAGES_POST_FIN) / 8, 0, 15};
Bargraphe bargrapheNormal = {IMAGES_NORMAL, sizeof(IMAGES_NORMAL) / 8, 0, 20};
Bargraphe bargrapheFin = {IMAGES_FIN, sizeof(IMAGES_FIN) / 8, 0, 15};
Bargraphe bargrapheOff = {IMAGES_OFF, sizeof(IMAGES_OFF) / 8, 0, 0};

void displayImage(const uint64_t image)
{
    for (int row = 3; row < 5; row++)
    {
        byte rowData = (image >> (row * 8)) & 0xFF;
        for (int col = 0; col < 8; col++)
        {
            lc.setLed(1, row, col, bitRead(rowData, col));
        }
    }
}

void animateBargraphe(Bargraphe &bargraphe)
{
    displayImage(bargraphe.images[bargraphe.index]);
    if (++bargraphe.index >= bargraphe.length)
    {
        bargraphe.index = 0;
    }
    delay(bargraphe.delayTime);
}

#define secondsinaday 86400
#define secondsinhour 3600
#define secondsinminute 60

void showTime()
{
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

void delayIncrement(byte rst)
{
    static unsigned long last_time3 = 0;
    unsigned long time_now3 = millis();

    if (rst)
    {
        last_time3 = time_now3;
        return;
    }

    if (time_now3 - last_time3 >= 1000)
    {
        delay(100);
    }
    else
    {
        delay(150);
    }
}

void handleMenu(int menu, int increment, int digitPosition, bool isDayMenu = false)
{
    button1.tick();
    button2.tick();

    ecranBlinkUnified(false, isDayMenu ? 1 : 0, digitPosition);

    if (digitalRead(BP_UP) == LOW)
    {
        customTone(3000, 50);

        if (isDayMenu)
        {
            unsigned long currentDays = totalsectime / 86400;
            currentDays++;
            totalsectime = (currentDays * 86400) + (totalsectime % 86400);
        }
        else
        {
            totalsectime += increment;
        }

        delayIncrement(false);
        showTime();
    }
    else
    {
        delayIncrement(true);
    }

    if (digitalRead(BP_DOWN) == LOW)
    {
        customTone(2900, 50);

        if (isDayMenu)
        {
            unsigned long currentDays = totalsectime / 86400;
            if (currentDays > 0)
            {
                currentDays--;
            }
            totalsectime = (currentDays * 86400) + (totalsectime % 86400);
        }
        else
        {
            totalsectime -= increment;
        }

        delayIncrement(false);
        showTime();
    }
    else
    {
        delayIncrement(true);
    }
}

void buttonStart()
{
    running = !running;
}

void normal()
{
    // mode 0=normal 1=burnout 2=foutu
    byte vortex = 0;

    // mise en route minuteur par double click BP_POWER
    if (BP_POWER_STATUS == true)
    {
        while (i == 0)
        {
            strip.show();
            BP_VORTEX_STATUS = false;
            startMusique();
            genserSequence();
            displayWrap();
            lc.clearDisplay(0);
            lc.clearDisplay(1);
            showTime();
            lc.setLed(0, 6, 1, true);
            lc.setLed(0, 6, 2, true);
            lc.setLed(0, 6, 3, true);
            lc.setLed(0, 6, 4, true);
            i++;
            byte vortex = 0;
            break;
        }

        if ((running) && (totalsectime_slide != 0))
        {
            mp3_play();
            while ((vortex == 0) && (BP_START_STATUS == true) && (totalsectime_slide != 0))
            {
                animation_normal();
                button1.tick();
                button2.tick();

                static unsigned long last_time = 0;
                unsigned long time_now = millis();

                unsigned long totalsectime_save;
                unsigned long totalsectime_reste = totalsectime_slide - totalsectime;

                if (time_now - last_time >= 1000)
                {
                    totalsectime++;
                    last_time = time_now;
                    showTime();
                }

                if (totalsectime_reste <= 0)
                {
                    mp3_play();
                    totalsectime = 0;
                    totalsectime_slide = 0;
                    totalsectime_reste = 0;
                    colorWipeUnified(Red, 100, false, true);
                    colorWipeUnified(Off, 100, false, false);
                    displayFade();
                    displayWrap();
                    animateBargraphe(bargrapheOff);
                    lc.setLed(1, 5, 5, false);
                    lc.setLed(1, 5, 3, false);
                    lc.setLed(1, 5, 4, false);
                    lc.setLed(1, 7, 2, false);
                    lc.setLed(1, 7, 3, false);
                    delay(200);
                    showTime();
                    running = false;
                    BP_START_STATUS = false;
                }

                if (digitalRead(BP_VORTEX) == LOW)
                {
                    mp3_play();
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
                }

                if (digitalRead(BP_UP) == LOW)
                {
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

                if ((digitalRead(BP_DOWN) == LOW) && (running == true))
                {
                    batterie();
                }

                if ((digitalRead(BP_START) == LOW) && (running == true))
                {
                    buzzerState = !buzzerState;
                }
            }
        }

        if (vortex == 1)
        {
            while (vortex == 1)
            {
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

                if (time_now1 - last_time1 >= 1000)
                {
                    totalsectime--;
                    last_time1 = time_now1;
                    showTime();
                }

                if ((totalsectime >= 0 && totalsectime <= 1) && (digitalRead(BP_VORTEX) == LOW))
                {
                    mp3_play();
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
                    totalsectime = random(16756131); // random
                }

                if (totalsectime <= 0)
                {
                    showTime();
                    vortex = 2;
                    colorWipeUnified(Off, 100, false, false);
                    displayFade();
                    totalsectime = 0;
                    startMusique();
                }

                if ((digitalRead(BP_DOWN) == LOW) && (running == true))
                {
                    batterie();
                }

                if ((digitalRead(BP_START) == LOW) && (running == true))
                {
                    buzzerState = !buzzerState;
                }
            }
        }

        if (vortex == 2)
        {
            while (vortex == 2)
            {
                button2.tick();
                noTone(speaker);
                wrapBurnout();
                animation_29();
                vortex = 2;

                if (digitalRead(BP_DOWN) == LOW)
                {
                    batterie();
                }

                if ((digitalRead(BP_START) == LOW) && (running == true))
                {
                    buzzerState = !buzzerState;
                }
            }
        }

        else
        {
            adafruit();
            lc.setRow(1, 6, B01111000);

            while (!running && menu == 1)
            {
                handleMenu(1, 1, 5); // Incrémenter/décrémenter les secondes, digit des secondes
            }

            while (!running && menu == 2)
            {
                handleMenu(2, 60, 3); // Incrémenter/décrémenter les minutes, digit des minutes
            }

            while (!running && menu == 3)
            {
                handleMenu(3, 3600, 1); // Incrémenter/décrémenter les heures, digit des heures
            }

            while (!running && menu == 4)
            {
                handleMenu(4, 1, 2, true); // Incrémenter/décrémenter les jours, digit des jours (isDayMenu = true)
            }

            while (!running && menu == 5)
            {
                button1.tick();
                button2.tick();
                ecranBlinkUnified(true);
                totalsectime_slide = totalsectime;
            }

            while (!running && menu == 6)
            {
                totalsectime = 0;
                delay(150);
                showTime();
                menu = 0;

                if (totalsectime_slide != 0)
                {
                    lc.setRow(0, 0, B01011011); // S
                    lc.setRow(0, 1, B00001110); // L
                    lc.setRow(0, 2, B00110000); // I
                    lc.setRow(0, 3, B01111110); // D
                    lc.setRow(0, 4, B01001111); // E
                    lc.setRow(0, 5, B00000000);
                }
                else
                {
                    lc.setRow(0, 0, B01001111); // E
                    lc.setRow(0, 1, B00010101); // r
                    lc.setRow(0, 2, B00010101); // r
                    lc.setRow(0, 3, B00011101); // o
                    lc.setRow(0, 4, B00010101); // r
                    lc.setRow(0, 5, B10000000);
                }
            }
        }
    }
}

void animation_burnout()
{
    adafruit();
    onadafruit();
    lc.setRow(1, 6, B01111000);

    if (totalsectime <= 30)
    {
        animateBargraphe(bargrapheFin);
    }
    else if ((totalsectime >= 30 && totalsectime <= 59))
    {
        animateBargraphe(bargraphePostFin);
    }
    else
    {
        animateBargraphe(bargrapheNormal);
    }

    if ((totalsectime >= 35 && totalsectime <= 59))
    {
        lc.setLed(1, 5, 5, true);
    }
    else
    {
        blinkLEDUnified(greenLED);
    }

    if ((totalsectime >= 15 && totalsectime <= 35))
    {
        lc.setLed(1, 5, 3, true);
    }
    else
    {
        blinkLEDUnified(yellowLED);
    }

    if ((totalsectime >= 0 && totalsectime <= 15))
    {
        lc.setLed(1, 5, 4, true);
    }
    else
    {
        blinkLEDUnified(redLED);
    }
    if (totalsectime == 10)
    {
        while (k1 == 0)
        {
            displayWrap();
            k1++;
        }
    }

    if (totalsectime > 10 && totalsectime <= 20)
    {
        updatespeaker_pattern(25, 5, 500);
    }
    else if (totalsectime > 0 && totalsectime <= 10)
    {
        updatespeaker_pattern(15, 10, 330);
    }
    else if (totalsectime == 0)
    {
        customTone(3000, 200);
    }
    else
    {
        updatespeaker_pattern(500, 2, 0);
    }

    if ((totalsectime <= 5))
    {
        lc.setLed(0, 6, 1, true);
        lc.setLed(0, 6, 2, true);
        lc.setLed(0, 6, 3, true);
        lc.setLed(0, 6, 4, true);
    }

    else
    {
        blinkLEDUnified(colonLED);
    }
}

void animation_normal()
{
    unsigned long totalsectime_reste = totalsectime_slide - totalsectime;
    animateBargraphe(bargraphePostFin);
    blinkLEDUnified(greenLED);
    blinkLEDUnified(yellowLED);
    blinkLEDUnified(redLED);
    blinkLEDUnified(colonLED);
    adafruit();
    lc.setRow(1, 6, B01111000);

    if ((totalsectime_reste >= 10) && (totalsectime_reste <= 20))
    {
        updatespeaker_pattern(25, 5, 500);
    }
    else if ((totalsectime_reste >= 1) && (totalsectime_reste < 10))
    {
        updatespeaker_pattern(15, 10, 330);
    }
    else if (totalsectime_reste <= 1)
    {
        customTone(3000, 1500);
    }
    else
    {
        updatespeaker_pattern(500, 2, 0);
    }

    if (totalsectime_reste == 6)
    {
        lc.setLed(1, 7, 2, true);
        lc.setLed(1, 7, 3, false);
    }
    if (totalsectime_reste == 5)
    {
        lc.setLed(1, 7, 2, false);
        lc.setLed(1, 7, 3, true);
    }
    if (totalsectime_reste == 4)
    {
        lc.setLed(1, 7, 2, true);
        lc.setLed(1, 7, 3, false);
    }
    if (totalsectime_reste == 3)
    {
        lc.setLed(1, 7, 2, false);
        lc.setLed(1, 7, 3, true);
    }
    if (totalsectime_reste <= 2)
    {
        lc.setLed(1, 7, 2, true);
        lc.setLed(1, 7, 3, true);
    }
    if (totalsectime_reste > 6)
    {
        lc.setLed(1, 7, 2, false);
        lc.setLed(1, 7, 3, false);
    }

    if (totalsectime <= 0)
    {
        lc.setLed(1, 7, 2, true);
        lc.setLed(1, 7, 3, true);
        customTone(3000, 1500);
        delay(1500);
    }
}

void animation_29()
{
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

void adafruit()
{
    total = total - readings[readIndex];
    readings[readIndex] = analogRead(pot1);
    total = total + readings[readIndex];
    readIndex = (readIndex + 1) % numReadings;
    average = total / numReadings;

    int newVal = map(average, 0, 1023, 0, 8);
    newVal = 8 - newVal;

    if (newVal != prevVal)
    {
        strip.setBrightness(10);

        for (x = 0; x < newVal; x++)
        {
            strip.setPixelColor(x, strip.Color(255, 0, 0));
        }

        for (x = newVal; x < strip.numPixels(); x++)
        {
            strip.setPixelColor(x, 0, 0, 0);
        }

        strip.show();
        prevVal = newVal;
    }
}

void offadafruit()
{
    strip.fill(strip.Color(0, 0, 0), 0, strip.numPixels());
    strip.show();
}

void onadafruit()
{
    for (int x = 0; x < prevVal; x++)
    {
        strip.setPixelColor(x, strip.Color(255, 0, 0));
    }

    for (int x = prevVal; x < strip.numPixels(); x++)
    {
        strip.setPixelColor(x, strip.Color(0, 0, 0));
    }

    strip.show();
}

void veille()
{
    lc.shutdown(1, HIGH);
    lc.shutdown(0, HIGH);
    offadafruit();
    buzzerState = 0;
}

void reveille()
{
    lc.shutdown(1, LOW);
    lc.shutdown(0, LOW);
    onadafruit();
    buzzerState = 1;
}

void debounceSTART()
{
    int reading = digitalRead(BP_START);
    if (reading != OLD_BP_START_STATUS)
    {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) >= debounceDelay)
    {
        if (reading != BP_START_STATUS)
        {
            BP_START_STATUS = reading;
        }
    }
    OLD_BP_START_STATUS = reading;
}

// antirebond bp vortex
void debounceVORTEX()
{
    int reading1 = digitalRead(BP_VORTEX);
    if (reading1 != OLD_BP_VORTEX_STATUS)
    {
        lastDebounceTime1 = millis();
    }
    if ((millis() - lastDebounceTime1) >= debounceDelay1)
    {
        if (reading1 != BP_VORTEX_STATUS)
        {
            BP_VORTEX_STATUS = reading1;
        }
    }
    OLD_BP_VORTEX_STATUS = reading1;
}

void mode_test()
{
    strip.show();
    lc.setRow(0, 0, B11111111); // ecrans hhmmss ddd et baregraphe
    lc.setRow(0, 1, B11111111);
    lc.setRow(0, 2, B11111111);
    lc.setRow(0, 3, B11111111);
    lc.setRow(0, 4, B11111111);
    lc.setRow(0, 5, B11111111);
    lc.setRow(1, 0, B11111111);
    lc.setRow(1, 1, B11111111);
    lc.setRow(1, 2, B11111111);
    lc.setRow(1, 3, B11111111);
    lc.setRow(1, 4, B11111111);
    lc.setLed(0, 6, 1, true); // colons
    lc.setLed(0, 6, 2, true);
    lc.setLed(0, 6, 3, true);
    lc.setLed(0, 6, 4, true);
    lc.setRow(1, 6, B01111000); // led extremiter chenillard
    lc.setLed(1, 5, 3, true);   // jaune
    lc.setLed(1, 5, 4, true);   // rouge
    lc.setLed(1, 5, 5, true);   // verte
    lc.setLed(1, 7, 4, true);   // emetteur
    lc.setLed(1, 7, 1, true);
    lc.setLed(1, 7, 2, true);
    lc.setLed(1, 7, 3, true);

    customTone(3000, 5000);
    strip.fill(strip.Color(255, 0, 0), 0, 7);

    delay(10000);

    lc.setRow(0, 0, B00000000);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B00000000);
    lc.setRow(0, 3, B00000000);
    lc.setRow(0, 4, B00000000);
    lc.setRow(0, 5, B00000000);
    lc.setRow(1, 0, B00000000);
    lc.setRow(1, 1, B00000000);
    lc.setRow(1, 2, B00000000);
    lc.setRow(1, 3, B00000000);
    lc.setRow(1, 4, B00000000);
    lc.setLed(0, 6, 1, false);
    lc.setLed(0, 6, 2, false);
    lc.setLed(0, 6, 3, false);
    lc.setLed(0, 6, 4, false);
    lc.setRow(1, 6, B00000000);
    lc.setLed(1, 5, 3, false);
    lc.setLed(1, 5, 4, false);
    lc.setLed(1, 5, 5, false);
    lc.setLed(1, 7, 4, false);
    lc.setLed(1, 7, 1, false);
    lc.setLed(1, 7, 2, false);
    lc.setLed(1, 7, 3, false);
    noTone(speaker);
    strip.fill(strip.Color(0, 0, 0), 0, 7);
}

void buzzer_menu()
{
    if ((!running) && (BP_POWER_STATUS == true))
    {
        customTone(3100, 200);
    }
}

void menu_up()
{
    if (BP_POWER_STATUS == true)
    {
        menu++;
    }
}

int getBattery()
{
    float lecture = analogRead(BATTERYPIN);
    float tension = (lecture / 1023.0) * 5.0; // Conversion en tension

    // Si en USB, ne pas calculer la batterie
    if (tension > 4.2)
    {
        return 100;
    }

    // Plage des valeurs
    float minValue = TensionMin;
    float maxValue = TensionMax;

    // Conversion en pourcentage
    float pourcentage = ((tension - minValue) / (maxValue - minValue)) * 100;

    // Limite entre 0 et 100
    pourcentage = constrain(pourcentage, 0, 100);

    return (int)pourcentage;
}

void batterie()
{
    noTone(speaker);
    running = false;
    unsigned long totalsectime_save = totalsectime;

    lc.clearDisplay(0);
    lc.setRow(0, 0, B00011111); // 'b'
    lc.setRow(0, 1, B01111101); // 'a'
    lc.setRow(0, 2, B00000111); // 't'
    lc.setRow(0, 3, B01101111); // 'e'
    lc.setRow(0, 4, B00000101); // 'r'
    lc.setRow(0, 5, B00111011); // 'y'
    delay(1500);

    //totalsectime = getBattery();
    //showTime();
    int batteryLevel = getBattery();
    lc.setRow(0, 0, B00000000);
    lc.setRow(0, 1, B00000000);
    lc.setRow(0, 2, B00000000);
    lc.setDigit(0, 3, batteryLevel / 100, false);
    lc.setDigit(0, 4, (batteryLevel % 100) / 10, false);
    lc.setDigit(0, 5, batteryLevel % 10, false);
    lc.setRow(1, 0, B00000000);
    lc.setRow(1, 1, B00000000);
    lc.setRow(1, 2, B00000000);
    delay(2500);

    totalsectime = totalsectime_save;
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    running = true;
}

void zap()
{
    menu = 5;
}