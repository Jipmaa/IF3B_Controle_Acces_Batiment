#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Stepper.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>          // Cette bibliothèque permet à l'ESP32 de se connecter au réseau WiFi.
#include <PubSubClient.h>  // Cette bibliothèque vous permet d'envoyer et de recevoir des messages MQTT.


//Oled
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
//Keypad
#define RX_PIN 17  // Connecté à TX du keypad
#define TX_PIN 16  // Connecté à RX du keypad
// Declaration for SSD1306 display connected using I2C
#define OLED_RESET -1  // Reset pin
#define SCREEN_ADDRESS 0x3C
//Led Stick
#ifdef __AVR__
#include <avr/power.h>  // Required for 16 MHz Adafruit Trinket
#endif
#define LED_PIN 33 //au lieu de 19
#define LED_COUNT 8
#define RST_PIN 4
#define SS_PIN 5

// "Variables" globales
const int relay = 32;
const int buzzer = 13;
const int ledRed = 33; //au lieu de 33
const int stepsPerRevolution = 2038;
int red = 255;
int green = 255;
int blue = 255;
String lastMessageOLED = "";

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins) = oled
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// rfid
MFRC522 mfrc522(SS_PIN, RST_PIN);
// Init array that will store new NUID
byte nuidPICC[4];
// Stepper
Stepper myStepper(stepsPerRevolution, 14, 26, 27, 25);
//Keypad
HardwareSerial TRANS_SERIAL(1);  // Utiliser le port UART1
String codeCorrect = "5190";
String codeCorrectEmploye = "1234";
String codeSaisi = "";
//LedStocl
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);
//Wifi
WiFiClient espClientTHUNE;                 // Initialiser la bibliothèque client wifi (connexion WiFi)
PubSubClient clientTHUNE(espClientTHUNE);  // Créer un objet client MQTT (connexion MQTT)

const char* ssid = "Partage de co";                //Nom du wifi
const char* password = "Tomates08";                //Mot de passe du wifi
const char* mqtt_server = "mqtt.ci-ciad.utbm.fr";  //Nom du serveur
long lastMsg = 0;

void setup() {
  Serial.begin(9600);  // Initialisation du port série
  while (!Serial)
    ;
  delay(100);
  Serial.println("Debug: Début de setup.");

  pinMode(ledRed, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);
  myStepper.setSpeed(8);

  //rfid
  SPI.begin();                        // Init SPI bus
  mfrc522.PCD_Init();                 // Init MFRC522
  delay(100);                         // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

  // Initialisation de l'écran OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Erreur : OLED non détecté."));
    while (true)
      ;  // Boucle infinie si l'OLED échoue
  }
  affichageScan(0, "a");  // Affichage par défaut

  // Initialisation du clavier
  TRANS_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("Debug: Clavier initialisé.");

  // Initialisation de la bande LED
  strip.begin();
  strip.show();
  strip.setBrightness(50);
  colorWipe(strip.Color(green, red, blue), 1);  // Couleur blanche par défaut


  setup_wifi();
  clientTHUNE.setServer(mqtt_server, 1883);
  clientTHUNE.setCallback(callback);
  Serial.println("Debug: Fin de setup.");
}



