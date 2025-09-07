#include <Arduino.h>
#include "HX711.h"
#include <TFT_eSPI.h>
#include <SPI.h>

// -------------------- HX711 Setup --------------------
#define HX711_DT D2  // GPIO4
#define HX711_SCK D1 // GPIO5

HX711 scale;

// Kalibrierfaktor (anpassen!)
float calibration_factor = -0.6428571428571429;

// -------------------- Display Setup --------------------
TFT_eSPI tft = TFT_eSPI();  // Pins aus User_Setup.h

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  delay(500);

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
  float gewicht = scale.get_units(5); // Mittelwert über 5 Messungen

  // Serial Ausgabe
  Serial.printf("Gewicht: %.1f g\n", gewicht);

  // TFT Ausgabe
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 40, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.println("Gewicht:");

  tft.setCursor(10, 80, 4);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.printf("%.1f g", gewicht);

  delay(500);
}
