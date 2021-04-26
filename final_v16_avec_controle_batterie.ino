//librairies
#include "LedControl.h" //Gestion max7219
#include "OneButton.h" //Gestion BP1 et 2 multi fonctions (voir dans le temps)
#include "Adafruit_NeoPixel.h" //Gestion ring lumineux rouge

//câblage
#define BP_START A0   // bouton Start/Stop (interruption)
#define BP_UP A1      // bouton +
#define BP_DOWN A2    // bouton -
#define PIN 0         //adafruit
#define BP_VORTEX A5 //vortex
#define BATTERYPIN A7 //controle batterie

//configuration led adafruit
Adafruit_NeoPixel strip = Adafruit_NeoPixel (7, PIN, NEO_GRB + NEO_KHZ800);
int val = 0;
int colorVal = 0;
int reading = 0;
int x;
int prevVal = 0;
boolean lastBtn = LOW;
boolean NeopixelColor = false;
boolean lastButton = LOW;

// definition des couleurs
uint32_t Red = strip.Color(255,0,0);
uint32_t Off = strip.Color(0,0,0);

//temps "d'anti-rebond", en millisecondes, pendant lequel les rebonds
//seront ignorés
#define TEMPS_ANTI_REBOND_MS 30

//pin 12 DataIn 
//pin 11  CLK 
//pin 10 LOAD 
//Two MAX7221's
LedControl lc=LedControl(12,11,10,2);

//declaration cablage et noms I/O
const int pot1 = A6; //potentiometre
const int speaker = 5; //piezo
const int etatarduino = 13; //led etat veille

//entrees analogique multifonctions
OneButton button1(A4, true); //veille
OneButton button2(A3, true); //menu++

//variables globales 
volatile byte running = false; // Etat du minuteur (true = en fonctionnement, false = à l'arrêt)

//variables de temps
unsigned long totalsectime_slide = 0; //temps reglage mode normal
unsigned long totalsectime_reste = 0; //temps restant normal
unsigned long totalsectime = 0;

//variables anti rebond start
bool BP_START_STATUS = false;
bool OLD_BP_START_STATUS = false;
unsigned long lastDebounceTime  = 0;
unsigned long debounceDelay  = 50;

//variables anti rebond vortex
bool BP_VORTEX_STATUS = false;
bool OLD_BP_VORTEX_STATUS = false;
unsigned long lastDebounceTime1  = 0;
unsigned long debounceDelay1  = 50;
  
//variables bp power
bool BP_POWER_STATUS = false;

//variables batterie
const float TensionMin = 3.2; //tension min
const float TensionMax = 4.2; //tension max
float b = 0;

//variables divers
int i = 0;
int k1 = 0;
int menu = 0; //liste menu
int ecran = 0; //0 droite ++++ gauche
int bloc = 0; //max7219 0=hhmmss / 1=ddd

//declaration One button
void setup() {

  //lien bouton 1
  button1.attachClick(click1);
  button1.attachDoubleClick(doubleclick1);
  button1.attachDuringLongPress(longPress1);

  //lien bouton 2
  button2.attachClick(click2);
  button2.attachDoubleClick(doubleclick2);
  button2.attachDuringLongPress(longPress2);

  //configuration led controle
  int devices=lc.getDeviceCount();
  //initialisation
  for(int address=0;address<devices;address++) 
  {
  //desactivation veille
  lc.shutdown(address,false);
  //reglage luminositées
  lc.setIntensity(0,15);
  lc.setIntensity(1,15);
  //nettoyage ecrans
  lc.clearDisplay(address);
  //multilpexage
  lc.setScanLimit(0,7);
  lc.setScanLimit(1,7);
  }
  
  //sorties carte 
  //tout est controller par les max7219 sauf le buzzer
  pinMode(speaker, OUTPUT);
  pinMode(etatarduino, OUTPUT);
 
  //entrees analogique
  pinMode(pot1, INPUT); //controle led adafruit
  randomSeed(analogRead(A6)); //potentiometre pour configuration aleatoire du minuteur

  //configuration boutons
  pinMode(BP_UP, INPUT_PULLUP);
  pinMode(BP_DOWN, INPUT_PULLUP);
  pinMode(BP_START, INPUT_PULLUP);
  pinMode(BP_VORTEX, INPUT_PULLUP);
  attachInterrupt(1, bp_start_int, FALLING );

  //adafruit
  strip.begin();
  strip.show();
}

//clignotement LED tau/delta/zeta

//LED rouge
int redpinState = true; //etat ON
unsigned long previousredpinMillis = 0;
unsigned long redpininterval = 600;
void redpinBlink(){

    unsigned long currentredpinMillis = millis();

   if (currentredpinMillis - previousredpinMillis > redpininterval) {
   // sauvegarde etat led 
   previousredpinMillis = currentredpinMillis;   

   // si ON -> OFF et vice versa
   if (redpinState == false)
     redpinState = true;
   else
     redpinState = false;

   // alignement etat Led physique avec redpinState
   lc.setLed(1,5,4,redpinState); //max7219 n°2 digi 6
   }
}

//LED jaune
int yellowpinState = true; //etat ON
unsigned long previousyellowpinMillis = 0;
unsigned long yellowpininterval = 250;
void yellowpinBlink(){

    unsigned long currentyellowpinMillis = millis();

   if (currentyellowpinMillis - previousyellowpinMillis > yellowpininterval) {
   // sauvegarde etat led 
   previousyellowpinMillis = currentyellowpinMillis;   

   // si ON -> OFF et vice versa
   if (yellowpinState == false)
     yellowpinState = true;
   else
     yellowpinState = false;

   // alignement etat Led physique avec yellowpinState
   lc.setLed(1,5,3,yellowpinState); //max7219 n°2 digi 6
   }
}

//LED vert
int greenpinState = true; //etat ON
unsigned long previousgreenpinMillis = 0;
unsigned long greenpininterval = 200;
void greenpinBlink(){

    unsigned long currentgreenpinMillis = millis();

   if (currentgreenpinMillis - previousgreenpinMillis > greenpininterval) {
   // sauvegarde etat led 
   previousgreenpinMillis = currentgreenpinMillis;   

   // si ON -> OFF et vice versa
   if (greenpinState == false)
     greenpinState = true;
   else
     greenpinState = false;

   // alignement etat Led physique avec yellowpinState
   lc.setLed(1,5,5,greenpinState); //max7219 n°2 digi 6
   }
}

//colon LED (entre les ecrans)
int colonsState = true; //etat ON
unsigned long previouscolonsMillis = 0;
unsigned long colonsinterval = 150; 
void colonBlink(){

 unsigned long currentcolonsMillis = millis();
 
 if(currentcolonsMillis - previouscolonsMillis > colonsinterval) {
   // sauvegarde etat led 
   previouscolonsMillis = currentcolonsMillis;   

   // si ON -> OFF et vice versa
   if (colonsState == false)
     colonsState = true;
   
   else
     colonsState = false;

   // alignement etat Led physique avec yellowpinState
   lc.setLed(0,6,1,colonsState); //max7219 n°1 digi 7
   lc.setLed(0,6,2,colonsState);
   lc.setLed(0,6,3,colonsState);
   lc.setLed(0,6,4,colonsState);
 }
}

//clignotement led 13 arduino pour etat arduino
int etatState = HIGH;
unsigned long previousetatMillis = 0;
unsigned long etatinterval = 100; 
void etatBlink(){

 unsigned long currentetatMillis = millis();
 
 if(currentetatMillis - previousetatMillis > etatinterval) {
   // save the last time you blinked the LED 
   previousetatMillis = currentetatMillis;   

   // if the LED is off turn it on and vice-versa:
   if (etatState == LOW)
     etatState = HIGH;
   
   else
     etatState = LOW;

   // set the LED with the ledState of the variable:
   digitalWrite(etatarduino, etatState);
 }
}

//clignotement 7 segments reglage temps
void ecranBlink(){
 static int stateecran = 0;
 static unsigned int ecrantime = 250;
 static unsigned int ecraninterval = 500;
 static unsigned long lasttimeecran = 0;
 unsigned long mecran = millis();
 
 if (stateecran == 0) {
   if (mecran >= lasttimeecran + ecraninterval){
     stateecran = 1;
     affiche_temps();
     lasttimeecran = mecran;
   }
   return;
 }
 if (stateecran == 1) {
   if (mecran >= lasttimeecran + ecrantime){
     lc.setRow(bloc,ecran,false);
     stateecran = 0;
     ecrantime = 250;
   }
   return;
 }
}