void loop() {
  checkingData();
  recupDataKeyboard();
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  Serial.println("Erreur 3333333333333333333333333");
  String UID = "";

  for (byte i = 0; i < mfrc522.uid.size; i++) {
    UID.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    UID.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  UID.toUpperCase();

  if (UID.substring(1) == "36 92 8B 44" || ((UID.substring(1) == "8B 7E BD 22" || UID.substring(1) == "BA FC A6 16" || UID.substring(1) == "39 77 91 6D") && codeSaisi == codeCorrectEmploye)) {
    sendData(2);
    affichageScan(1, "Employe\nreconnu");
    delay(2000);
    ouverture();
  } else if (UID.substring(1) == "8B 7E BD 22" || UID.substring(1) == "BA FC A6 16" || UID.substring(1) == "39 77 91 6D") {
    sendData(1);
    ouverture();
  }

  else {
    affichageScan(2, "Badge non autorise");
    erreur();
  }
}

void ouverture() {
  codeSaisi = "";
  Serial.println("test beginning condition");

  affichageScan(1, "Ouverture");  //oled

  //https://javl.github.io/image2cpp/?pseSrc=pgEcranOledArduino
  //serrure
  digitalWrite(relay, HIGH);  //serrure "éjecté"

  digitalWrite(ledRed, LOW);

  tone(buzzer, 1000, 200);
  delay(50);
  tone(buzzer, 250, 200);

  //moteur
  Serial.println("Ouverture");
  myStepper.step(-stepsPerRevolution * 1.2);
  green = 255;
  red = 0;
  blue = 0;
  for (int i = 0 ; i < 5 ; i++) {
  colorWipe(strip.Color(green, red, blue), 100);
  green = 0;
  red = 255;
  colorWipe(strip.Color(green, red, blue), 100);
  red = 0;
  blue = 255;
  colorWipe(strip.Color(green, red, blue), 100);
  blue = 0;
  green = 255;
  colorWipe(strip.Color(green, red, blue), 100);
  }

  affichageScan(1, "Fermeture imminente");

  for (int j = 0; j < 5; j++) {
    digitalWrite(ledRed, HIGH);
    delay(500);
    digitalWrite(ledRed, LOW);
    delay(500);
  }

  digitalWrite(relay, LOW);
  tone(buzzer, 250, 200);
  affichageScan(1, "Fermeture");

  // 1 rotation counterclockwise:
  Serial.println("Fermeture");
  myStepper.step(stepsPerRevolution * 1.2);
  delay(1000);

  colorWipe(strip.Color(255, 255, 255), 1);  // White

  affichageScan(0, "a");
}

void erreur() {

  sendData(0);

  tone(buzzer, 1000, 200);
  delay(50);
  tone(buzzer, 250, 1000);

  for (int j = 0; j < 5; j++) {
    digitalWrite(ledRed, HIGH);
    delay(500);
    digitalWrite(ledRed, LOW);
    delay(500);
  }

  digitalWrite(ledRed, HIGH);
  delay(5000);
  digitalWrite(ledRed, LOW);

  affichageScan(0, "a");

  Serial.println("ceci est un test dans l'erreur");
}


//RFID
// TO DELETEEE
/*
void readRFID()
{ 
  
  // Look for new card
  if ( ! mfrc522.PICC_IsNewCardPresent()) 
  return;
 
    // Verify if the NUID has been readed
  if (  !mfrc522.PICC_ReadCardSerial())
  return;
  
  if (mfrc522.uid.uidByte[0] != nuidPICC[0] || 
    mfrc522.uid.uidByte[1] != nuidPICC[1] || 
    mfrc522.uid.uidByte[2] != nuidPICC[2] || 
    mfrc522.uid.uidByte[3] != nuidPICC[3] ) {
    Serial.println(F("A new card has been detected."));
 
    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = mfrc522.uid.uidByte[i];
    }
    Serial.print(F("RFID tag in dec: "));
    printDec(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
  }
  // Halt PICC
  mfrc522.PICC_HaltA();
 
  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();
}
 
 

void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
*/
// Affiche un message par défaut si x = FALSE
void affichageScan(int x, String msg) {

  if (x == 1) {

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(5, 30);

    // Display static text
    display.println(msg);
    display.display();
    display.stopscroll();

  } else if (x == 0) {

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(5, 30);

    // Display static text
    display.println("Scan badge");
    display.display();
    display.stopscroll();

  } else if (x == 2) {
    display.clearDisplay();
    display.setTextColor(INVERSE);
    display.setCursor(5, 30);
    display.println(msg);
    display.startscrollleft(0x00, 0x07);
    display.display();
  }
}


// Keypad
void recupDataKeyboard() {
  while (TRANS_SERIAL.available()) {
    uint8_t data = TRANS_SERIAL.read();
    switch (data) {
      case 0xE1:  // "1"
        Serial.println("1");
        ajouterTouche('1');
        tone(buzzer, 750, 200);
        break;
      case 0xE2:  // "2"
        Serial.println("2");
        ajouterTouche('2');
        tone(buzzer, 800, 200);
        break;
      case 0xE3:  // "3"
        Serial.println("3");
        ajouterTouche('3');
        tone(buzzer, 850, 200);
        break;
      case 0xE4:  // "4"
        Serial.println("4");
        ajouterTouche('4');
        tone(buzzer, 900, 200);
        break;
      case 0xE5:  // "5"
        Serial.println("5");
        ajouterTouche('5');
        tone(buzzer, 950, 200);
        break;
      case 0xE6:  // "6"
        Serial.println("6");
        ajouterTouche('6');
        tone(buzzer, 1000, 200);
        break;
      case 0xE7:  // "7"
        Serial.println("7");
        ajouterTouche('7');
        tone(buzzer, 1050, 200);
        break;
      case 0xE8:  // "8"
        Serial.println("8");
        ajouterTouche('8');
        tone(buzzer, 1100, 200);
        break;
      case 0xE9:  // "9"
        Serial.println("9");
        ajouterTouche('9');
        tone(buzzer, 1150, 200);
        break;
      case 0xEA:  // "*"
        Serial.println("*");
        Serial.println("Effacement du code saisi.");
        tone(buzzer, 1200, 200);
        affichageScan(0, "a");
        digitalWrite(ledRed, HIGH);
        delay(500);
        digitalWrite(ledRed, LOW);
        codeSaisi = "";  // Réinitialise le code
        break;
      case 0xEB:  // "0"
        Serial.println("0");
        ajouterTouche('0');
        tone(buzzer, 1250, 200);
        break;
      case 0xEC:  // "#"
        Serial.println("#");
        tone(buzzer, 1300, 200);
        verifierCode();  // Vérifie le code saisi
        break;
      default:
        Serial.println("Touche inconnue.");
    }
  }
}


void ajouterTouche(char touche) {
  codeSaisi += touche;  // Ajoute la touche au code saisi
  affichageCode();
}


void verifierCode() {
  Serial.println("Code entré : ");
  Serial.print(codeSaisi);

  if (codeSaisi == codeCorrect) {

    Serial.println("Code correct ! Ouverture...");
    affichageScan(1, "Code bon");
    delay(500);
    sendData(1);
    ouverture();

  } else {

    Serial.println("Code incorrect ! Accès refusé.");
    affichageScan(1, "Code\nincorrect");
    erreur();
    delay(500);
  }
}



void affichageCode() {

  Serial.print("Longueur du code : ");
  Serial.println(codeSaisi.length());
  Serial.print("Code saisi : ");
  Serial.println(codeSaisi);

  if (codeSaisi.length() < 5) {  // à remettre 5 ici

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(25, 30);

    for (int i = 0; i < codeSaisi.length(); i++) {

      display.print("*");
    }

    display.display();
  }

  else {
    affichageScan(1, "Code\nincorrect");
    codeSaisi = "";
    erreur();
  }
}



// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait) {
  green = 255;
  red = 0;
  blue = 0;
  for (int i = 0 ; i < 5 ; i++) {
    colorWipe(strip.Color(green, red, blue), 100);
    green = 0;
    red = 255;
    colorWipe(strip.Color(green, red, blue), 100);
    red = 0;
    blue = 255;
    colorWipe(strip.Color(green, red, blue), 100);
    blue = 0;
    green = 255;
    colorWipe(strip.Color(green, red, blue), 100);
  }
}


