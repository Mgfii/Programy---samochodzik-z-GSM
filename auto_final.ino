#include <NewPing.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "DHT.h"
#include <HardwareSerial.h>

#define MODEM_RX 26
#define MODEM_TX 27

HardwareSerial gsmSerial(2); // UART2 dla SIM800L
String numerDocelowy = "+48698273847";

// dane wifi
const char *ssid = "auto";
const char *password = "auto2025";
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);  // Port WebSocket

#define moc_skret 22
#define prawo 16
#define lewo 17
#define moc_naped 23
#define przod 18
#define tyl 19
#define PIN_BUZER     25
#define PIN_TRIG2      32
#define PIN_ECHO2      33
#define PIN_TRIG1      13
#define PIN_ECHO1      12
#define MAX_DYSTANS   100
#define PROG_DYSTANS  50
#define OKRES_POMIARU 200
#define CZAS_PISKU    50
#define swiatla_tyl 21
#define swiatla_przod 5
#define DHTPIN 14       // GPIO, do którego podłączony jest czujnik
#define DHTTYPE DHT11

NewPing sonar1(PIN_TRIG1, PIN_ECHO1, MAX_DYSTANS);
unsigned int dystans1 = 0;
NewPing sonar2(PIN_TRIG2, PIN_ECHO2, MAX_DYSTANS);
unsigned int dystans2 = 0;
unsigned long czasOstatniegoPomiaru = 0;
bool isOn = false;
unsigned long lastBeepTime = 0;
unsigned long lastSentTime = 0;
const unsigned long SEND_INTERVAL = 1000;
String lastDirection = "STOP";  // Zmienna globalna przechowująca ostatni kierunek jazdy
bool swiatlaWlaczone = false;
DHT dht(DHTPIN, DHTTYPE);
unsigned long lastTempSent = 0;
bool smsSent = false;

void setup() {
  // konfiguracja punktu dostępowego Wi-Fi
  Serial.begin(115200);
  Serial.println("Start programu...");
  WiFi.softAP(ssid, password);
  Serial.println("Konfiguracja WiFi...");
  Serial.println(WiFi.softAPIP()); // Adres IP ESP32 w trybie AP
  
  // Uruchamiamy serwer HTTP
  server.on("/command", handleCommand);
  server.begin();

  // Uruchamiamy serwer WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  dht.begin();

  gsmSerial.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  gsmSerial.println("AT");
  delay(1000);
  gsmSerial.println("AT+CMGF=1"); // Tryb tekstowy
  delay(1000);

  // Ustawienie pinów jako wyjścia
  pinMode(moc_skret, OUTPUT);
  pinMode(prawo, OUTPUT);
  pinMode(lewo, OUTPUT);
  pinMode(moc_naped, OUTPUT);
  pinMode(przod, OUTPUT);
  pinMode(tyl, OUTPUT);
  pinMode(PIN_BUZER, OUTPUT);
  pinMode(swiatla_tyl, OUTPUT);
  pinMode(swiatla_przod, OUTPUT);
  digitalWrite(swiatla_tyl, LOW);
  digitalWrite(swiatla_przod, LOW);

  // Ustawienie mocy silników
  analogWrite(moc_skret, 255);
  analogWrite(moc_naped, 190);

  digitalWrite(PIN_BUZER, LOW);
}

