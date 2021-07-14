#include <Arduino.h>
#include <ArduinoLowPower.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SigFox.h>
#include "ZeroPowerManager.h" 
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <DHT.h>


// ------- Mode de fonctionnement -------
volatile int OPERATING_MODE;

#define OUVERTURE_MODE 11 //code de correspondance pour le mode de fonctionnement d'ouverture
#define FERMETURE_MODE 12 //code de correspondance pour le mode de fonctionnement de la fermeture
#define SLEEPING_MODE 13 //code de correspondance pour le mode de fonctionnement de mise en sommeil
// ---------------------------------------

#define attach_interrupt 1

// Affectation des broches et création de MFRC522
#define RST_PIN 6
#define SS_PIN 11

//Gestion moteurs
#define pinINA1 A2 // Moteur A, entrée 1 - Commande en PWM possible
#define pinINA2 A3 // Moteur A, entrée 2 - Commande en PWM possible

// Définition gestion des interruptions
#define pin_msg 1 // Broche d'interruption du bouton poussoir
#define pin_alert 0 // Broche d'interruption du bouton poussoir
#define pin_int2 A1 // Broche de la seconde interruption après fermeture du bac

// Broche du buzzer
#define buzzer_pin 5

// Affectation des broches pour les LEDs de debug
#define Rled 2
#define Jled 3
#define Bled 4

// Supervision du niveau de batterie
#define battery_level_pin A5


#define relaypin 12

// Interrupteur détectant l'ouverture et la fermeture du loquet.
#define limit1 A4
#define limit2 A6

// Standby du moteur
#define motor_stby 7

#define DEBUG 1 // Variable DEBUG (activation LEDs, delay, liaison série)

//Variables nécessaire à l'implémentation du temps d'attente avec retour en sommeil profond
#define INTERVAL_TAGS 10000 // interval de 30 secondes
unsigned long time_tags = 0;

#define DHTPIN 2     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
int chk;
float hum;  //Stores humidity value
float temp; //Stores temperature value

// Structure du message sigfox qui va être envoyé par le biais du réseau
typedef struct __attribute__ ((packed)) sigfox_message {
          int8_t uid[6];
          int8_t battery[2];
          // int8_t duree[2];
          int8_t temperature[2];
          int8_t humidity;
} SigfoxMessage;


byte readCard[4]; // Variable qui permet de stocker l'UID du tag 

//FLags de gestion du mode de fonctionnement
volatile int ALERT_FLAG;

// Variable des durées d'exécution relative à chaque partie du code.
unsigned long duree_ouverture;
unsigned long duree_fermeture;
unsigned long sigfox_time;


// Création de l'entité de contrôle pour le module arduino RFID
MFRC522 mfrc522(SS_PIN, RST_PIN);

DHT dht(DHTPIN, DHTTYPE);

// Définition du message pour Sigfox 
SigfoxMessage msg;

// =========== CONVERTIS UN MESSAGE DE 4 OCTETS EN HEXA
char *MsgtoHex( byte msg[6])
{
  const static char *hex="0123456789ABCDEF";
  static char toHexBuf[4*3];
  
  uint8_t c;
  
  for ( uint8_t i = 0; i < 4; i++) {  //
    c = msg[i];
    toHexBuf[i+0]= hex[c>>4];
    toHexBuf[i+1]= hex[c & 0x0f];
    toHexBuf[i+2]=0;
    
  }
  return toHexBuf;
}

// =========== MISE EN SOMMEIL DU MODULE RFID RC522
// Induit la mise en power off du module RFID dans le cas ou le drapeau est à "1"
void sleep_RC522(bool sleep_flag){
  if(sleep_flag){
    digitalWrite(RST_PIN, LOW);
    delay(100);
  }else{
    digitalWrite(RST_PIN, HIGH);
    delay(100);
  }
}