void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); i++) {  // For each pixel in strip...
    strip.setPixelColor(i, color);               //  Set pixel's color (in RAM)
    strip.show();                                //  Update strip to match
    delay(wait);                                 //  Pause for a moment
  }
}


//------Cette fonction se connecte au réseau WiFi en utilisant les paramètres de connexion fournis
//dans les variables ssid et password.
void setup_wifi() {
  delay(10);  // Cette instruction pause l'exécution du programme pendant 10 millisecondes.
  // We start by connecting to a WiFi network
  Serial.println();  // Imprime une ligne vide/saut de ligne dans la console série.
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);  // //démarre la connexion Wi-Fi avec les informations de connexion (SSID et mot de passe) fournies.

  //Cette boucle effectue une pause de 500 millisecondes jusqu'à ce que l'ESP32 soit
  //connecté au réseau Wi-Fi. Le statut de la connexion est obtenu en appelant "WiFi.status()".
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


// Application des requêtes MQTT reçues via nodered
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);  // imprime le nom du Topic sur lequel le message a été publié.
  Serial.println(". Message: ");
  String messageTemp;  // déclare une variable de chaîne temporaire pour stocker le message reçu.

  // boucle sur chaque élement dans le tableau de bytes "message" et les ajoute à la chaîne "messageTemp".
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (String(topic) == "esp32/maison/door") {
    if (messageTemp == "onDoor") {
      myStepper.step(-stepsPerRevolution * 1.2);  // Ouvrir la porte
      Serial.println("Porte ouverte !");
    } else if (messageTemp == "offDoor") {
      myStepper.step(stepsPerRevolution * 1.2);  // Fermer la porte
      Serial.println("Porte fermée !");
    } else {
      Serial.println("Problème messageTemp pour Moteur");
    }
  }


  if (String(topic) == "esp32/maison/ledRed") {
    if (messageTemp == "onLed") {
      digitalWrite(ledRed, HIGH);
    } else if (messageTemp == "offLed") {
      digitalWrite(ledRed, LOW);
    } else {
      Serial.println("Problème messageTemp pour LedRed");
    }
  }



  if (String(topic) == "esp32/maison/buzzer") {
    if (messageTemp == "1") {
      playJingleBells();
    } else if (messageTemp == "2") {
      playBackInBlack();
    } else {
      Serial.println("Problème messageTemp pour buzzer");
    }
  }

  if (String(topic) == "esp32/maison/ledStickRed") {
    red = messageTemp.toInt();
    colorWipe(strip.Color(green, red, blue), 100);
  }

  if (String(topic) == "esp32/maison/ledStickGreen") {
    green = messageTemp.toInt();
    colorWipe(strip.Color(green, red, blue), 100);
  }

  if (String(topic) == "esp32/maison/ledStickBlue") {
    blue = messageTemp.toInt();
    colorWipe(strip.Color(green, red, blue), 100);
  }

  if (String(topic) == "esp32/maison/ledStickRainbow") {
    rainbow(10);
    Serial.println("Rainbowwww");
  }

  if (String(topic) == "esp32/maison/ecran2") {
    display.setTextSize(messageTemp.toInt());
    affichageScan(2, lastMessageOLED);
    Serial.println("Changement taille texte");
  }
  if (String(topic) == "esp32/maison/ecran") {
    if (messageTemp == "EraseAll") {
      affichageScan(0, "a");
      lastMessageOLED = "Scan badge";
    } else {
      affichageScan(2, messageTemp);
      lastMessageOLED = messageTemp;
    }
  }
}