//clignotement tout les 7 segments reglage temps
void ecranBlink1(){
 static int stateecran1 = 0;
 static unsigned int ecrantime1 = 250;
 static unsigned int ecraninterval1 = 500;
 static unsigned long lasttimeecran1 = 0;
 unsigned long mecran1 = millis();
 
 if (stateecran1 == 0) {
   if (mecran1 >= lasttimeecran1 + ecraninterval1){
     stateecran1 = 1;
     affiche_temps();
     lasttimeecran1 = mecran1;
   }
   return;
 }
 if (stateecran1 == 1) {
   if (mecran1 >= lasttimeecran1 + ecrantime1){
     lc.setRow(0,0,false);
     lc.setRow(0,1,false);
     lc.setRow(0,2,false);
     lc.setRow(0,3,false);
     lc.setRow(0,4,false);
     lc.setRow(0,5,false);
     lc.setRow(1,0,false);
     lc.setRow(1,1,false);
     lc.setRow(1,2,false);
     stateecran1 = 0;
     ecrantime1 = 250;
   }
   return;
 }
}

//buzzer normal
void updatespeaker(){
 static int state = 0;
 static unsigned int beeptime = 50;
 static unsigned int sinterval = 500;
 static unsigned int freq = 2700;
 static unsigned long lasttimeon = 0;
 unsigned long m = millis();
 
 if (state == -1){
   lasttimeon = m;
   state = 1;
   tone(speaker,1000);
   noTone(speaker);
   tone(speaker,1000);
   return;
 }
 if (state == 0){
   if (m >= lasttimeon + sinterval){
     state = 1;
     tone(speaker,freq);
     lasttimeon = m;
   }
   return;
 }
 if (state == 1){
   if (m >= lasttimeon + beeptime){
     noTone(speaker);
     state = 0;
     beeptime = 50;
   }
   return;
 }
}

//buzzer 20s
void updatespeaker1(){
 static int state1 = 0;
 static unsigned int beeptime1 = 25;
 static unsigned int sinterval1 = 200;
 static unsigned int freq1 = 2800;
 static unsigned long lasttimeon1 = 0;
 unsigned long m1 = millis();
 
 if (state1 == -1){
   lasttimeon1 = m1;
   state1 = 1;
   tone(speaker,1000);
   noTone(speaker);
   tone(speaker,1000);
   return;
 }
 if (state1 == 0){
   if (m1 >= lasttimeon1 + sinterval1){
     state1 = 1;
     tone(speaker,freq1);
     lasttimeon1 = m1;
   }
   return;
 }
 if (state1 == 1){
   if (m1 >= lasttimeon1 + beeptime1){
     noTone(speaker);
     state1 = 0;
     beeptime1 = 25;
   }
   return;
 }
}

//buzzer 10s
void updatespeaker2(){
 static int state2 = 0;
 static unsigned int beeptime2 = 10;
 static unsigned int sinterval2 = 100;
 static unsigned int freq2 = 2900;
 static unsigned long lasttimeon2 = 0;
 unsigned long m2 = millis();
 
 if (state2 == -1){
   lasttimeon2 = m2;
   state2 = 1;
   tone(speaker,1000);
   noTone(speaker);
   tone(speaker,1000);
   return;
 }
 if (state2 == 0) {
   if (m2 >= lasttimeon2 + sinterval2){
     state2 = 1;
     tone(speaker,freq2);
     lasttimeon2 = m2;
   }
   return;
 }
 if (state2 == 1){
   if (m2 >= lasttimeon2 + beeptime2){
     noTone(speaker);
     state2 = 0;
     beeptime2 = 10;
   }
   return;
 }
}

//Musique de demarrage
void musique() {

tone(speaker, 5500, 150);
delay(200);
tone(speaker, 5500, 150);
delay(200);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 5500, 10);
delay(10);
tone(speaker, 3500, 100);
delay(100);
tone(speaker, 3500, 500);
delay(250);
tone(speaker, 2500, 500);
delay(250);
tone(speaker, 1500, 500);
delay(250);
noTone(speaker);
delay(250);
}

//GENSER SEQUENCE + bug baregraphe
void genserOne() {  
 //GEnSEr
 //G  
 lc.setRow(0,0,B01011110);
 //E  
 lc.setChar(0,1,'E',false);
 //n
 lc.setRow(0,2,0x15);
 //S(5)
 lc.setChar(0,3,'5',false);
 //E
 lc.setChar(0,4,'E',false);
 //r
 lc.setRow(0,5,0x05);
 //bug
 lc.setRow(1,3,B10001000);
 lc.setRow(1,4,B10001000);
 lc.setRow(1,0,B01001001);
 lc.setRow(1,1,B00000011);
 lc.setRow(1,2,B01011110);
 //bip
 tone(speaker, 5500,50);
 
 delay(400);
 //clear
 lc.clearDisplay(0);
 lc.clearDisplay(1);
 noTone(speaker);
 delay(40);

 //random 1
 lc.setRow(0,0,B01001001);
 lc.setRow(0,1,B00010101);
 lc.setRow(0,2,B00100011);
 lc.setRow(0,3,B00000011);
 lc.setRow(0,4,B01010100);
 lc.setRow(0,5,B01000011);
 lc.setRow(1,3,B10101010);
 lc.setRow(1,4,B10001010);
 lc.setRow(1,0,B00000101);
 lc.setRow(1,1,B01000000);
 lc.setRow(1,2,B01000000);
 tone(speaker, 5500,50);
  
 delay(200);
 //clear
 lc.clearDisplay(0);
 lc.clearDisplay(1);
 noTone(speaker);
 delay(40);

 //CAluri
 //C
 lc.setRow(0,0,B01001110);
 //A
 lc.setChar (0,1,'A',false);
 //l(lowercase "L")
 lc.setRow(0,2,B00110000);
 //u
 lc.setRow(0,3,B00011100);
 //r
 lc.setRow(0,4,B00000101);
 //i
 lc.setRow(0,5,B00010000);
 lc.setRow(1,3,B10000010);
 lc.setRow(1,4,B00001010);
 lc.setRow(1,0,B01000011);
 lc.setRow(1,1,B00001000);
 lc.setRow(1,2,B01010100);
 //bip
 tone(speaker, 5500,50);

 delay(200);
 //clear
 lc.clearDisplay(0);
 lc.clearDisplay(1);
 noTone(speaker);
 delay(40);

 //4th Sequence
 lc.setRow(0,0,B00100011);
 lc.setRow(0,1,B00000011);
 lc.setRow(0,2,B01010100);
 lc.setRow(0,3,B01000011);
 lc.setRow(0,4,B00010011);
 lc.setRow(0,5,B00011001);
 lc.setRow(1,3,B10001001);
 lc.setRow(1,4,B01001010);
 lc.setRow(1,0,B00110000);
 lc.setRow(1,1,B01000001);
 lc.setRow(1,2,B01001110);
 tone(speaker, 5500,50);

 delay(200);
 //clear
 lc.clearDisplay(0);
 lc.clearDisplay(1);
 noTone(speaker);
 delay(40);

 //Second CAluri sequence
 //C
 lc.setRow(0,0,B01001110);
 //A
 lc.setChar (0,1,'A',false);
 //l(lowercase "L")
 lc.setRow(0,2,B00110000);
 //u
 lc.setRow(0,3,B00011100);
 //r
 lc.setRow(0,4,B00000101);
 //i
 lc.setRow(0,5,B00010000);
 lc.setRow(1,3,B00101001);
 lc.setRow(1,4,B00001010); 
 lc.setRow(1,0,B00101010);
 lc.setRow(1,1,B00011010);
 lc.setRow(1,2,B00011010);
 //bip
 tone(speaker, 5500,50);

 delay(200);
 //clear
 lc.clearDisplay(0);
 lc.clearDisplay(1);
 noTone(speaker);
 delay(40);

 //6th Sequence
 lc.setRow(0,0,B00101010);
 lc.setRow(0,1,B00000011);
 lc.setRow(0,2,B00011010);
 lc.setRow(0,3,B01001000);
 lc.setRow(0,4,B01000001);
 lc.setRow(0,5,B01001001);
 lc.setRow(1,3,B00101001); 
 lc.setRow(1,4,B00001010);
 lc.setRow(1,0,B00011001);
 lc.setRow(1,1,B00001001);
 lc.setRow(1,2,B00010011);
 tone(speaker, 5500,50);

 delay(200);
 //clear
 lc.clearDisplay(0);
 lc.clearDisplay(1);
 noTone(speaker);
 delay(40);

 //6th Sequence
 lc.setRow(0,0,B00010011);
 lc.setRow(0,1,B00011001);
 lc.setRow(0,2,B00100010);
 lc.setRow(0,3,B00011000);
 lc.setRow(0,4,B01000001);
 lc.setRow(0,5,B00100101);
 lc.setRow(1,3,B10010101);
 lc.setRow(1,4,B00100100);
 lc.setRow(1,0,B00110011);
 lc.setRow(1,1,B00110111);
 lc.setRow(1,2,B10001001);
 tone(speaker, 5500,50);

 delay(200);
 //clear
 lc.clearDisplay(0);
 lc.clearDisplay(1);
 noTone(speaker);
 delay(40);

 //KEnnY
 //K  
 lc.setRow(0,0,B00110111);
 //E  
 lc.setChar(0,1,'E',false);
 //n
 lc.setRow(0,2,0x15);
 //n
 lc.setRow(0,3,0x15);
 //Y
 lc.setRow(0,4,B00110011);
 //-
 lc.setRow(0,5,B00000001);
 //bug
 lc.setRow(1,3,B10001001);
 lc.setRow(1,4,B10001001);
 lc.setRow(1,0,B00100001);
 lc.setRow(1,1,B00010001);
 lc.setRow(1,2,B01000001);
 //bip
 tone(speaker, 5500,50);

 delay(350);
 //clear
 lc.clearDisplay(0);
 lc.clearDisplay(1);
 noTone(speaker);
 delay(75);
} 

