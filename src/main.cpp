#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Button
#define WECKER_BUTTON 26
#define ALARM_BUTTON 25

// LED Ring
#define LED_PIN 27
#define LED_COUNT 12 // Anzahl LEDs am Ring
Adafruit_NeoPixel ledRing(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// WLAN Zugangsdaten
const char *ssid = "iPhone von Thesi";
const char *password = "WlanFuerEsp32FuerUhrzeit";

// NTP Client (Uhrzeit)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000); // Zeitzone UTC+2 (Sommerzeit), Aktualisierung alle 60 Sekunden

// Rotary Encoder
#define EC_CLK 32
#define EC_DT 33
#define EC_SW 19

// Zustände
bool displayOn = false;
bool lastButtonState = HIGH;
bool lastCLKState = HIGH;
bool lastSWState = HIGH;
bool alarmActive = false;  // Wecker aktiv
bool settingAlarm = false; // Wecker Alarm Status
bool alarmSet = false;     // Wecker Alarm gesetzt
bool settingHour = true;   // true = Stunde einstellen, false = Minute einstellen
bool alarmStopped = false; // Wecker Alarm gestoppt

int alarmHour = 7;          // Wecker Stunde
int alarmMinute = 0;        // Wecker Minute
int lastCheckedMinute = -1; // Letzte Minute, die überprüft wurde

// Anzeige aktuelle Uhrzeit
void displayCurrentTime()
{
  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 20);

  String time = timeClient.getFormattedTime();
  String timeShort = time.substring(0, 5); // Nur hh:mm
  display.print(timeShort);
  display.display();
}

// Simulation Sonnenaufgang zum wecken
void sunriseAlarm()
{
  for (int brightness = 0; brightness <= 130; brightness += 3)
  {
    // Farbe für den LED Ring setzen
    uint8_t r = map(brightness, 0, 130, 0, 255);
    uint8_t g = map(brightness, 0, 130, 0, 110);
    uint8_t b = map(brightness, 0, 130, 0, 58);

    for (int i = 0; i < LED_COUNT; i++)
    {
      ledRing.setPixelColor(i, ledRing.Color(r, g, b));
    }

    ledRing.show();
    delay(50);
  }
}

// Lichtwecker Einschalten (alles)
void turnOnLichtwecker()
{
  display.ssd1306_command(SSD1306_DISPLAYON); // Display an
  displayCurrentTime();                       // Aktuelle Uhrzeit anzeigen
  displayOn = true;
}

// Lichtwecker ausschalten (alles)
void turnOffLichtwecker()
{
  display.ssd1306_command(SSD1306_DISPLAYOFF); // Display aus
  display.clearDisplay();                      
  display.display();                           
  displayOn = false;
}

// Anzeigen Alarmzeit
void displayAlarmSettings(int hour, int minute, int blinken)
{
  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 20);

  if (blinken && settingHour)
  {
    display.print("  :");
  }
  else
  {
    if (hour < 10)
    {
      display.print("0");
    }
    display.print(hour);
    display.print(":");
  }
  if (blinken && !settingHour)
  {
    display.print("  ");
  }
  else
  {
    if (minute < 10)
    {
      display.print("0");
    }
    display.print(minute);
  }
  display.display();
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Wecker Button initialisieren
  pinMode(WECKER_BUTTON, INPUT_PULLUP);

  // Alarm Button initialisieren
  pinMode(ALARM_BUTTON, INPUT_PULLUP);

  // Rotary Encoder initialisieren
  pinMode(EC_CLK, INPUT_PULLUP);
  pinMode(EC_DT, INPUT_PULLUP);
  pinMode(EC_SW, INPUT_PULLUP);

  // Display initialisieren
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("Display nicht gefunden"));
    while (true)
      ; // Loop nicht für immer
  }
  display.ssd1306_command(SSD1306_DISPLAYOFF); // Display aus

  // LED Ring initialisieren
  ledRing.begin();
  ledRing.show(); // Alle LEDs ausschalten

  // WLAN Verbindung herstellen
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WLAN verbunden");

  // NTP Client initialisieren
  timeClient.begin();
}