//La fonction "reconnect()" est utilisée pour garantir la connexion MQTT entre l'ESP32 et le broker MQTT.
//Elle est appelée dans la boucle principale et elle se répète jusqu'à ce que la connexion soit établie avec succès.
void reconnect() {
  while (!clientTHUNE.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (clientTHUNE.connect("espClientTHUNE")) {
      Serial.println("connected");
      // Subscribe
      clientTHUNE.subscribe("esp32/maison/ledRed");
      clientTHUNE.subscribe("esp32/maison/buzzer");
      clientTHUNE.subscribe("esp32/maison/door");
      clientTHUNE.subscribe("esp32/maison/ledStickRed");
      clientTHUNE.subscribe("esp32/maison/ledStickBlue");
      clientTHUNE.subscribe("esp32/maison/ledStickGreen");
      clientTHUNE.subscribe("esp32/maison/ledStickRainbow");
      clientTHUNE.subscribe("esp32/maison/ecran");
      clientTHUNE.subscribe("esp32/maison/ecran2");
    } else {
      Serial.print("failed, rc=");
      Serial.print(clientTHUNE.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}






int lastLedState = 0;


// Met à jour les valeurs sur le nodered
void checkingData() {
  // La première tâche de la fonction principale est de vérifier si le client MQTT est connecté.
  //Si ce n'est pas le cas, la fonction reconnect() est appelée pour reconnecter le client.
  if (!clientTHUNE.connected()) {
    reconnect();
  }

  clientTHUNE.loop();  // La méthode client.loop() est appelée pour traiter les messages MQTT entrants.
    // Maintient la connexion avec le serveur MQTT en vérifiant si de nouveaux messages sont arrivés et en envoyant les messages en attente.

  //La dernière partie vérifie le temps écoulé depuis le dernier message publié et n'envoie le prochain message que toutes les 2 secondes (2000 millisecondes).
  long now = millis();        // Crée une variable "now" pour stocker le nombre de millisecondes écoulées depuis le démarrage du programme.
  if (now - lastMsg > 500) {  // Vérifie si le temps écoulé depuis le dernier message publié est supérieur à 2000 millisecondes.
    lastMsg = now;            // Si oui, met à jour la variable "lastMsg" avec la valeur actuelle de "now".

    int ledRedState = digitalRead(ledRed);

    if (ledRedState != lastLedState) {
      if (ledRedState == LOW) {
        clientTHUNE.publish("esp32/maison/ledRedDisplay", "off");
        Serial.println("Led OFF");
      } else {
        clientTHUNE.publish("esp32/maison/ledRedDisplay", "on");
        Serial.println("Led ON");
      }
      lastLedState = ledRedState;
    }
  }
}

//Pour envoyer les données dans la base SQL

void sendData(int identifiant) {

  if (identifiant == 1) {
    Serial.println("Données envoyés OPEN");
    clientTHUNE.publish("esp32/maison/ouverture", "1");
  } else if (identifiant == 2) {
    Serial.println("Données envoyés OPEN");
    clientTHUNE.publish("esp32/maison/ouverture", "1");
  } else if (identifiant == 0) {
    clientTHUNE.publish("esp32/maison/erreur", "null");
    Serial.println("Données envoyés ERREUR");
  } else {
    Serial.println("Problème dans l'identifiant entré");
  }
}


// Fréquences des notes en Hz
#define NOTE_B3 247
#define NOTE_B4 494
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define NOTE_B4 494

// Durées des notes (en ms)
#define QUARTER 400
#define EIGHTH 200
#define HALF 800


void playJingleBells() {
  // Tableau contenant les fréquences des notes
  int melody[] = {
    NOTE_E4, NOTE_E4, NOTE_E4,                    // "Jingle Bells"
    NOTE_E4, NOTE_E4, NOTE_E4,                    // "Jingle Bells"
    NOTE_E4, NOTE_G4, NOTE_C4, NOTE_D4, NOTE_E4,  // "Oh what fun it is to ride"
    NOTE_F4, NOTE_F4, NOTE_F4, NOTE_F4,           // "In a one-horse open sleigh"
    NOTE_F4, NOTE_E4, NOTE_E4, NOTE_E4, NOTE_E4,
    NOTE_E4, NOTE_D4, NOTE_D4, NOTE_E4, NOTE_D4, NOTE_G4  // "Hey!"
  };

  // Tableau contenant la durée de chaque note
  int durations[] = {
    QUARTER, QUARTER, HALF,                    // "Jingle Bells"
    QUARTER, QUARTER, HALF,                    // "Jingle Bells"
    QUARTER, QUARTER, QUARTER, QUARTER, HALF,  // "Oh what fun it is to ride"
    QUARTER, QUARTER, QUARTER, QUARTER,        // "In a one-horse open sleigh"
    QUARTER, QUARTER, QUARTER, QUARTER, QUARTER,
    QUARTER, QUARTER, QUARTER, QUARTER, HALF
  };

  // Joue la mélodie
  for (int i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
    int noteDuration = durations[i];
    tone(buzzer, melody[i], noteDuration);
    delay(noteDuration * 1.3);  // Ajout d'une pause entre les notes
    noTone(buzzer);             // Arrête le son avant la prochaine note
  }
}


void playBackInBlack() {
  // Tableau contenant les fréquences des notes
  int melody[] = {
    NOTE_E4, NOTE_E4, NOTE_G4, NOTE_A4, NOTE_E4,  // "Back in Black" riff part 1
    NOTE_E4, NOTE_E4, NOTE_B4, NOTE_D4, NOTE_E4,  // riff part 2
    NOTE_E4, NOTE_G4, NOTE_A4                     // riff repeat
  };

  // Tableau contenant la durée de chaque note
  int durations[] = {
    EIGHTH, EIGHTH, QUARTER, QUARTER, HALF,  // Durées du riff part 1
    EIGHTH, EIGHTH, QUARTER, QUARTER, HALF,  // riff part 2
    QUARTER, QUARTER, HALF                   // riff repeat
  };

  // Joue la mélodie
  for (int i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
    int noteDuration = durations[i];
    tone(buzzer, melody[i], noteDuration);
    delay(noteDuration * 1.3);  // Ajout d'une pause entre les notes
    noTone(buzzer);             // Arrête le son avant la prochaine note
  }
}