void displayFade() {
 delay(100);
 //INSERT FUNCTION FOR DIGIT FADING AFTER ZERO 
 lc.setRow(0,0,B01111000);       //partially zapped 0
 lc.setRow(0,1,B01111110);       //0
 lc.setRow(0,2,B01111110);       //0
 lc.setRow(0,3,B01111110);       //0
 lc.setRow(0,4,B01111110);       //0
 lc.setRow(0,5,B00011110);       //partially zapped 0
 lc.setRow(1,3,B11111111);
 lc.setRow(1,4,B11111111);
 lc.setRow(1,0,B01111111);
 lc.setRow(1,1,B01111111);
 lc.setRow(1,2,B01111111);
 delay(30);
 lc.setRow(0,0,B00110000);       //partially zapped 0
 lc.setRow(0,1,B01111110);       //0
 lc.setRow(0,2,B01111110);       //0
 lc.setRow(0,3,B01111110);       //0
 lc.setRow(0,4,B10000000);       //0
 lc.setRow(0,5,B00000110);       //partially zapped 0
 lc.setRow(1,3,B11101111);
 lc.setRow(1,4,B11101111);
 lc.setRow(1,0,B00111111);
 lc.setRow(1,1,B00111111);
 lc.setRow(1,2,B00111111);
 delay(30);
 lc.setRow(0,0,B00000000);       //fully zapped 0
 lc.setRow(0,1,B01111110);       //0
 lc.setRow(0,2,B01111110);       //0
 lc.setRow(0,3,B01111110);       //0
 lc.setRow(0,4,B01111110);       //0
 lc.setRow(0,5,B00000000);       //fully zapped 0
 lc.setRow(1,3,B11100111);
 lc.setRow(1,4,B11100111);
 lc.setRow(1,0,B00101111);
 lc.setRow(1,1,B00101111);
 lc.setRow(1,2,B00101111);
 delay(30);
 lc.setRow(0,0,B00000000);       //fully zapped 0
 lc.setRow(0,1,B01111000);       //partially zapped 0
 lc.setRow(0,2,B01111110);       //0
 lc.setRow(0,3,B01111110);       //0
 lc.setRow(0,4,B01000000);       //partially zapped 0
 lc.setRow(0,5,B00000000);       //fully zapped 0
 lc.setRow(1,3,B11000111);
 lc.setRow(1,4,B11000111);
 lc.setRow(1,0,B00101011);
 lc.setRow(1,1,B00101011);
 lc.setRow(1,2,B00101011);
 delay(30);
 lc.setRow(0,0,B00000000);       //fully zapped 0
 lc.setRow(0,1,B00110000);       //partially zapped 0
 lc.setRow(0,2,B01111110);       //0
 lc.setRow(0,3,B01111110);       //0
 lc.setRow(0,4,B00000110);       //partially zapped 0
 lc.setRow(0,5,B00000000);       //fully zapped 0
 lc.setRow(1,3,B11000011);
 lc.setRow(1,4,B11000011);
 lc.setRow(1,0,B00001011);
 lc.setRow(1,1,B00001011);
 lc.setRow(1,2,B00001011);
 delay(30);
 lc.setRow(0,0,B00000000);       //fully zapped 0
 lc.setRow(0,1,B00000000);       //fully zapped 0
 lc.setRow(0,2,B01111110);       //0
 lc.setRow(0,3,B01111110);       //0
 lc.setRow(0,4,B00000000);       //fully zapped 0
 lc.setRow(0,5,B00000000);       //fully zapped 0
 lc.setRow(1,3,B10000011);
 lc.setRow(1,4,B10000011);
 lc.setRow(1,0,B00001001);
 lc.setRow(1,1,B00001001);
 lc.setRow(1,2,B00001001);
 delay(55);
 lc.setRow(0,0,B00000000);       //fully zapped 0
 lc.setRow(0,1,B00000000);       //fully zapped 0
 lc.setRow(0,2,B01111000);       //partially zapped 0
 lc.setRow(0,3,B01000000);       //partially zapped 0
 lc.setRow(0,4,B00000000);       //fully zapped 0
 lc.setRow(0,5,B00000000);       //fully zapped 0
 lc.setRow(1,3,B10000001);
 lc.setRow(1,4,B10000001);
 lc.setRow(1,0,B00000001);
 lc.setRow(1,1,B00000001);
 lc.setRow(1,2,B00000001);
 delay(30);
 lc.setRow(0,0,B00000000);       //fully zapped 0
 lc.setRow(0,1,B00000000);       //fully zapped 0
 lc.setRow(0,2,B00110000);       //partially zapped 0
 lc.setRow(0,3,B00000110);       //partially zapped 0
 lc.setRow(0,4,B00000000);       //fully zapped 0
 lc.setRow(0,5,B00000000);       //fully zapped 0
 lc.setRow(1,3,B00000001);
 lc.setRow(1,4,B00000001);
 lc.setRow(1,0,B00000000);
 lc.setRow(1,1,B00000000);
 lc.setRow(1,2,B00000000);
 delay(30);
 lc.setRow(0,0,B00000000);       //fully zapped 0
 lc.setRow(0,1,B00000000);       //fully zapped 0
 lc.setRow(0,2,B00000000);       //fully zapped 0
 lc.setRow(0,3,B00000000);       //fully zapped 0
 lc.setRow(0,4,B00000000);       //fully zapped 0
 lc.setRow(0,5,B00000000);       //fully zapped 0
 lc.setRow(1,3,B00000000);
 lc.setRow(1,4,B00000000);
 lc.setRow(1,0,B00000000);
 lc.setRow(1,1,B00000000);
 lc.setRow(1,2,B00000000);
 delay(1000);
}