// =========== RÉCUPÉRATION DE L'IDENTIFIANT DU TAG
uint8_t getID() {
  Serial1.println("trying to get an ID");
  // Prépataion pour la lecture des tags RFID
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //Si un nouveau PICC est placé sur le capter RFID on continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial1 and continue
    return 0;
  }

  // Début lecture UID
  for ( uint8_t i = 0; i < 4; i++) {  // Récupération de l'UID de la carte
    readCard[i] = mfrc522.uid.uidByte[i]; 
  }
  mfrc522.PICC_HaltA(); // Arrête de la lecture

  return 1;
}


// =========== CALLBACK INTERRUPTION BOUTON ALERTE
void alert_wk(){
  // Serial1.println("alert wk"); 
  // delay(50);
  OPERATING_MODE = OUVERTURE_MODE;

  // Serial1.println("changement du mode de fonctionnement");
  // Serial1.println(OPERATING_MODE);
  // delay(50);
  ALERT_FLAG = 1;

}

// =========== CALLBACK INTERRUPTION BOUTON TAG
void tag_wk(){
  // Serial1.println("tag wk");
  // delay(50);
  OPERATING_MODE = OUVERTURE_MODE;
  ALERT_FLAG = 0;

  // Serial1.println("changement du mode de fonctionnement");
  // Serial1.println(OPERATING_MODE);
  // delay(50);

}

// =========== CALLBACK INTERRUPTION FERMETURE DU BAC
void int2_wk(){
  Serial1.println("second interrupt");
  delay(50);
  OPERATING_MODE = FERMETURE_MODE;

  Serial1.println("changement du mode de fonctionnement");
  Serial1.println(OPERATING_MODE);
}

// =========== DÉTECTION DU NIVEAU DE BATTERIE
int16_t battery_level(){
  String s_voltage = "";
  int sensorValue = analogRead(battery_level_pin);
  float voltage = sensorValue * (((6 / 1023.0)*100)/6);
  s_voltage.concat(voltage);

  return s_voltage.toInt();
}

// =========== REBOOT
void reboot() {
  NVIC_SystemReset();
  while (1);
}

// =========== SETUP
void setup()
{
  zpmRTCInit();

    // INITIALISATION SIGFOX
  if (DEBUG) {
    Serial1.begin(9600);
  }

  // INITIALISATION SIGFOX
  // SigFox.begin();
  // delay(200);
  // SigFox.end();
  // delay(200);

  if (!SigFox.begin()) {
    // reboot si problème avec le Sigfox
    // le reboot se produit dans le cas ou la source d'énergie est instable.
    reboot();
  }

  // Send module to standby until we need to send a message
  SigFox.end();

  if (DEBUG) {
    // Enable DEBUG prints and LED indication if we are testing
    SigFox.debug();
  }

  pinMode(pin_msg, INPUT); //Bouton en pullup
  pinMode(pin_alert, INPUT); //Bouton en pullup
  pinMode(pin_int2, INPUT); //Bouton en pullup

  pinMode(buzzer_pin, OUTPUT);
  pinMode(motor_stby, OUTPUT);
  digitalWrite( motor_stby, LOW);

  pinMode(RST_PIN, OUTPUT);

  pinMode(limit1, INPUT); //DEBUG CONSO
  pinMode(limit2, INPUT); //DEBUG CONSO

  pinMode( pinINA1, OUTPUT ); //DEBUG CONSO
  pinMode( pinINA2, OUTPUT ); //DEBUG CONSO

  // Activation des Leds de debug si DEBUG = 1

  pinMode(relaypin, OUTPUT); //DEBUG CONSO

  SPI.begin(); //DEBUG CONSO
  dht.begin(); //DEBUG CONSO

  OPERATING_MODE = SLEEPING_MODE;
  ALERT_FLAG = 0;
  Serial1.println("changement du mode de fonctionnement");
  Serial1.println(OPERATING_MODE);
 
  //------------------------------------------------------------------
  // Configuration des interruptions

  LowPower.attachInterruptWakeup(pin_msg, tag_wk, FALLING );
  LowPower.attachInterruptWakeup(pin_alert, alert_wk, FALLING);
  LowPower.attachInterruptWakeup(pin_int2, int2_wk, FALLING);

  Serial1.println("setup done");
}