void loop()
{
  // NTP Client aktualisieren
  timeClient.update();

  // Rotary Encoder
  bool currentCLKState = digitalRead(EC_CLK);
  bool currentSWState = digitalRead(EC_SW);

  static unsigned long lastBlinken = 0;
  static bool blinken = false;

  // Wecker Einstellung
  if (settingAlarm)
  {
    if (currentCLKState != lastCLKState && currentCLKState == LOW)
    {
      int direction = (digitalRead(EC_DT) != currentCLKState ? 1 : -1);
      if (settingHour)
      {
        alarmHour = (alarmHour + direction + 24) % 24; // Stunden anpassen
      }
      else
      {
        alarmMinute = (alarmMinute + direction + 60) % 60; // Minuten
      }
      Serial.print("Alarmzeit: ");
      Serial.print(alarmHour);
      Serial.print(":");
      Serial.println(alarmMinute);
    }
    lastCLKState = currentCLKState;

    if (millis() - lastBlinken > 500)
    {
      blinken = !blinken;
      lastBlinken = millis();
      displayAlarmSettings(alarmHour, alarmMinute, blinken);
    }
  }

  // Rotary Encoder Button
  if (lastSWState == HIGH && currentSWState == LOW)
  {
    if (!settingAlarm)
    {
      settingAlarm = true;
      settingHour = true; // Start mit Stunden einstellen

      Serial.println("Wecker Einstellung gestartet");
    }
    else if (settingHour)
    {
      settingHour = false; // Wechsel zu Minuten einstellen

      Serial.println("Minuten einstellen");
    }
    else
    {
      settingAlarm = false; // Wecker Einstellung beenden
      alarmSet = true;      
      displayOn = true;
      displayCurrentTime(); 

      Serial.print("Weckerzeit: ");
      Serial.print(alarmHour);
      Serial.print(":");
      Serial.println(alarmMinute);
    }
  }
  lastSWState = currentSWState;

  // Wecker Button Status abfragen
  bool currentButtonState = digitalRead(WECKER_BUTTON);

  if (lastButtonState == HIGH && currentButtonState == LOW)
  {
    displayOn = !displayOn;
    if (displayOn)
    {
      turnOnLichtwecker(); // Display und Uhrzeit anzeigen
    }
    else
    {
      turnOffLichtwecker(); // Display und Uhrzeit ausschalten
    }
    delay(500); // Entprellen
  }
  lastButtonState = currentButtonState;

  // aktuelle Uhrzeit mit Weckzeit vergleichen
  if (alarmSet && !settingAlarm)
  {
    String now = timeClient.getFormattedTime();
    int currentHour = now.substring(0, 2).toInt();
    int currentMinute = now.substring(3, 5).toInt();

    if (currentMinute != lastCheckedMinute)
    {
      alarmStopped = false;              // Reset Alarm gestoppt
      lastCheckedMinute = currentMinute; // Letzte Minute aktualisieren
    }

    if (currentHour == alarmHour && currentMinute == alarmMinute && !alarmActive && !alarmStopped)
    {
      // Alarm auslösen
      Serial.println("Wecker Alarm!");
      alarmActive = true;
      sunriseAlarm();
    }
  }

  // Alarm Stop Button
  if (alarmActive && digitalRead(ALARM_BUTTON) == LOW)
  {
    Serial.println("Wecker Alarm gestoppt");
    alarmActive = false;
    alarmStopped = true; // Alarm gestoppt

    if (displayOn)
    {
      displayCurrentTime();
    }
    // LEDs ausschalten
    for (int i = 0; i < LED_COUNT; i++)
    {
      ledRing.setPixelColor(i, 0);
    }
  }
  ledRing.show();

  // immer aktuelle Uhrzeit anzeigen, auch bei Wecker
  if(displayOn && !settingAlarm)
  {
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 1000) // jede Sekunde aktualisieren
    {
      lastDisplayUpdate = millis();
      displayCurrentTime();
    }
  }
}