void displayWrap() {
 //INSERT DISPLAY WRAP
 delay(30);
 tone(speaker, 5500,100);
 lc.setRow(0,0,B00000010);       
 lc.setRow(0,1,B00000010);       
 lc.setRow(0,2,B00000010);       
 lc.setRow(0,3,B00000010);       
 lc.setRow(0,4,B00000010);       
 lc.setRow(0,5,B00000010); 
 lc.setRow(1,3,B01010011);
 lc.setRow(1,4,B01011010);
 noTone(speaker);
 delay(30);
 tone(speaker, 5500,100);
 lc.setRow(0,0,B01000000);       
 lc.setRow(0,1,B01000000);       
 lc.setRow(0,2,B01000000);       
 lc.setRow(0,3,B01000000);       
 lc.setRow(0,4,B01000000);       
 lc.setRow(0,5,B01000000); 
 lc.setRow(1,3,B01101001);
 lc.setRow(1,4,B01000000);
 lc.setRow(1,0,B00000010);
 lc.setRow(1,1,B00000010);
 lc.setRow(1,2,B00000010);
 noTone(speaker);
 delay(30);
 tone(speaker, 5500,100);
 lc.setRow(0,0,B00100000);       
 lc.setRow(0,1,B00100000);       
 lc.setRow(0,2,B00100000);       
 lc.setRow(0,3,B00100000);       
 lc.setRow(0,4,B00100000);       
 lc.setRow(0,5,B00100000); 
 lc.setRow(1,3,B00100101);
 lc.setRow(1,4,B01001000);
 lc.setRow(1,0,B00000100);
 lc.setRow(1,1,B00000100);
 lc.setRow(1,2,B00000100);
 noTone(speaker);
 delay(30);
 tone(speaker, 5500,100);
 lc.setRow(0,0,B00010000);       
 lc.setRow(0,1,B00010000);       
 lc.setRow(0,2,B00010000);       
 lc.setRow(0,3,B00010000);       
 lc.setRow(0,4,B00010000);       
 lc.setRow(0,5,B00010000); 
 lc.setRow(1,3,B10100101);
 lc.setRow(1,4,B11011000);
 lc.setRow(1,0,B00001000);
 lc.setRow(1,1,B00001000);
 lc.setRow(1,2,B00001000);
 noTone(speaker);
 delay(30);
 tone(speaker, 5500,100);
 lc.setRow(0,0,B00001000);       
 lc.setRow(0,1,B00001000);       
 lc.setRow(0,2,B00001000);       
 lc.setRow(0,3,B00001000);       
 lc.setRow(0,4,B00001000);       
 lc.setRow(0,5,B00001000); 
 lc.setRow(1,3,B10100101);
 lc.setRow(1,4,B11011010);
 lc.setRow(1,0,B00010000);
 lc.setRow(1,1,B00010000);
 lc.setRow(1,2,B00010000);
 noTone(speaker);
 delay(30);
 tone(speaker, 5500,100);
 lc.setRow(0,0,B00000100);       
 lc.setRow(0,1,B00000100);       
 lc.setRow(0,2,B00000100);       
 lc.setRow(0,3,B00000100);       
 lc.setRow(0,4,B00000100);       
 lc.setRow(0,5,B00000100);
 lc.setRow(1,3,B10001101);
 lc.setRow(1,4,B10101010);
 lc.setRow(1,0,B00100000);
 lc.setRow(1,1,B00100000);
 lc.setRow(1,2,B00100000);
 noTone(speaker);
 delay(30);
 tone(speaker, 5500,100);
 lc.setRow(0,0,B00000010);       
 lc.setRow(0,1,B00000010);       
 lc.setRow(0,2,B00000010);       
 lc.setRow(0,3,B00000010);       
 lc.setRow(0,4,B00000010);       
 lc.setRow(0,5,B00000010);
 lc.setRow(1,3,B01010111);
 lc.setRow(1,4,B00110110);
 lc.setRow(1,0,B01000000);
 lc.setRow(1,1,B01000000);
 lc.setRow(1,2,B01000000);
 noTone(speaker);
 delay(30);
 tone(speaker, 5500,100);
 lc.setRow(0,0,B01000000);       
 lc.setRow(0,1,B01000000);       
 lc.setRow(0,2,B01000000);       
 lc.setRow(0,3,B01000000);       
 lc.setRow(0,4,B01000000);       
 lc.setRow(0,5,B01000000);
 lc.setRow(1,3,B01010100);
 lc.setRow(1,4,B10110110); 
 noTone(speaker);
 delay(30);
 tone(speaker, 5500,100);
 lc.setRow(0,0,B00100000);       
 lc.setRow(0,1,B00100000);       
 lc.setRow(0,2,B00100000);       
 lc.setRow(0,3,B00100000);       
 lc.setRow(0,4,B00100000);       
 lc.setRow(0,5,B00100000);
 lc.setRow(1,3,B01000100);
 lc.setRow(1,4,B10000010);
 lc.setRow(1,0,B00000010);
 lc.setRow(1,1,B00000010);
 lc.setRow(1,2,B00000010);
 noTone(speaker);
 delay(30);
 tone(speaker, 5500,100);
 lc.setRow(0,0,B00010000);       
 lc.setRow(0,1,B00010000);       
 lc.setRow(0,2,B00010000);       
 lc.setRow(0,3,B00010000);       
 lc.setRow(0,4,B00010000);       
 lc.setRow(0,5,B00010000);
 lc.setRow(1,3,B01010101);
 lc.setRow(1,4,B10000100);
 lc.setRow(1,0,B00000100);
 lc.setRow(1,1,B00000100);
 lc.setRow(1,2,B00000100);
 noTone(speaker);
 delay(30);
 tone(speaker, 2500,100);
 lc.setRow(0,0,B00001000);       
 lc.setRow(0,1,B00001000);       
 lc.setRow(0,2,B00001000);       
 lc.setRow(0,3,B00001000);       
 lc.setRow(0,4,B00001000);       
 lc.setRow(0,5,B00001000);
 lc.setRow(1,3,B00100101);
 lc.setRow(1,4,B10001101); 
 lc.setRow(1,0,B00001000);
 lc.setRow(1,1,B00001000);
 lc.setRow(1,2,B00001000);
 noTone(speaker);
 delay(30);
 tone(speaker, 2500,100);
 lc.setRow(0,0,B00000100);       
 lc.setRow(0,1,B00000100);       
 lc.setRow(0,2,B00000100);       
 lc.setRow(0,3,B00000100);       
 lc.setRow(0,4,B00000100);       
 lc.setRow(0,5,B00000100);
 lc.setRow(1,3,B01010111);
 lc.setRow(1,4,B10110110);
 lc.setRow(1,0,B00010000);
 lc.setRow(1,1,B00010000);
 lc.setRow(1,2,B00010000); 
 noTone(speaker);
 delay(30);
 tone(speaker, 2500,100);
 lc.setRow(0,0,B00000010);       
 lc.setRow(0,1,B00000010);       
 lc.setRow(0,2,B00000010);       
 lc.setRow(0,3,B00000010);       
 lc.setRow(0,4,B00000010);       
 lc.setRow(0,5,B00000010);
 lc.setRow(1,3,B01000100);
 lc.setRow(1,4,B01001000);
 lc.setRow(1,0,B00100000);
 lc.setRow(1,1,B00100000);
 lc.setRow(1,2,B00100000); 
 noTone(speaker);
 delay(30);
 tone(speaker, 2500,100);
 lc.setRow(1,3,B01010101);
 lc.setRow(1,4,B10000100);
 lc.setRow(1,0,B01000000);
 lc.setRow(1,1,B01000000);
 lc.setRow(1,2,B01000000);
 noTone(speaker);
 delay(30);
 affiche_temps(); 
}