// =========== ENVOIS DONNÉS AVEC SIGFOX
void send_data(SigfoxMessage msg){
  Serial1.println("Send data");
  sigfox_time = millis(); // Millis pour le calcul de la durée de transmission Sigfox

  digitalWrite(Bled, HIGH);
  
  // Différent tone du buzzer selon si l'utilisateur envoi une alerte ou non 
  if(ALERT_FLAG){
    tone(buzzer_pin, 200, 500);
  }else{
    tone(buzzer_pin, 1000, 500);
  }
 
  // Démarrage du module
  SigFox.begin();
  // Attente d'au moins 30ms pour l'initialisation des fonctionnalité du module.
  delay(100);
  
  // Nettoyage de toute les interruptions en cours.
  SigFox.status();
  delay(1);

  //Transmission du message
  SigFox.beginPacket();
  SigFox.write((uint8_t*)&msg, sizeof(msg));
  int ret = SigFox.endPacket();
  if(ret > 0) {
          Serial1.println("No transmission");
  } else {
          Serial1.println("Transmission ok");
  }
  delay(50);

  SigFox.end();

  digitalWrite(Bled, LOW);
  sigfox_time = millis() - sigfox_time;
}

// =========== FONCTION DE CONTROLE DU MOTEUR
void power_motor(bool on){
  if(on){
    Serial1.println("Motor driver 'on'");
    //Réveil du module de pilotage
    digitalWrite( motor_stby, HIGH);
    delay(100); 

    //Moteur en marche
    digitalWrite( pinINA1, LOW );
    analogWrite(pinINA2, 70);
  }else{
    //Moteur en marche
    Serial1.println("Motor driver 'off'");
    digitalWrite( pinINA1, LOW);
    analogWrite( pinINA2, 0); 

    //Mise en sommeil du module de pilotage
    digitalWrite( motor_stby, LOW);
    delay(100);
    Serial1.println("c'est good");
  }

}

// =========== ROUTINE DE MISE EN SOMMEIL DE LA CARTE
void sleeping_routine(){
    digitalWrite(relaypin, LOW);
    delay(50);
    Serial1.println("sleeping time!!");
    //zpmPortDisableDigital();
    zpmPortDisableUSB();
    zpmPortDisableSPI();

    SPI.end();

    // Mise en sommeil profond du module
    LowPower.deepSleep();
}

// =========== ROUTINE DE FONCTIONNEMENT
void routine(){
  delay(50);
  if(ALERT_FLAG){
    Serial1.println("Alert flag on");
    duree_ouverture = millis();
    
    //------------------------------------------------------------------
    // Construction du message à envoyer (avec séparation des données par le caractère '_')

    // Niveau de batterie
    msg.battery[0] = battery_level();
    msg.battery[1] = '_';

    // UID, ou putôt dans ce cas indication d'une alerte
    msg.uid[0] = 'A';
    msg.uid[1] = 'L';
    msg.uid[2] = 'E';
    msg.uid[3] = 'R';
    msg.uid[4] = 'T';
    msg.uid[5] = '_';
  
    // Durée d'exécution 
    // msg.duree[0] = (duree_ouverture + sigfox_time)/1000;
    // msg.duree[1] = '_';

    // Température
    msg.temperature[0] = (int8_t)dht.readTemperature();
    msg.temperature[1] = '_';

    // Humidité
    msg.humidity = (int8_t)dht.readHumidity();

    // Envoi du message
    send_data(msg);

    // Remise en mode sommeil du device
    ALERT_FLAG = 0;
    digitalWrite(relaypin, LOW);
    delay(50);

    sleeping_routine();

  }else{
    
    Serial1.println("Alerte flag off & Message on");
    delay(100);
    sleep_RC522(0);

     Serial1.println("je suis après sleep RC522");
    // Test si lecture d'un badge RFID
    mfrc522.PCD_Init();

    Serial1.println("je suis après PCD init");
    if(getID()){
      Serial1.println("ID GET");

      // digitalWrite(relaypin, LOW);
      sleep_RC522(1);

      power_motor(1);

      // Clignotement de la Led bleu pour le debug
      while(digitalRead(limit1)){
        digitalWrite(Bled, HIGH);
        delay(200);
        digitalWrite(Bled, LOW);
        delay(200);
      } 
      power_motor(0);
  
      // Formattage de son UID
      char* hex_txt = MsgtoHex(readCard);

      // UID, ou putôt dans ce cas indication d'une alerte
      msg.uid[0] = ((String)hex_txt)[0];
      msg.uid[1] = ((String)hex_txt)[1];
      msg.uid[2] = ((String)hex_txt)[2];
      msg.uid[3] = ((String)hex_txt)[3];
      msg.uid[4] = ((String)hex_txt)[4];
      msg.uid[5] = '_';

      // Construction du message
      msg.battery[0] = battery_level();
      msg.battery[1] = '_';

      // Température
      msg.temperature[0] = (int8_t)dht.readTemperature();
      msg.temperature[1] = '_';

      // Humidité
      msg.humidity = (int8_t)dht.readHumidity();

      duree_ouverture = millis() - duree_ouverture;
      Serial.println("end message creation");
      // Mise en sommeil du module
      digitalWrite(relaypin, LOW);
      delay(50);

      sleeping_routine();
      // OPERATING_MODE = SLEEPING_MODE;
      // Serial1.println("changement du mode de fonctionnement");
      // Serial1.println(OPERATING_MODE);
    }
  }
}

