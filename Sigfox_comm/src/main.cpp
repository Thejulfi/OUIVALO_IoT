#include <Arduino.h>
#include <SigFox.h>
#include <ArduinoLowPower.h>
#include <MFRC522.h>
#include <SPI.h>
#include <DHT.h>  

// ------- Mode de fonctionnement -------
volatile int OPERATING_MODE;

#define INIT 10 //code de correspondance pour le mode de fonctionnement d'ouverture
#define ON_MOTOR 11 //code de correspondance pour le mode de fonctionnement d'ouverture
#define ROUTINE 12 //code de correspondance pour le mode de fonctionnement d'ouverture
#define SEND_MSG 13
#define SLEEPING 14 //code de correspondance pour le mode de fonctionnement d'ouverture
// ------------------------------------- 

// ------- Gestion des interruptions ------
#define wk_button 0 // Broche de la premiere interruption pour ouverture du bac
#define wk_door A1 // Broche de la seconde interruption après fermeture du bac
#define wk_alert 1 // Broche d'interruption du bouton poussoir
volatile int ALERT_FLAG;
volatile int ROUTINE_FLAG;
// ----------------------------------------

// ------- Gestion des mosfets ------------
#define mosfetRFID 3 //power on RFID
#define mosfetSWITCHa 4 // switches
// ----------------------------------------

// ------- Gestion du temps d'attente ------------
#define INTERVAL_TAGS 10000 // interval de 30 secondes
unsigned long time_tags = 0;
// -----------------------------------------------

// ------- Gestion du module RFID RC522 ------------
#define RST_PIN 6
#define SS_PIN 11
MFRC522 mfrc522(SS_PIN, RST_PIN);
// -------------------------------------------------

#define battery_level_pin A5 // Gestion du niveau de batterie
#define buzzer_pin 5 // Buzzer

// ------- Gestion du module DHT ------------
#define DHTPIN 2
#define DHTTYPE DHT22
float hum;
float temp;
DHT dht(DHTPIN, DHTTYPE);
// -------------------------------------------

// ------- Limti switches -------------------
#define limit_open A6
#define limit_close A4
// -------------------------------------------

// ------- Gestion des moteurs ------------
#define pinINA1 A2 // Moteur A, entrée 1 - Commande en PWM possible
#define pinINA2 A3 // Moteur A, entrée 2 - Commande en PWM possible
#define motor_stby 7
// ----------------------------------------

#define DEBUG 0 // Variable à mettre à "1" dans le cas où on souhaite réaliser un debug sur le système.
#define ledR 13 // Led pour indiquer l'état d'ouverture du bac à l'utilisateur

// Structure du message sigfox qui va être envoyé par le biais du réseau
typedef struct __attribute__ ((packed)) sigfox_message {
          int8_t alert[4];
          unsigned long uid;
          int8_t separator;
          int8_t battery[2];
          int8_t temperature[2];
          int8_t humidity;
} SigfoxMessage;

SigfoxMessage msg;


// =========== "ON" ET "OFF" DES MOTEURS ===========
void power_motor(bool on, int vitesse){
  if(on){
    digitalWrite( pinINA1, LOW );
    analogWrite(pinINA2, vitesse);
  }else{
    digitalWrite( pinINA1, LOW);
    analogWrite( pinINA2, 0); 
  }
}

// =========== GESTION DES MOTEURS ===========
void motor_management(int routine_f){
  if(routine_f){
    power_motor(1, 70);
    while(!digitalRead(limit_open)){
      Serial1.println(digitalRead(limit_open));
    }
    power_motor(0, 70);
    
  }else{
    power_motor(1, 70);
    while(!digitalRead(limit_close)){
      Serial1.println(digitalRead(limit_close));
    }
    power_motor(0, 70);
  }
}

// Interrupts management
//******************************************************************
void wk_closing(){
  OPERATING_MODE = INIT;
  ROUTINE_FLAG = 0;
}

void wk_init() {
  OPERATING_MODE = INIT;
  ALERT_FLAG = 0;
  ROUTINE_FLAG = 1;
}

void wk_init_a() {
  OPERATING_MODE = INIT;
  ALERT_FLAG = 1;
  ROUTINE_FLAG = 1;
}

// =========== INITIALISATION DES COMPOSANTS ===========
void init(bool on){
  if(on){
    
    digitalWrite(motor_stby, HIGH);
    delay(10);
    digitalWrite(mosfetSWITCHa, HIGH);
    delay(10);
    digitalWrite(RST_PIN, HIGH);
    delay(10);
    analogWrite(mosfetRFID, 255); 
    delay(10);

    SPI.begin();
    mfrc522.PCD_Init();
    
  }else{
    
    digitalWrite(motor_stby, LOW);
    delay(10);
    digitalWrite(mosfetSWITCHa, LOW);
    delay(10);
    digitalWrite(RST_PIN, LOW);
    delay(10);
    analogWrite(mosfetRFID, 0);
    delay(10); 
   
  }
}

// =========== RÉCUPÉRATION DE L'IDENTIFIANT DU TAG
unsigned long getID(){
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return -1;
  }
  unsigned long hex_num;
  hex_num =  mfrc522.uid.uidByte[0] << 24;
  hex_num += mfrc522.uid.uidByte[1] << 16;
  hex_num += mfrc522.uid.uidByte[2] <<  8;
  hex_num += mfrc522.uid.uidByte[3];
  mfrc522.PICC_HaltA(); // Arrêt de la lecture
  return hex_num;
}