//Boucle fin attente
void wrapFinhhmmss() {
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B00000000);
lc.setRow(0,2,B00000000);
lc.setRow(0,3,B00000000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
lc.setRow(1,0,B01101101);
lc.setRow(1,1,B11111011);
lc.setRow(1,2,B00111011);
lc.setRow(1,6,B01111000);
lc.setRow(1,3,B00000000);
lc.setRow(1,4,B00000000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B00000000);
lc.setRow(0,3,B00000000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B01000000);
lc.setRow(0,3,B00000000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B01000000);
lc.setRow(0,3,B01000000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B01000000);
lc.setRow(0,3,B01000000);
lc.setRow(0,4,B01000000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B01000000);
lc.setRow(0,3,B01000000);
lc.setRow(0,4,B01000000);
lc.setRow(0,5,B01000000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B01000000);
lc.setRow(0,3,B01000000);
lc.setRow(0,4,B01000000);
lc.setRow(0,5,B01100000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B01000000);
lc.setRow(0,3,B01000000);
lc.setRow(0,4,B01000000);
lc.setRow(0,5,B01110000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B01000000);
lc.setRow(0,3,B01000000);
lc.setRow(0,4,B01000000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B01000000);
lc.setRow(0,3,B01000000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B01000000);
lc.setRow(0,3,B01001000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01000000);
lc.setRow(0,2,B01001000);
lc.setRow(0,3,B01001000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B01000000); 
lc.setRow(0,1,B01001000);
lc.setRow(0,2,B01001000);
lc.setRow(0,3,B01001000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B01001000); 
lc.setRow(0,1,B01001000);
lc.setRow(0,2,B01001000);
lc.setRow(0,3,B01001000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B01001100); 
lc.setRow(0,1,B01001000);
lc.setRow(0,2,B01001000);
lc.setRow(0,3,B01001000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B01001110); 
lc.setRow(0,1,B01001000);
lc.setRow(0,2,B01001000);
lc.setRow(0,3,B01001000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B01001000);
lc.setRow(0,2,B01001000);
lc.setRow(0,3,B01001000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B01001000);
lc.setRow(0,3,B01001000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B00001000);
lc.setRow(0,3,B01001000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B00001000);
lc.setRow(0,3,B00001000);
lc.setRow(0,4,B01001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B00001000);
lc.setRow(0,3,B00001000);
lc.setRow(0,4,B00001000);
lc.setRow(0,5,B01111000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B00001000);
lc.setRow(0,3,B00001000);
lc.setRow(0,4,B00001000);
lc.setRow(0,5,B00111000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B00001000);
lc.setRow(0,3,B00001000);
lc.setRow(0,4,B00001000);
lc.setRow(0,5,B00011000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B00001000);
lc.setRow(0,3,B00001000);
lc.setRow(0,4,B00001000);
lc.setRow(0,5,B00001000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B00001000);
lc.setRow(0,3,B00001000);
lc.setRow(0,4,B00001000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B00001000);
lc.setRow(0,3,B00001000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B00001000);
lc.setRow(0,3,B00000000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00001000);
lc.setRow(0,2,B00000000);
lc.setRow(0,3,B00000000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B00001110); 
lc.setRow(0,1,B00000000);
lc.setRow(0,2,B00000000);
lc.setRow(0,3,B00000000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B00000110); 
lc.setRow(0,1,B00000000);
lc.setRow(0,2,B00000000);
lc.setRow(0,3,B00000000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B00000010); 
lc.setRow(0,1,B00000000);
lc.setRow(0,2,B00000000);
lc.setRow(0,3,B00000000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
delay(50);
lc.setRow(0,0,B00000000); 
lc.setRow(0,1,B00000000);
lc.setRow(0,2,B00000000);
lc.setRow(0,3,B00000000);
lc.setRow(0,4,B00000000);
lc.setRow(0,5,B00000000);
}

// fonction qui allume et eteind les leds unes apres les autres de la même couleur
// arg 1 : couleur , arg 2 : Vitesse , arg 3 : une seule led allumé

void colorWipe(uint32_t color, int speed=100,bool single=false) {
  for(int16_t i=0; i<strip.numPixels(); i++) {
    if (single) strip.clear();
    strip.setPixelColor(i, color);
    strip.show();
    delay(speed);
  } 
}

void colorWipe1(uint32_t color, int speed=100,bool single=false) {
  for(int16_t i=(strip.numPixels()-1); i>=0; i--) {
   if (single) strip.clear();
   strip.setPixelColor(i, color);
   strip.show();
   delay(speed); 
  }
}

//programme principal
void loop() {

  //controle bouton 1
  button1.tick();
  button2.tick();

  //anti rebond
  debounceSTART(); 
  debounceVORTEX();
  
  //attente
  delay(10);

 //lancer programme minuteur 
 strip.show();
 etatBlink();
 normal();

 if (digitalRead(BP_VORTEX)==LOW){
  BP_VORTEX_STATUS = true;
    }
 if ((digitalRead(BP_UP)==LOW)&&(digitalRead(BP_DOWN)==LOW)) {
  mode_test();
 }
 if (digitalRead(BP_START)==true){
  BP_START_STATUS = true;  
 } 
}

//fonction reboot/reset
void(* reboot) (void) = 0;

//definir fonctions boutons
void click1(){veille();}//veille
void click2(){menu_up(); buzzer_menu();}//selection digit reglage

void longPress1(){reveille();}//sortie veille
void longPress2(){reboot();}//reset

void doubleclick1(){zap();}
void doubleclick2(){BP_POWER_STATUS = true;}

//baregraphe post fin
const uint64_t IMAGES[] = {
  0x0000000000000000,
  0x0000000180000000,
  0x00000003c0000000,
  0x00000007e0000000,
  0x0000000ff0000000,
  0x0000001ff8000000,
  0x0000003ffc000000,
  0x0000007ffe000000,
  0x000000ffff000000,
  0x0000007ffe000000,
  0x0000003ffc000000,
  0x0000001ff8000000,
  0x0000000ff0000000,
  0x00000007e0000000,
  0x00000003c0000000,
  0x0000000180000000,
  0x0000000000000000
};
const int IMAGES_LEN = sizeof(IMAGES)/8;

void displayImage(uint64_t image) {
  for (int o = 3; o < 5; o++) {
    byte row = (image >> o * 8) & 0xFF;
    for (int p = 0; p < 8; p++) {
      lc.setLed(1, o, p, bitRead(row, p));
    }
  }
}
int o = 0;

void baregraphepostfin() {
  displayImage(IMAGES[o]);
  if (++o >= IMAGES_LEN ) {
    o = 0;
  }
  delay(15);
}

  //baregraphe normal
  const uint64_t IMAGE[] = {
  0x0000000000000000,
  0x0000000180000000,
  0x00000003c0000000,
  0x00000007e0000000,
  0x0000000ff0000000,
  0x0000001ff8000000,
  0x0000000ff0000000,
  0x00000007e0000000,
  0x0000000ff0000000,
  0x0000001ff8000000,
  0x0000003ffc000000,
  0x0000007ffe000000,
  0x000000ffff000000,
  0x0000007ffe000000,
  0x0000003ffc000000,
  0x0000001ff8000000,
  0x0000000ff0000000,
  0x00000007e0000000,
  0x0000000ff0000000,
  0x0000001ff8000000,
  0x0000000ff0000000,
  0x00000007e0000000,
  0x00000003c0000000,
  0x0000000180000000,
  0x0000000000000000
};
const int IMAGES_LE = sizeof(IMAGE)/8;

void displayImage1(uint64_t image1) {
  for (int a = 3; a < 5; a++) {
    byte row2 = (image1 >> a * 8) & 0xFF;
    for (int b = 0; b < 8; b++) {
      lc.setLed(1, a, b, bitRead(row2, b));
    }
  }
}
int a = 0;

void baregraphe() {
  displayImage1(IMAGE[a]);
  if (++a >= IMAGES_LE ) {
    a = 0;
  }
  delay(20);
}

  //baregraphe fin
  const uint64_t IMAGE1[] = {
  0x0000000000000000,
  0x0000000101000000,
  0x0000008181000000,
  0x0000008383000000,
  0x000000c3c3000000,
  0x000000c7c7000000,
  0x000000e7e7000000,
  0x000000efef000000,
  0x000000ffff000000,
  0x000000efef000000,
  0x000000e7e7000000,
  0x000000c7c7000000,
  0x000000c3c3000000,
  0x0000008383000000,
  0x0000008181000000,
  0x0000000101000000,
  0x0000000000000000,
  0x0000000101000000,
  0x0000008181000000,
  0x0000008383000000,
  0x000000c3c3000000,
  0x000000c7c7000000,
  0x000000e7e7000000,
  0x000000efef000000,
  0x000000ffff000000,
  0x000000efef000000,
  0x000000e7e7000000,
  0x000000c7c7000000,
  0x000000c3c3000000,
  0x0000008383000000,
  0x0000008181000000,
  0x0000000101000000,
  0x0000000000000000,
  0x0000000000000000,
  0x0000000101000000,
  0x0000008181000000,
  0x0000008383000000,
  0x000000c3c3000000,
  0x000000c7c7000000,
  0x000000e7e7000000,
  0x000000f3cf000000,
  0x000000f99f000000,
  0x000000fc3f000000,
  0x000000fe7f000000,
  0x000000fc3f000000,
  0x000000f99f000000,
  0x000000f3cf000000,
  0x000000e7e7000000,
  0x000000cff3000000,
  0x0000009ff9000000,
  0x0000003ffc000000,
  0x0000007ffe000000,
  0x0000009ff9000000,
  0x000000cff3000000,
  0x000000e7e7000000,
  0x000000f3cf000000,
  0x000000f99f000000,
  0x000000fc3f000000,
  0x000000fe7f000000,
  0x000000fc3f000000,
  0x000000f99f000000,
  0x000000f3cf000000,
  0x000000e7e7000000,
  0x000000cff3000000,
  0x0000009ff9000000,
  0x0000003ffc000000,
  0x0000007ffe000000,
  0x0000009ff9000000,
  0x000000cff3000000,
  0x000000e7e7000000,
  0x000000f3cf000000,
  0x000000f99f000000,
  0x000000fc3f000000,
  0x000000fe7f000000,
  0x000000fc3f000000,
  0x000000f99f000000,
  0x000000f3cf000000,
  0x000000e7e7000000,
  0x000000cff3000000,
  0x0000009ff9000000,
  0x0000003ffc000000,
  0x0000007ffe000000,
  0x0000009ff9000000,
  0x000000cff3000000,
  0x000000e7e7000000
    
  
};
const int IMAGES_LE1 = sizeof(IMAGE1)/8;

void displayImage2(uint64_t image2) {
  for (int c = 3; c < 5; c++) {
    byte row3 = (image2 >> c * 8) & 0xFF;
    for (int d = 0; d < 8; d++) {
      lc.setLed(1, c, d, bitRead(row3, d));
    }
  }
}
int c = 0;

void baregraphefin() {
  displayImage2(IMAGE1[c]);
  if (++c >= IMAGES_LE1 ) {
    c = 0;
  }
  delay(15);
}

  //off baregraphe
  const uint64_t IMAGE2[] = {
  0x0000000000000000
};
const int IMAGES_LE2 = sizeof(IMAGE2)/8;

void displayImage3(uint64_t image3) {
  for (int e = 3; e < 5; e++) {
    byte row4 = (image3 >> e * 8) & 0xFF;
    for (int f = 0; f < 8; f++) {
      lc.setLed(1, e, f, bitRead(row4, f));
    }
  }
}
int e = 0;

void offbaregraphe() {
  displayImage3(IMAGE2[e]);
  if (++e >= IMAGES_LE2 ) {
    e = 0;
  } 
}

//reglage de conversion
#define secondsinaday 86400  //((60*60)*24)
#define secondsinhour 3600  //(60*60)
#define secondsinminute 60

//Fonction d'affichage du temps restant normal
void affiche_temps() {

   unsigned long sectime = totalsectime; 
   unsigned long days = sectime/secondsinaday; 
   sectime = sectime % secondsinaday; 
   unsigned long hours = sectime/secondsinhour; 
   sectime = sectime % secondsinhour; 
   unsigned long minutes = sectime/secondsinminute;
   unsigned long seconds = sectime % secondsinminute; 

   unsigned long days_hundreds = days/100; 
   unsigned long days_tens = (days %100)/10;
   unsigned long days_units = (days %100)%10;

   unsigned long hours_tens = (hours %100)/10;
   unsigned long hours_units = (hours %100)%10;

   unsigned long minutes_tens = (minutes %100)/10;
   unsigned long minutes_units = (minutes %100)%10;

   unsigned long seconds_tens = (seconds %100)/10;
   unsigned long seconds_units = (seconds %100)%10;
  
//bloc de conversion vers 7 segments
   
   //bloc DDD
   lc.setDigit(1,0,days_hundreds,false);
   lc.setDigit(1,1,days_tens,false);
   lc.setDigit(1,2,days_units,false);
   
   //bloc HH
   lc.setDigit(0,0,hours_tens,false);
   lc.setDigit(0,1,hours_units,false);
   
   //bloc MM
   lc.setDigit(0,2,minutes_tens,false);
   lc.setDigit(0,3,minutes_units,false);
   
   //bloc SS
   lc.setDigit(0,4,seconds_tens,false);
   lc.setDigit(0,5,seconds_units,false);  
}

//gestion reglage multi vitesse
void delay_increment(byte rst) {
  static unsigned long last_time3 = 0; // Temps antérieur
  unsigned long time_now3 = millis();  // Temps actuel
 
  // Si le paramètre de reset est activé
  if(rst) {
    last_time3 = time_now3; // Remise à zéro
    return; }       // Fin de fonction
   
  // Si le temps d'appui est supérieur ou égale à 10 secondes
  if(time_now3 - last_time3 >= 1000) {
    delay(100);

  }// Sinon pour tout appui de moins de 10 secondes
  else {  
  delay(150);
  }
}
 
//interruption fonctionnement
void bp_start_int() {
  running = !running;
}

//fonction du timer normal
void normal(){
  
  //mode 0=normal 1=burnout 2=foutu
  byte vortex=0;

//mise en route minuteur par double click BP_POWER
if (BP_POWER_STATUS == true) {
   
   //sequence demarrage normal 1 fois
   while(i == 0) {
    strip.show();
    BP_VORTEX_STATUS = false;
    musique();
    genserOne();
    displayWrap();
    lc.clearDisplay(0);
    lc.clearDisplay(1);
    affiche_temps();
    lc.setLed(0,6,1,true);
    lc.setLed(0,6,2,true);
    lc.setLed(0,6,3,true);
    lc.setLed(0,6,4,true); 
    i++;
    byte vortex = 0;
    break; 
  } 
  
  // demarrage comptage si different de zero et running
  if((running)&&(totalsectime_slide!=0)) {
  
  //comptage normal bloquer sans appuis sur vortex
  while((vortex==0)&&(BP_START_STATUS==true)&&(totalsectime_slide!=0)) {
  animation_normal();
  button1.tick();
  button2.tick();
  
  //Variables locales de gestion du temps 
  static unsigned long last_time = 0; // Temps antérieur
  unsigned long time_now = millis();  // Temps actuel
  
  //sauvegarde du temps a l'ecran pour affichage temps restant
  unsigned long totalsectime_save;

  //conversion pour temps restant : temps en memoire - temps ecran = temps restant
  unsigned long totalsectime_reste = totalsectime_slide - totalsectime;

  if(time_now - last_time >= 1000) {
    totalsectime++;
    last_time = time_now;
    affiche_temps();  
    }  

         //fin mode normal
         if(totalsectime_reste <= 0){                   
          totalsectime = 0; 
          totalsectime_slide = 0;
          totalsectime_reste = 0;
          colorWipe(Red);
          colorWipe1(Off);        
          displayFade();
          displayWrap();
          displayWrap();
          offbaregraphe();
          lc.setLed(1,5,5,false);
          lc.setLed(1,5,3,false);
          lc.setLed(1,5,4,false);
          lc.setLed(1,7,1,false);
          lc.setLed(1,7,2,false);
          delay(200);
          affiche_temps();  
          running = false;   
          BP_START_STATUS = false;  
          }       

          //activation vortex trop tot
          if(digitalRead(BP_VORTEX)==LOW) {
                     
            vortex=1;
            
            lc.setLed(1,7,0,true); //on led rouge emmetteurs
            lc.setLed(1,7,3,true); //on led rouge emmetteurs
            
            tone(speaker, 3000, 1000);
            delay(1500);
            noTone(speaker);
            
            lc.setLed(1,7,0,false); //off led rouge emmetteurs
            lc.setLed(1,7,3,false); //off led rouge emmetteurs 
            lc.setLed(1,7,1,false); //off led blanche emmetteurs 
            lc.setLed(1,7,2,false); //off led blanche emmetteurs         
                  
          totalsectime = 90; 
          totalsectime_slide = 0;
          totalsectime_reste = 0;

            displayFade();
            genserOne();
            displayWrap(); 
          }
          //affichage temps restant
          if(digitalRead(BP_UP)==LOW) {

            noTone(speaker);
            running = false;                  //arret minuteur
            totalsectime_save = totalsectime; //sauvegarde du chrono
            lc.setRow(0,0,B00000101);         //r
            lc.setRow(0,1,B01101111);         //e
            lc.setRow(0,2,B00010101);         //m
            lc.setRow(0,3,B01110111);         //a
            lc.setRow(0,4,B00010000);         //i
            lc.setRow(0,5,B00010101);         //n
            delay(1000);
            lc.setRow(0,0,B00001111);         //t
            lc.setRow(0,1,B00010000);         //i
            lc.setRow(0,2,B00010101);         //m
            lc.setRow(0,3,B01101111);         //e
            lc.setRow(0,4,B00000000);
            lc.setRow(0,5,B00000000);
            delay(1000);

            //affichage du temps restant
            totalsectime = totalsectime_reste;
            affiche_temps();
            delay(2000);

            //restoration du temps de chono et affichage
            totalsectime = totalsectime_save;
            affiche_temps(); 
            running = true;
          }
           //affichage batterie
           if ((digitalRead(BP_DOWN)==LOW)&&(running==true)) {
           batterie();  
          }
        }  
      }

if(vortex==1) {
      
    //comptage burnout
    while(vortex==1) { 
    
    affiche_temps();
    
    animation_burnout(); //animation minuteur led/baregraphe/colons/buzzer...
    button1.tick();
    button2.tick();

    lc.setLed(1,7,1,false);
    lc.setLed(1,7,2,false);
    lc.setLed(1,7,3,false);
    lc.setLed(1,7,0,false); 
    
  //Variables locales de gestion du temps 
  static unsigned long last_time1 =0; // Temps antérieur
  unsigned long time_now1 = millis();  // Temps actuel
 
    // Et que 1 sec s'est écoulée
    if(time_now1 - last_time1 >= 1000) {
      totalsectime--;
      last_time1 = time_now1;
      affiche_temps();      
    } 

//fin mode burnout slide
if((totalsectime>=0 && totalsectime<=1)&&(digitalRead(BP_VORTEX)==LOW))
{
          lc.setLed(1,7,1,true);
          lc.setLed(1,7,2,true);
          lc.setLed(1,7,3,true);
          lc.setLed(1,7,0,true); 
          tone(speaker, 3000, 1000);                  
          colorWipe(Red);
          colorWipe1(Off);                   
          delay(1500);
          vortex=1;    
          displayFade();
          genserOne();
          displayWrap(); 
          totalsectime = random(16756131);   //random                         
      } 

//fin mode burnout glisse rater
if(totalsectime<=0)
{
  affiche_temps();  
  vortex=2; 
  colorWipe1(Off);
  displayFade();
  totalsectime = 0;
  musique(); 
    } 
     //affichage batterie
     if ((digitalRead(BP_DOWN)==LOW)&&(running==true)) {
     batterie();  
    }       
  }
}  

//mode bloquer 29ans avec reset sur bp2
if(vortex==2) {
     
    //mode 29ans
    while(vortex==2) { 
       button2.tick(); //reset
       noTone(speaker);
       wrapFinhhmmss(); 
       animation_29();
       vortex=2;

     //affichage batterie
     if (digitalRead(BP_DOWN)==LOW) {
     batterie();  
    }
  }
}
       
  else { // Si le minuteur n'est pas en marche reglage possible

    adafruit(); //ring
    lc.setRow(1,6,B01111000);//extremitées baregraphe

//reglage secondes
while((!running)&&(menu == 1)) {
    
    button1.tick();
    button2.tick();
    ecranBlink();
    bloc = 0;
    ecran = 5;
    
    // Si le bouton + est appuyé
    if(digitalRead(BP_UP) == LOW) {
    tone(speaker, 3000, 50);  
 
    totalsectime++;

      // Attente variable (basé sur le temps d'appui)
      delay_increment(false);
 
      // Affichage du temps restant
      affiche_temps();

    }// Si aucun bouton n'est appuyé
    else {

      // Remise à zéro du delai variable
      delay_increment(true);
    }

    // Si le bouton - est appuyé
    if(digitalRead(BP_DOWN) == LOW) {
    tone(speaker, 2900, 50);
 
      totalsectime--;
           
      // Attente variable (basé sur le temps d'appui)
      delay_increment(false);
 
      // Affichage du temps restant
      affiche_temps();

    }// Si aucun bouton n'est appuyé
    else {

      // Remise à zéro du delai variable
      delay_increment(true);
    }}

//reglage minutes
while((!running)&&(menu == 2)) {

    button1.tick();
    button2.tick();
    ecranBlink();
    bloc = 0;
    ecran = 3;
    
    // Si le bouton + est appuyé
    if(digitalRead(BP_UP) == LOW) {
    tone(speaker, 3000, 50);
     
      totalsectime = totalsectime+60;            

      // Attente variable (basé sur le temps d'appui)
      delay_increment(false);
 
      // Affichage du temps restant
      affiche_temps();

    }// Si aucun bouton n'est appuyé
    else {

      // Remise à zéro du delai variable
      delay_increment(true);
    }

    // Si le bouton - est appuyé
    if(digitalRead(BP_DOWN) == LOW) {
    tone(speaker, 2900, 50);
     
      totalsectime = totalsectime-60; 
     
      // Attente variable (basé sur le temps d'appui)
      delay_increment(false);
 
      // Affichage du temps restant
      affiche_temps();

    }// Si aucun bouton n'est appuyé
    else {

      // Remise à zéro du delai variable
      delay_increment(true);
    }}
    
//reglage heures
while((!running)&&(menu == 3)) {

    button1.tick();
    button2.tick();
    ecranBlink();
    bloc = 0;
    ecran = 1;
    // Si le bouton + est appuyé
    if(digitalRead(BP_UP) == LOW) {
    tone(speaker, 3000, 50);
    
    totalsectime = totalsectime+3600;
    
      // Attente variable (basé sur le temps d'appui)
      delay_increment(false);
 
      // Affichage du temps restant
      affiche_temps();

    }// Si aucun bouton n'est appuyé
    else {

      // Remise à zéro du delai variable
      delay_increment(true);
    }

    // Si le bouton - est appuyé
    if(digitalRead(BP_DOWN) == LOW) {
    tone(speaker, 2900, 50);
     
    totalsectime = totalsectime-3600;
     
      // Attente variable (basé sur le temps d'appui)
      delay_increment(false);
 
      // Affichage du temps restant
      affiche_temps();

    }// Si aucun bouton n'est appuyé
    else {

      // Remise à zéro du delai variable
      delay_increment(true);
    }}

//reglage jours
while((!running)&&(menu == 4)) {

    button1.tick();
    button2.tick();
    ecranBlink();
    bloc = 1;
    ecran = 2;
    // Si le bouton + est appuyé
    if(digitalRead(BP_UP) == LOW) {
    tone(speaker, 3000, 50);
    
    totalsectime = totalsectime+86400;

      // Attente variable (basé sur le temps d'appui)
      delay_increment(false);
 
      // Affichage du temps restant
      affiche_temps();

    }// Si aucun bouton n'est appuyé
    else {

      // Remise à zéro du delai variable
      delay_increment(true);
    }

    // Si le bouton - est appuyé
    if(digitalRead(BP_DOWN) == LOW) {
    tone(speaker, 2900, 50);
    
    totalsectime = totalsectime-86400;
     
      // Attente variable (basé sur le temps d'appui)
      delay_increment(false);
 
      // Affichage du temps restant
      affiche_temps();

    }// Si aucun bouton n'est appuyé
    else {

      // Remise à zéro du delai variable
      delay_increment(true);

    }}  
    
    //enregistrement reglage
    while((!running)&&(menu == 5)){

    button1.tick();
    button2.tick();    
    ecranBlink1();
    totalsectime_slide = totalsectime;
    }

    //mise a zero ecrans pret a demarrer
    while((!running)&&(menu == 6)){

    totalsectime = 0;
    delay(150);
    affiche_temps();
    menu = 0;  

if(totalsectime_slide != 0) {
     lc.setRow(0,0,B01011011);//S
     lc.setRow(0,1,B00001110);//L
     lc.setRow(0,2,B00110000);//I
     lc.setRow(0,3,B01111110);//D
     lc.setRow(0,4,B01001111);//E
     lc.setRow(0,5,B10110000);//!
           }
        }
     }
   }
}

//gestion animation burnout
void animation_burnout() {
adafruit(); //ring
lc.setRow(1,6,B01111000); //led extremiter chenillard
  
  //baregraphe fin  
  if (totalsectime <=30)
  {
  baregraphefin();
  }
  //baregraphe post fin
  else if ((totalsectime >= 30 && totalsectime <= 59))
  {
  baregraphepostfin();
  }
  //baregraphe normal
  else
  {
  baregraphe();
  }

    //led verte
    if((totalsectime >= 35 && totalsectime <= 59)) {  
    lc.setLed(1,5,5,true);
   }
      else{
     greenpinBlink();
   }

   //led jaune
   if((totalsectime >= 15 && totalsectime <= 35)) {
    lc.setLed(1,5,3,true);
   }
      else{
     yellowpinBlink();
   }

   //led rouge
   if((totalsectime >= 0 && totalsectime <= 15)) {   
     lc.setLed(1,5,4,true);
   }   
      else{
     redpinBlink();
   }
   if(totalsectime == 10) {
     while(k1 == 0){
     displayWrap();
     k1++;
     }
   }
    
   //Buzzer 20s
   if((totalsectime >= 10 && totalsectime <= 20)) {
   updatespeaker1();
   }
   //Buzzer 10s
   else if((totalsectime >= 1 && totalsectime <= 10)) {       
   updatespeaker2();
   } 
   //Buzzer 0s
   else if((totalsectime == 0 && totalsectime <= 1)) {       
   tone(speaker, 3000, 200);
   } 
   //Buzzer normal
   else {
   updatespeaker();
   }   

//animation colons
   if((totalsectime <= 5)) {
    lc.setLed(0,6,1,true);
    lc.setLed(0,6,2,true);
    lc.setLed(0,6,3,true);
    lc.setLed(0,6,4,true);      
    }  
    
    else {
    colonBlink();      
  }               
}

//animation mode normal
void animation_normal() {

//constante pour comptage
unsigned long totalsectime_reste = totalsectime_slide - totalsectime;

baregraphepostfin(); //baregraphe
greenpinBlink(); //led vert
yellowpinBlink(); //led jaune
redpinBlink(); //led rouge
colonBlink(); //led colons clignotente
adafruit(); //ring
lc.setRow(1,6,B01111000); //led extremiter chenillard

   //Buzzer 20s
   if((totalsectime_reste >= 10) && (totalsectime_reste <= 20)){   
   updatespeaker1();
   }
   //Buzzer 10s
   else if((totalsectime_reste >= 1) && (totalsectime_reste <= 10)) {       
   updatespeaker2();
   } 
   //Buzzer 0s
   else if(totalsectime_reste <= 1) {       
   tone(speaker, 3000, 1500);
   } 
   //Buzzer normal
   else {
   updatespeaker();
   } 

   //led blanche
   if(totalsectime_reste == 6) {
     lc.setLed(1,7,1,true);
     lc.setLed(1,7,2,false);
   }
   if(totalsectime_reste == 5) {
     lc.setLed(1,7,1,false);
     lc.setLed(1,7,2,true);
   }
   if(totalsectime_reste == 4) {
     lc.setLed(1,7,1,true);
     lc.setLed(1,7,2,false);
   }
   if(totalsectime_reste == 3) {
     lc.setLed(1,7,1,false);
     lc.setLed(1,7,2,true);
   }
   if(totalsectime_reste <= 2) {
     lc.setLed(1,7,1,true);
     lc.setLed(1,7,2,true);
   }
   if(totalsectime_reste > 6) {
     lc.setLed(1,7,1,false);
     lc.setLed(1,7,2,false);
   }    
     //allumage led au demarrage
     if(totalsectime <= 0) {
     lc.setLed(1,7,1,true);
     lc.setLed(1,7,2,true);
     tone(speaker, 3000, 1500); 
     delay(1500);   
     }
}

//animation blocage
void animation_29() {  
     
lc.setLed(1,7,1,false);
lc.setLed(1,7,2,false);
lc.setLed(1,7,3,false);
lc.setLed(1,7,0,false); 
lc.setLed(1,5,4,false);
lc.setLed(1,5,3,false);
lc.setLed(1,5,5,false);             
lc.setLed(0,6,1,false);
lc.setLed(0,6,2,false);
lc.setLed(0,6,3,false);
lc.setLed(0,6,4,false); 
offbaregraphe();
}

//controle adafruit
void adafruit() {

reading = analogRead(pot1);
val = (reading/1024.0) * 8;

    // Neopixel LED number code
    strip.setBrightness(10);
    if (val != prevVal)
    {
      for ( x = 0; x < val; x++) 
      {
        strip.setPixelColor(x,255,0,0);
      }
      for (x=val; x<13; x++) 
      { 
        strip.setPixelColor(x,0,0,0);
        strip.show();
      }
      prevVal = val;
    }
    else
    {
      strip.show();
    }    
  }

void offadafruit() { //off ring adafruit
  strip.fill(strip.Color(0,0,0),0,7);
}

void onadafruit() { //on ring adafruit
  strip.fill(strip.Color(255,0,0),0,7);
}

void veille() {
     
     lc.shutdown(1 ,HIGH);
     lc.shutdown(0 ,HIGH); 
     offadafruit(); 
}

void reveille() {

    lc.shutdown(1, LOW); 
    lc.shutdown(0 ,LOW); 
    onadafruit(); 
}

//animation DP HHMMSS non utiliser
void animation_dp() {
 if((totalsectime >= 15) && (running == true)) {  
    lc.setLed(0,0,0,false);
    lc.setLed(0,1,0,false);
    lc.setLed(0,2,0,false);
    lc.setLed(0,3,0,false);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
   }    
     
 if((totalsectime == 14) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,false);
    lc.setLed(0,2,0,false);
    lc.setLed(0,3,0,false);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
   }    

 if((totalsectime == 13) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,false);
    lc.setLed(0,3,0,false);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
   }         

 if((totalsectime == 12) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,true);
    lc.setLed(0,3,0,false);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
    }

 if((totalsectime == 11) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,true);
    lc.setLed(0,3,0,true);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
    }

 if((totalsectime == 10) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,true);
    lc.setLed(0,3,0,true);
    lc.setLed(0,4,0,true);
    lc.setLed(0,5,0,false);
    }

 if((totalsectime == 9) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,true);
    lc.setLed(0,3,0,true);
    lc.setLed(0,4,0,true);
    lc.setLed(0,5,0,true);
    }

 if((totalsectime == 8) && (running == true)) {  
    lc.setLed(0,0,0,false);
    lc.setLed(0,1,0,false);
    lc.setLed(0,2,0,false);
    lc.setLed(0,3,0,false);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
    }

 if((totalsectime == 7) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,false);
    lc.setLed(0,2,0,false);
    lc.setLed(0,3,0,false);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
    }

 if((totalsectime == 6) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,false);
    lc.setLed(0,3,0,false);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
    }

 if((totalsectime == 5) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,true);
    lc.setLed(0,3,0,false);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
    }

 if((totalsectime == 4) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,true);
    lc.setLed(0,3,0,true);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
    }    

 if((totalsectime == 3) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,true);
    lc.setLed(0,3,0,true);
    lc.setLed(0,4,0,true);
    lc.setLed(0,5,0,false);
    }    

 if((totalsectime == 2) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,true);
    lc.setLed(0,3,0,true);
    lc.setLed(0,4,0,true);
    lc.setLed(0,5,0,true);
    }     

 if((totalsectime == 1) && (running == true)) {  
    lc.setLed(0,0,0,false);
    lc.setLed(0,1,0,false);
    lc.setLed(0,2,0,false);
    lc.setLed(0,3,0,false);
    lc.setLed(0,4,0,false);
    lc.setLed(0,5,0,false);
    }      

 if((totalsectime == 0) && (running == true)) {  
    lc.setLed(0,0,0,true);
    lc.setLed(0,1,0,true);
    lc.setLed(0,2,0,true);
    lc.setLed(0,3,0,true);
    lc.setLed(0,4,0,true);
    lc.setLed(0,5,0,true);
    } 
  }