void handleCommand() {
  String cmd = server.arg("cmd"); // Pobiera komendę z URL
  server.send(200, "text/plain", "OK");

  if (cmd == "FORWARD") {
    digitalWrite(przod, HIGH);
    digitalWrite(tyl, LOW);
    
  } else if (cmd == "BACKWARD") {
    digitalWrite(przod, LOW);
    digitalWrite(tyl, HIGH);
  } else if (cmd == "LEFT") {
    digitalWrite(prawo, LOW);
    digitalWrite(lewo, HIGH);
  } else if (cmd == "RIGHT") {
    digitalWrite(prawo, HIGH);
    digitalWrite(lewo, LOW);
  } else if (cmd == "STOP") {
    digitalWrite(przod, LOW);
    digitalWrite(tyl, LOW);
    digitalWrite(prawo, LOW);
    digitalWrite(lewo, LOW);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = String((char *)payload);
    Serial.printf("Otrzymano wiadomość: %s\n", message.c_str());

    if (message == "FORWARD") {
      digitalWrite(przod, HIGH);
      digitalWrite(tyl, LOW);
      while (dystans1 > 0 && dystans1 < PROG_DYSTANS) {
      dystans1 = sonar1.ping();
      dystans1 = sonar1.convert_cm(dystans1);
      if (dystans1 <= PROG_DYSTANS) {
      // Natychmiastowe zatrzymanie pojazdu, jeśli wykryta przeszkoda
        digitalWrite(przod, LOW);
        digitalWrite(tyl, LOW);
        webSocket.broadcastTXT("STOP");
      break;  // Zatrzymaj pętlę i pojazd
        } 
      }
      lastDirection = "FORWARD";
    }
    else if (message == "BACKWARD") {
      digitalWrite(przod, LOW);
      digitalWrite(tyl, HIGH);
      while (dystans2 > 0 && dystans2 < PROG_DYSTANS) {
      dystans2 = sonar2.ping();
      dystans2 = sonar2.convert_cm(dystans1);
      if (dystans2 <= PROG_DYSTANS) {
      // Natychmiastowe zatrzymanie pojazdu, jeśli wykryta przeszkoda
        digitalWrite(przod, LOW);
        digitalWrite(tyl, LOW);
        webSocket.broadcastTXT("STOP");
      break;  // Zatrzymaj pętlę i pojazd
        }
      }
      lastDirection = "BACKWARD";
    } 
    else if (message == "LEFT") {
      digitalWrite(lewo, HIGH);
      digitalWrite(prawo, LOW);
    } 
    else if (message == "RIGHT") {
      digitalWrite(prawo, HIGH);
      digitalWrite(lewo, LOW);
    } 
    else if (message == "LEFT_RELEASE" || message == "RIGHT_RELEASE") {
      digitalWrite(lewo, LOW);
      digitalWrite(prawo, LOW);

      if (lastDirection == "FORWARD") {
        digitalWrite(przod, HIGH);
        digitalWrite(tyl, LOW);
      } else if (lastDirection == "BACKWARD") {
        digitalWrite(przod, LOW);
        digitalWrite(tyl, HIGH);
      }
    } 
    else if (message == "STOP") {
      digitalWrite(przod, LOW);
      digitalWrite(tyl, LOW);
      digitalWrite(lewo, LOW);
      digitalWrite(prawo, LOW);
      lastDirection = "STOP";
    }
    else if (message == "TOGGLE_LIGHTS") {
      swiatlaWlaczone = !swiatlaWlaczone;
      digitalWrite(swiatla_przod, swiatlaWlaczone ? HIGH : LOW);
      digitalWrite(swiatla_tyl, swiatlaWlaczone ? HIGH : LOW);
  }
  }
}

void sendSMS(String phoneNumber, String message) {
  gsmSerial.println("AT+CMGF=1"); // Tryb tekstowy
  delay(1000);

  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");
  delay(1000);

  gsmSerial.println(message);
  delay(1000);

  gsmSerial.write(26); // Ctrl+Z = koniec wiadomości
  delay(3000);

  Serial.println("SMS wysłany!");
}