// =========== ENVOIS DONNÉS AVEC SIGFOX
void send_data(SigfoxMessage msg){

  Serial1.println("Send data");
  SigFox.begin();
  delay(100);
  SigFox.beginPacket();
  SigFox.write((uint8_t*)&msg, sizeof(msg));
  int ret = SigFox.endPacket();
  if (ret == 0)
    Serial1.println("OK");
  else
    Serial1.println("KO");

  SigFox.end();
}

// =========== DÉTECTION DU NIVEAU DE BATTERIE
int16_t battery_level(){
  float max_battery_voltage = 4.2;
  
  String s_voltage = "";
  int sensorValue = analogRead(battery_level_pin);
  float voltage = sensorValue * (((max_battery_voltage / 1023.0)*100)/max_battery_voltage);
  s_voltage.concat(voltage);

  return s_voltage.toInt();
}

// Setup
//******************************************************************
void setup() {
  
  //if(DEBUG){
    Serial1.begin(9600); 
  //}else{
    pinMode(ledR, OUTPUT);
  //}
  
  delay(2000); //délais pour laisser le temps de charger un code arduino

  // Bouton déclenchement interruption
  pinMode(wk_button, INPUT);
  pinMode(wk_button, INPUT); // AJOUTE APRES
  pinMode(wk_door, INPUT); // AJOUTE APRES

  // Buzzer
  pinMode(buzzer_pin, OUTPUT);
  
  // Mosfets de contrôle d'accès au courant
  pinMode(mosfetRFID, OUTPUT);
  pinMode(mosfetSWITCHa, OUTPUT);

  pinMode( pinINA1, OUTPUT );
  pinMode( pinINA2, OUTPUT );

  // Gestion sommeil composants
  pinMode(motor_stby, OUTPUT); 
  pinMode(RST_PIN, OUTPUT);

  // Niveau de batterie
  pinMode(battery_level_pin, INPUT);

  //Switch
  pinMode(limit_open, INPUT);
  pinMode(limit_close, INPUT);
   

  //Initialisation Sigfox
  if (!SigFox.begin()) {
    Serial1.println("Shield error or not present!");
    return;
  }
  SigFox.end();
  SigFox.debug();
  
  //------------------------------------------------------------------
  //  Gestion du réveil
  
  pinMode(wk_button , INPUT);
  LowPower.attachInterruptWakeup(wk_button, wk_init, FALLING);
  
  pinMode(wk_door , INPUT);
  LowPower.attachInterruptWakeup(wk_door, wk_closing, RISING);
  
  pinMode(wk_alert , INPUT);
  LowPower.attachInterruptWakeup(wk_alert, wk_init_a, RISING);
  
  //------------------------------------------------------------------

  // Initialisation des composants 
  dht.begin();
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();


  // Initialisation des flags et définition de mode de fonctionnement initial.
  OPERATING_MODE = SLEEPING;
  ALERT_FLAG = 0;
  ROUTINE_FLAG = 1;
  
  Serial1.println("end setup");

}
//******************************************************************

void routine(int routine_f, bool alert) {
  if(routine_f){
    Serial1.println("routine 1");
      if(mfrc522.PICC_IsNewCardPresent()) {
        unsigned long uid = getID();
        if(uid != -1){
          
          Serial1.print("Card detected, UID: "); Serial1.println(uid);

          // ------- Construction du message à envoyer -------
          if(alert){
            msg.alert[0] = 'O';
            msg.alert[1] = 'U';
            msg.alert[2] = 'I';
            msg.alert[3] = '_';
            tone(buzzer_pin, 200, 500);
            OPERATING_MODE = SEND_MSG; 
          }else{
            msg.alert[0] = 'N';
            msg.alert[1] = 'O';
            msg.alert[2] = 'N';
            msg.alert[3] = '_';
            tone(buzzer_pin, 1000, 500);
            OPERATING_MODE = ON_MOTOR;
          }

          //UID
          msg.uid = uid;
          msg.separator = '_';

          //Battery level
          msg.battery[0] = battery_level();
          msg.battery[1] = '_';
          Serial1.println(battery_level());

          //Temperature
          msg.temperature[0] = (int8_t)dht.readTemperature();
          Serial1.println(dht.readTemperature());
          
          msg.temperature[1] = '_';

          //Humidity
          msg.humidity = (int8_t)dht.readHumidity();
          Serial1.println(dht.readHumidity());

          // ------------------------------------------
        }
      }else{
        Serial1.println(F("No card found"));
        
      }
  }else{
    Serial1.println("routine 2");
    OPERATING_MODE = ON_MOTOR;
    
  }
}

// Loop
//******************************************************************
void loop() {

  switch(OPERATING_MODE){
    case INIT:
    
      Serial1.println("INIT mode");
      
      init(1);

      OPERATING_MODE = ROUTINE;

      break;
      
    case ROUTINE:
    
      Serial1.println("ROUTINE mode");
      
      routine(ROUTINE_FLAG, ALERT_FLAG);
      
      break;
      
    case ON_MOTOR:
    
      Serial1.println("ON_MOTOR mode");
      digitalWrite(ledR, HIGH);
      
      motor_management(ROUTINE_FLAG);

      if(ROUTINE_FLAG){
        OPERATING_MODE = SEND_MSG;
      }else{
        OPERATING_MODE = SLEEPING; 
      }

      digitalWrite(ledR, LOW);

      break;
      
    case SEND_MSG:
      Serial1.println("SEND_MSG mode");

      send_data(msg);
      delay(50);

      OPERATING_MODE = SLEEPING;
      
      break;

    case SLEEPING:
      Serial1.println("SLEEPING mode");

      init(0);
    
      LowPower.deepSleep();
      break;
  }
}