//antirebond bp start
void debounceSTART() {

  int reading = digitalRead(BP_START);
  if (reading != OLD_BP_START_STATUS) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime)>= debounceDelay) {
    if (reading != BP_START_STATUS) {
      BP_START_STATUS = reading;
    }
  }
  OLD_BP_START_STATUS = reading;
}

//antirebond bp vortex
void debounceVORTEX() {

  int reading1 = digitalRead(BP_VORTEX);
  if (reading1 != OLD_BP_VORTEX_STATUS) {
    lastDebounceTime1 = millis();
  }
  if ((millis() - lastDebounceTime1)>= debounceDelay1) {
    if (reading1 != BP_VORTEX_STATUS) {
      BP_VORTEX_STATUS = reading1;
    }
  }
  OLD_BP_VORTEX_STATUS = reading1;
}

//mode test toutes les LED et ecrans alumer pendant 10s
void mode_test() {
  strip.show();
  lc.setRow(0,0,B11111111); //ecrans hhmmss ddd et baregraphe
  lc.setRow(0,1,B11111111);
  lc.setRow(0,2,B11111111);
  lc.setRow(0,3,B11111111);
  lc.setRow(0,4,B11111111);
  lc.setRow(0,5,B11111111);
  lc.setRow(1,0,B11111111);
  lc.setRow(1,1,B11111111);
  lc.setRow(1,2,B11111111);
  lc.setRow(1,3,B11111111);
  lc.setRow(1,4,B11111111);
    lc.setLed(0,6,1,true); //colons
    lc.setLed(0,6,2,true);
    lc.setLed(0,6,3,true);
    lc.setLed(0,6,4,true);  
    lc.setRow(1,6,B01111000); //led extremiter chenillard
     lc.setLed(1,5,3,true); //jaune
     lc.setLed(1,5,4,true); //rouge
     lc.setLed(1,5,5,true); //verte
     lc.setLed(1,7,0,true); //emetteur
     lc.setLed(1,7,1,true);
     lc.setLed(1,7,2,true);
     lc.setLed(1,7,3,true);
  tone(speaker, 3000, 5000);
  strip.fill(strip.Color(255,0,0),0,7);

  delay(10000);

  lc.setRow(0,0,B00000000); //ecrans hhmmss ddd et baregraphe
  lc.setRow(0,1,B00000000);
  lc.setRow(0,2,B00000000);
  lc.setRow(0,3,B00000000);
  lc.setRow(0,4,B00000000);
  lc.setRow(0,5,B00000000);
  lc.setRow(1,0,B00000000);
  lc.setRow(1,1,B00000000);
  lc.setRow(1,2,B00000000);
  lc.setRow(1,3,B00000000);
  lc.setRow(1,4,B00000000);
    lc.setLed(0,6,1,false); //colons
    lc.setLed(0,6,2,false);
    lc.setLed(0,6,3,false);
    lc.setLed(0,6,4,false);  
    lc.setRow(1,6,B00000000); //led extremiter chenillard
     lc.setLed(1,5,3,false); //jaune
     lc.setLed(1,5,4,false); //rouge
     lc.setLed(1,5,5,false); //verte
     lc.setLed(1,7,0,false); //emetteur
     lc.setLed(1,7,1,false);
     lc.setLed(1,7,2,false);
     lc.setLed(1,7,3,false);
  noTone(speaker);  
  strip.fill(strip.Color(0,0,0),0,7);
}