// =========== LOOP
void loop()
{ 
  Serial1.println("loop");
  delay(100);
  

  // Switch de changement du mode de fonctionnement
  switch(OPERATING_MODE){
    case OUVERTURE_MODE:

      digitalWrite(relaypin, HIGH);
      delay(50);
      Serial1.println("Je suis dans ouverture");

      duree_ouverture = millis();
      //Fonction d'attente - 30 secondes
      if(millis() >= time_tags + INTERVAL_TAGS){
        Serial1.println("Je suis dans ouverture, temps ecoule");

        sleep_RC522(1);
        digitalWrite(motor_stby, LOW);
        digitalWrite(relaypin, LOW);

        delay(50);

        time_tags +=INTERVAL_TAGS;

        // // LEDs de debug
        digitalWrite(Jled, HIGH);
        delay(1000);
        digitalWrite(Jled, LOW);

        // sleeping_routine();
        
        OPERATING_MODE = SLEEPING_MODE;
        // Serial1.println("changement du mode de fonctionnement");
        // Serial1.println(OPERATING_MODE);

      }else{
        Serial1.println("Je suis dans ouverture, routine");
        // Éxecution de la routine
        routine();
      }
      break;

    case FERMETURE_MODE:
      digitalWrite(relaypin, HIGH);
      delay(50);

      Serial1.println("entrer dans le mode fermeture");
      duree_fermeture = millis();

      power_motor(1);

      // Clignotement de la Led rouge pour le debug
      while(digitalRead(limit2)){
        digitalWrite(Rled, HIGH);
        delay(200);
        digitalWrite(Rled, LOW);
        delay(200);
      }
      power_motor(0);

      duree_fermeture = millis() - duree_fermeture;
     
      // Température
      msg.temperature[0] = (int8_t)dht.readTemperature();
      msg.temperature[1] = '_';

      // Humidité
      msg.humidity = (int8_t)dht.readHumidity();
      delay(50);
     
      // msg.duree[0] = (duree_ouverture + duree_fermeture + sigfox_time)/1000;
      // msg.duree[1] = '_';
      send_data(msg);
      
      digitalWrite(relaypin, LOW);
      delay(50);

      // Mise en sommeil du module une fois le loquet fermé
      // sleeping_routine();
      OPERATING_MODE = SLEEPING_MODE;
      // Serial1.println("changement du mode de fonctionnement");
      // Serial1.println(OPERATING_MODE);
      break;

    case SLEEPING_MODE:
      sleeping_routine();
      break;
  }
}