void buzer_pisk(int dystans1, int dystans2) {
  // Ignoruj wartości 0 jako błędne odczyty
  bool valid1 = dystans1 > 1;
  bool valid2 = dystans2 > 1;

  int minDystans;

  if (valid1 && valid2) {
    minDystans = min(dystans1, dystans2);
  } else if (valid1) {
    minDystans = dystans1;
  } else if (valid2) {
    minDystans = dystans2;
  } else {
    // Brak poprawnych pomiarów – wyłącz buzzer
    digitalWrite(PIN_BUZER, LOW);
    isOn = false;
    return;
  }

  // Mapowanie dystansu na ilość pisków na sekundę
  int piskiNaSek = map(minDystans, 0, PROG_DYSTANS, 8, 0);
  piskiNaSek = constrain(piskiNaSek, 0, 8);

  if (piskiNaSek == 0) {
    digitalWrite(PIN_BUZER, LOW);
    isOn = false;
    return;
  }

  int beepInterval = 1000 / piskiNaSek;
  if (millis() - lastBeepTime >= beepInterval) {
    lastBeepTime = millis();
    isOn = !isOn;
    digitalWrite(PIN_BUZER, isOn ? HIGH : LOW);
  }
}


void loop() {
  // Obsługuje połączenia HTTP
  server.handleClient();
  
  // Obsługuje połączenia WebSocket
  webSocket.loop();

  // Pomiar dystans1u
  if (millis() - czasOstatniegoPomiaru >= OKRES_POMIARU) {
    dystans1 = sonar1.ping();
    dystans1 = sonar1.convert_cm(dystans1);
    dystans2 = sonar2.ping();
    dystans2 = sonar2.convert_cm(dystans2);
    czasOstatniegoPomiaru = millis();
  }
  if (millis() - lastSentTime >= SEND_INTERVAL) {
    String json = "{\"front\":" + String(dystans1) + ",\"back\":" + String(dystans2) + "}";
    webSocket.broadcastTXT(json);
    lastSentTime = millis();
  }

unsigned long currentMillis = millis();
if (currentMillis - lastTempSent >= 1000) {
  lastTempSent = currentMillis;

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temperature)) {
  // Wysyłka SMS przy przekroczeniu 30°C
  if (temperature > 30.0 && !smsSent) {
    sendSMS(numerDocelowy, "Uwaga! Temperatura w aucie RC przekroczyla 30 stopni Celsjusza.");
    smsSent = true;
  } else if (temperature <= 30.0) {
    smsSent = false; // reset flagi gdy spadnie poniżej progu
  }
    Serial.print("Temperatura: ");
    Serial.print(temperature);
    Serial.println(" °C");

    String json = "{";
    json += "\"front\":" + String(dystans1) + ",";
    json += "\"back\":" + String(dystans2) + ",";
    json += "\"temperature\":" + String(temperature);
    json += "}";

    webSocket.broadcastTXT(json);  // Wyślij dane do aplikacji
  } else {
    Serial.println("Błąd odczytu temperatury z DHT11.");
  }
}

  // zmniejszenie mocy silnika po wykryciu przeszkody
  if((dystans1 > 0 && dystans1 < PROG_DYSTANS) || (dystans2 > 0 && dystans2 < PROG_DYSTANS))
  {
    analogWrite(moc_naped, 180);
  }
  else if ((dystans1 > 0 && dystans1 < 70) || (dystans2 > 0 && dystans2 < 70)){
    analogWrite(moc_naped, 190);
  }
  else{analogWrite(moc_naped, 200);}

  if (dystans1 <= 30 && dystans1 > 0) {
        digitalWrite(przod, LOW);
        if (dystans1 > 0 && dystans1 <= 15){
        digitalWrite(tyl, HIGH);
        delay(200);
        digitalWrite(tyl, LOW);}
  }
  if (dystans2 <= 30  && dystans2 > 0) {
        digitalWrite(tyl, LOW);
        if (dystans2 > 0 && dystans2 <= 15){
        digitalWrite(przod, HIGH);
        delay(200);
        digitalWrite(przod, LOW);}
  }
    Serial.print("Dystans1: ");
    Serial.print(dystans1);
    Serial.print(" , Dystans2: ");
    Serial.println(dystans2);


  // Generowanie pisków na buzzerze
  buzer_pisk(dystans1, dystans2);

}