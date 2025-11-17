#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <EEPROM.h>
#include <HX711.h>

// #define DEBUG // Sammelt mehr Werte und macht die serielle Ausgabe gesprächiger


// --- Pin-Definitionen  ---

// DISPLAY + TOUCH
//#define TOUCH_IRQ   -    // IRQ Pin für Touchscreen, hier unbenutzt
//#define TOUCH_D0    D6   // (MISO)
//#define TOUCH_DIN   D7   // (MOSI)
  #define TOUCH_CS    D0   
//#define TOUCH_CLK   D5   // (SCK) 

//#define MISO        D6    // GPIO12 // TFT_MISO
//#define LED         3V
//#define SCK         D5    // GPIO14 // TFT_SCLK 
//#define MOSI        D7    // GPIO13 // SDI MOSI
  #define TFT_DC      D3    
  #define TFT_RST     D4    
  #define TFT_CS      D8    
//#define GND         G
//#define VCC         3V

// Wägezellencontroller HX711
//#define VCC         3V
  #define HX711_SCK   D1
  #define HX711_DT    D2 
//#define GND         G

// Datenstruktur für die Werte
struct CalibrationData {
  char    head           [5]  = "MRAb";
  int     tsMinX              = 384;
  int     tsMaxX              = 3503;
  int     tsMinY              = 544;
  int     tsMaxY              = 3610;
  float   scaleCalibration    = -21; // -837.8887096774194; // -21;
  char    foot           [5]  = "MRAe";
};
CalibrationData calib;

// -- Waage --------------------

HX711 scale;
boolean scalePresent = false;

// -- TFT + Touch Objekte --------------------
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS);

// --- Speicher --------------------
#define EEPROM_SIZE 64
#define EEPROM_ADDR 0

// --- GUI Struktur --------------------
typedef void (*CallbackFn)();

struct GuiBox {
  int x, y, w, h;
  const char *label;
  uint16_t color;
  CallbackFn onTouch;
};

// --- Forward-Deklarationen --------------------
void calibrateTouch();
void drawBox(const GuiBox &box);
void drawGui();
void handleTouch(int tx, int ty);

// Forward-Deklarationen der Callback-Funktionen
void onBoxTara();


// --- Zentrales Array mit allen Boxen ---
GuiBox guiBoxes[] = {
// x,   y,   w,   h,  label,   color,        onTouch
  {210,  200, 60, 30, "Tara", ILI9341_GREEN,  onBoxTara},


};
const int NUM_BOXES = sizeof(guiBoxes) / sizeof(guiBoxes[0]);

// -- GUI Funktionen ---
void drawBox(const GuiBox &box) {
  tft.drawRect(box.x, box.y, box.w, box.h, box.color);
  tft.setCursor(box.x + 5, box.y + box.h / 2 - 8);
  tft.setTextColor(box.color);
  tft.setTextSize(2);
  tft.print(box.label);
}

void drawGui() {
  for (int i = 0; i < NUM_BOXES; i++) {
    drawBox(guiBoxes[i]);
  }
}

void handleTouch(int tx, int ty) {
  for (int i = 0; i < NUM_BOXES; i++) {
    GuiBox &b = guiBoxes[i];
    if (tx >= b.x && tx <= b.x + b.w &&
        ty >= b.y && ty <= b.y + b.h) {
      if (b.onTouch) b.onTouch();
    }
  }
}

// --- GUI Callback-Funktionen ---
void onBoxTara() {
  scale.tare();
  tft.setCursor(10, 210);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.println("Tariert");
  delay(1000);
  tft.setCursor(10, 210);
  tft.println("            ");
}