//scenario buzzer menu++
void buzzer_menu() {
  if((!running)&&(BP_POWER_STATUS == true)) {
  tone(speaker, 3100, 200);
}
}

//scenario activation bp menu++
void menu_up() {
  if(BP_POWER_STATUS == true) {
    menu++;
  }
}

//calcul % batterie
int getBattery ()
{
  float b = analogRead(BATTERYPIN); //valeur analogique

  int minValue = (1023 * TensionMin) / 5; //Arduino
  int maxValue = (1023 * TensionMax) / 5; //Arduino

  b = ((b - minValue) / (maxValue - minValue)) * 100; //mettre en pourcentage

  if (b > 100) //max is 100%
    b = 100;

  else if (b < 0) //min is 0%
    b = 0;
  int valeur = b;
  return b;
}

//commande controle batterie
void batterie() {
  
  //sauvegarde du temps a l'ecran pour affichage temps restant
  unsigned long totalsectime_save;  
 
  noTone(speaker);
  running = false;                  //arret minuteur          
  totalsectime_save = totalsectime; //sauvegarde du chrono          
            
            lc.setRow(0,0,B00011111);         //b
            lc.setRow(0,1,B01111101);         //a
            lc.setRow(0,2,B00000111);         //t
            lc.setRow(0,3,B01101111);         //e
            lc.setRow(0,4,B00000101);         //r     
            lc.setRow(0,5,B00111011);         //y
            lc.setRow(1,0,B00000000);         
            lc.setRow(1,1,B00000000);         
            lc.setRow(1,2,B00000000);             
            delay(1500);  
                             
            totalsectime = getBattery;
            affiche_temps();
            lc.setRow(1,0,B00000000);         
            lc.setRow(1,1,B00000000);         
            lc.setRow(1,2,B00000000);
            lc.setRow(0,0,B00011111);        
            lc.setRow(0,1,B01111101);         
            lc.setRow(0,2,B10000111);
            delay(2500);

            //restoration du temps de chono et affichage
            totalsectime = totalsectime_save;
            lc.clearDisplay(0);
            lc.clearDisplay(1);
            running = true; 
}

//zap rapide menu
void zap() {
  menu = 5;
}
