#include <Arduino.h>
#include "HX711.h"
#include <TFT_eSPI.h>
#include <SPI.h>

// -------------------- HX711 Setup --------------------
#define HX711_DT D2  // GPIO4
#define HX711_SCK D1 // GPIO5

HX711 scale;

// Button
#define TARE_BUTTON D0
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 300; // ms
boolean       buttonState     = false;

// Kalibrierfaktor (anpassen!)
float calibration_factor = -21; // -837.8887096774194; // -21;

// -------------------- Display Setup --------------------
TFT_eSPI tft = TFT_eSPI();  // Pins aus User_Setup.h

boolean getTareButtonState();

boolean getTareButtonState() {
  boolean newState;
  newState = digitalRead(TARE_BUTTON);
  if (!newState == buttonState) {
    buttonState = newState;
    if (buttonState == 1) {
      scale.tare();
      Serial.println("Tariere");
      tft.setCursor(10, 10, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.println("Tariert");
      delay(600);
      tft.setCursor(10, 10, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.println("        ");
    }
    return true;
  } else {
    return false;
  }
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  // Button
  pinMode(TARE_BUTTON, INPUT);

  // HX711 initialisieren
  scale.begin(HX711_DT, HX711_SCK);
  scale.set_scale(calibration_factor);
  scale.tare();

  Serial.println("HX711 bereit, Nullpunkt gesetzt.");

  // Display initialisieren
  tft.init();
  tft.setRotation(2);  // ggf. anpassen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Waage bereit...");
  delay(1500);
  tft.fillScreen(TFT_BLACK);
}

// -------------------- Loop --------------------
void loop() {
  float gewicht = scale.get_units(5) / 1000; // Mittelwert über 5 Messungen
  float raw = scale.read_average(5);
  float value = scale.get_value(5);

  // Serial Ausgabe
  Serial.printf("Gewicht: %.1f kg\n", gewicht);
  Serial.printf("Raw: %.2f \n", raw);
  Serial.printf("Value: %.1f \n", value);
  Serial.println(" ");

  // TFT Ausgabe
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 40, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Gewicht:");

  tft.setCursor(10, 80, 2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.printf("%.2f kg", gewicht);

  // --- Button check ---
  getTareButtonState(); 

}