// -- Kalibrierungsfunktion ---
void calibrateTouch() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(2);

  // Hilfsfunktion: Punkt zeichnen und warten
  auto measurePoint = [&](int px, int py, int &rx, int &ry) {
    Serial.print("Punkt an ");
    Serial.print(px);
    Serial.print("x");
    Serial.println(py); 
    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(10, 10);
    tft.println("Beruehre den roten Punkt");
    tft.drawCircle(px, py, 5, ILI9341_RED);
    // warten bis Touch gedrückt
    while (!ts.touched()) delay(10);
    delay(200); // Entprellen
    TS_Point p = ts.getPoint();
    rx = p.x;
    ry = p.y;
    // warten bis losgelassen
    while (ts.touched()) delay(10);
    delay(200);
  };

  int rx1, ry1, rx2, ry2, rx3, ry3, rx4, ry4;

  // linke obere Ecke
  measurePoint(20, 20, rx1, ry1);
  // rechte obere Ecke
  measurePoint(tft.width() - 20, 20, rx2, ry2);
  // linke untere Ecke
  measurePoint(20, tft.height() - 20, rx3, ry3);
  // rechte untere Ecke
  measurePoint(tft.width() - 20, tft.height() - 20, rx4, ry4);

  // Minimal/Maximal ermitteln
  calib.tsMinX = min(min(rx1, rx3), min(rx2, rx4));
  calib.tsMaxX = max(max(rx1, rx3), max(rx2, rx4));
  calib.tsMinY = min(min(ry1, ry2), min(ry3, ry4));
  calib.tsMaxY = max(max(ry1, ry2), max(ry3, ry4));
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(10, 10);
  tft.println("Kalibrierung fertig");

  // Ausgabe der Werte im Seriellen Monitor
  Serial.printf("TS_MINX=%d TS_MAXX=%d TS_MINY=%d TS_MAXY=%d\n",
                calib.tsMinX, calib.tsMaxX, calib.tsMinY, calib.tsMaxY);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println();
  Serial.println("DoggoScale!");

  // TFT starten
  tft.begin();
  Serial.print("Display initialisiert: ");
  Serial.print(tft.width());
  Serial.print("x");  
  Serial.println(tft.height()); 
  tft.setRotation(1); // Querformat
  tft.fillScreen(ILI9341_BLACK);

  // Touch initialisieren
  if (ts.begin()) {
    Serial.println("XPT2046 bereit");
    ts.setRotation(3);
  } else {
    Serial.println("XPT2046 nicht gefunden!");
  }


  // HX711 initialisieren
  tft.setCursor(0, 50);
  tft.println("Warte auf Waage...");
  Serial.println("Initialisiere Waage");
  scale.begin(HX711_DT, HX711_SCK);

  while (!scale.is_ready()) {
    Serial.println("Warte auf Waage...");
    delay(100);
  }

  scale.set_scale(calib.scaleCalibration);
  scale.tare();
  scalePresent = true;
  Serial.println("HX711 bereit, Nullpunkt gesetzt.");
  tft.println("Waage bereit...       ");
  delay(1000);
  tft.fillScreen(ILI9341_BLACK);

  drawGui();
}

void loop() {
  TS_Point p;
  if (ts.touched()) {
    p = ts.getPoint();

    // Rohwerte -> Pixel
    int x = map(p.x, calib.tsMinX, calib.tsMaxX, 0, tft.width());
    int y = map(p.y, calib.tsMinY, calib.tsMaxY, 0, tft.height());
    x = constrain(x, 0, tft.width() - 1);
    y = constrain(y, 0, tft.height() - 1);

    Serial.printf("Touch: raw=(%d,%d) -> pixel=(%d,%d)\n", p.x, p.y, x, y);
    handleTouch(x, y);
    delay(200);
  }

  if (scalePresent) {
    float gewicht = scale.get_units(3) / 1000; // Mittelwert über 3 Messungen
    #ifdef DEBUG
      float raw = scale.read_average(5);
      float value = scale.get_value(5);
      Serial.printf("Raw: %.2f \n", raw);
      Serial.printf("Value: %.1f \n", value);
    #endif

    // Serial Ausgabe
    Serial.printf("Gewicht: %.1f kg\n", gewicht);
    Serial.println(" ");

    // TFT Ausgabe
    tft.setCursor(0, 40);
    tft.setTextSize(5);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    //tft.fillRect(130, 130, 160, 160, ILI9341_BLACK); //TODO: Letzte Pixel wegkratzen
    tft.printf("%6.2f kg", gewicht);
  }  

}
