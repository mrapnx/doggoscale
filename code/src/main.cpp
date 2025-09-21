#include <Arduino.h>
#include <map>
#include <HX711.h>
#include <TFT_eSPI.h>
#include <SPI.h>

// -------------------- HX711 Setup --------------------
#define HX711_DT D2  // GPIO4
#define HX711_SCK D1 // GPIO5

HX711 scale;
boolean scalePresent = false;

// Button
#define TARE_BUTTON D0
#define BUTTON_2    D3   // TODO: Wenn der ESP aufwacht und der Pin angeschlossen ist, klappt es nicht. Nur, wenn man den Pin nach dem Aufwache nanschließt :/
//#define BUTTON_3    D4
std::map<uint8_t, unsigned long> lastButtonPress;

const unsigned long debounceTime = 300; // ms
boolean       buttonState     = false;

// Kalibrierfaktor (anpassen!)
float calibration_factor = -21; // -837.8887096774194; // -21;

// -------------------- Display Setup --------------------
TFT_eSPI tft = TFT_eSPI();  // Pins aus User_Setup.h

boolean getTareButtonState();
boolean getButtonState(uint8_t buttonPin);

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

boolean getButtonState(uint8_t buttonPin) {
  boolean newState;
  newState = digitalRead(buttonPin);
  if (!newState == lastButtonPress[buttonPin]) {
    lastButtonPress[buttonPin] = newState;
    if (lastButtonPress[buttonPin] == 1) {
      Serial.print("Button ");
      Serial.print(buttonPin);
      Serial.println(" gedrückt");
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
  Serial.println(" ");
  Serial.println("Aufgewacht");

  // Button
  pinMode(TARE_BUTTON, INPUT);
  pinMode(BUTTON_2, INPUT);
//  pinMode(BUTTON_3, INPUT);

  // Display initialisieren
  tft.init();
  tft.setRotation(2);  // ggf. anpassen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  // HX711 initialisieren
  tft.setCursor(0, 0);
  tft.println("Warte auf Waage...");
  Serial.println("Initialisiere Waage");
  scale.begin(HX711_DT, HX711_SCK);

  while (!scale.is_ready()) {
    tft.setCursor(0, 0);
    Serial.println("Warte auf Waage...");
    delay(100);
  }

  scale.set_scale(calibration_factor);
  scale.tare();
  scalePresent = true;
  Serial.println("HX711 bereit, Nullpunkt gesetzt.");
  tft.setCursor(0, 0);
  tft.println("Waage bereit...       ");

}

// -------------------- Loop --------------------
void loop() {
  if (scalePresent) {
    float gewicht = scale.get_units(5) / 1000; // Mittelwert über 5 Messungen
    float raw = scale.read_average(5);
    float value = scale.get_value(5);

    // Serial Ausgabe
    Serial.printf("Gewicht: %.1f kg\n", gewicht);
    Serial.printf("Raw: %.2f \n", raw);
    Serial.printf("Value: %.1f \n", value);
    Serial.println(" ");

    // TFT Ausgabe
    tft.setTextSize(2);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(10, 40, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("Gewicht:");

    tft.setCursor(10, 80, 2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.printf("%.2f kg", gewicht);

    // --- Button check ---
    getTareButtonState(); 
    getButtonState(BUTTON_2);
 //   getButtonState(BUTTON_3);
  }
}
