#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <XPT2046_Touchscreen.h>
#include <HX711.h>

#define DEBUG   // Sammelt mehr Werte und macht die serielle Ausgabe gesprächiger
#define DRYRUN  // Simuliert ein eingeschlossenes HX711 ohne dass es angeschlossen ist 


// --- Pin-Definitionen  ---

//**  DISPLAY + TOUCH
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

//**  Wägezellencontroller HX711
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
HX711           scale;
boolean         scalePresent      = false;
unsigned short  scaleTimeout      = 2000; // Millisek.
unsigned short  samplesPerReading = 1;
unsigned long   lastTareCheck     = 0;
unsigned short  tareCheckInterval = 5000; // Millisek.
float           tareThreshold     = 5.0; // Kg, schwankt das Gewicht innerhalb dieses Bereichs, wird automatisch tariert
float           tare              = 0.0;
float           untarredWeight    = 0.0;
float           lastWeight        = 0.0;
float           currentWeight     = 0.0;

#ifdef DEBUG
double          value             = 0;
long            raw               = 0;
#endif


// -- TFT + Touch Objekte --------------------
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas1 canvas(330, 50);
XPT2046_Touchscreen ts(TOUCH_CS);

// --- GUI Struktur --------------------
typedef void (*CallbackFn)();

struct GuiBox {
  int x, y, w, h;
  const char *label;
  uint16_t color;
  CallbackFn onTouch;
};

// Forward-Deklarationen der Callback-Funktionen
void onBoxTara();


// --- Zentrales Array mit allen Boxen ---
GuiBox guiBoxes[] = {
//   x,   y,   w,   h,  label,         color,    onTouch
  {190,  170, 120, 60, "Tara", ILI9341_GREEN,  onBoxTara},


};
const int NUM_BOXES = sizeof(guiBoxes) / sizeof(guiBoxes[0]);

// --- Forward-Deklarationen --------------------
void calibrateTouch();
void drawBox(const GuiBox &box);
void drawGui();
void handleTouch(int tx, int ty);
void doTare();
void manualTare();
void getCurrentWeight();
void outputCurrentWeight();
void checkTouch();
void checkAutoTare();


// --- Funktionen --------------------

void drawBox(const GuiBox &box) {
  tft.drawRect(box.x, box.y, box.w, box.h, box.color);
  tft.setCursor(box.x + 5, box.y + box.h / 2);
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

void manualTare() {
  doTare();
  tft.setCursor(10, 210);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setFont(&FreeSans9pt7b);
  tft.println("Tariert");
  delay(1000);
  tft.setCursor(10, 210);
  tft.println("       ");
}

void doTare() {
  #ifdef DEBUG
    Serial.println("doTare() BEGIN");
  #endif
  lastWeight = currentWeight;
  tare = untarredWeight;
  Serial.print("Waage tariert: ");
  Serial.println(tare);
  lastTareCheck = millis();
  #ifdef DEBUG
    Serial.println("doTare() END");
  #endif
}

void onBoxTara() {
  manualTare();
}

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


void checkAutoTare() {
  #ifdef DEBUG
    Serial.println("checkAutoTare() BEGIN");
  #endif
  float difference = 0;

  // Wenn seit der letzten Prüfung genug Zeit vergangen ist
  if (millis() > (lastTareCheck + tareCheckInterval)) {
    // Wenn der aktuelle Messwert negativ ist
    difference = currentWeight - lastWeight;
    if (difference < 0) {
      // ins Positive drehen
      difference = -difference;
    } 

    Serial.print("Auto-Tare Check: ");
    Serial.print("currentWeight: ");
    Serial.print(currentWeight);
    Serial.print(" - lastWeight: ");
    Serial.print(lastWeight);
    // wenn Gewicht stabil ist
    Serial.print(" -> difference: ");
    Serial.print(difference);
    if (difference < tareThreshold) { 
      Serial.print(" < tareThreshold: ");
      Serial.print(tareThreshold);
      Serial.println(" => Automatisch tariert.");
      doTare();
    } else {
      Serial.println(" >= tareThreshold: kein Tare.");
    }
  }
  #ifdef DEBUG
    Serial.println("checkAutoTare() END");
  #endif
}

void getCurrentWeight() {
  #ifdef DEBUG
    Serial.println("getCurrentWeight() BEGIN");
  #endif

  #ifdef DRYRUN
    currentWeight = random(-3000, 3000) / 100.0;
    Serial.printf("DRYRUN Gewicht: %.2f kg\n", currentWeight);
    delay(1000);
  #else
    if (scalePresent) {
      // Gib der Waage einen Timeout mit, nachdem sie aufgeben soll
      if (scale.wait_ready_timeout(scaleTimeout)) {
        untarredWeight = scale.get_units(samplesPerReading) / 1000; // Mittelwert über [samplesPerReading] Messungen
        currentWeight = untarredWeight - tare;
        // Serial Ausgabe
        Serial.printf("Gewicht: %.2f kg\n", currentWeight);
    
      checkAutoTare();
      
      #ifdef DEBUG
        Serial.println("scale.read_average");
        raw = scale.read_average(samplesPerReading);
        Serial.printf("Raw: %ld \n", raw);

        Serial.println("scale.get_value");
        value = scale.get_value(samplesPerReading);
        Serial.printf("Value: %.5f \n", value);

        tft.setTextSize(1);
        tft.setCursor(0, 100);
        Serial.println("TFT Ausgabe Debug Werte");
        tft.printf("untarredWeight: %.5f   \n", untarredWeight);
        tft.printf("currentWeight: %.5f   \n",  currentWeight);
        tft.printf("lastWeight: %.5f   \n",     lastWeight); 
        tft.printf("tare: %.5f   \n",           tare); 
      #endif    
    } else {
      currentWeight = 0.0;
      Serial.println("scalePresent == false");
    }    
  } else {
    Serial.println("HX711 nicht mehr gefunden");
    scalePresent = false;
    currentWeight = 0.0;
  }
  #endif

  #ifdef DEBUG
    Serial.println("getCurrentWeight() END");
  #endif  
}

void outputCurrentWeight() {
  #ifdef DEBUG
    Serial.println("outputCurrentWeight() BEGIN");
  #endif
    // TFT Ausgabe
    canvas.fillScreen(ILI9341_BLACK);
    canvas.setCursor(0, 38); // Keine Ahnung warum, aber wenn man Font verwendet, muss Y höher gesetzt werden
    canvas.setFont(&FreeSans24pt7b);
    //canvas.setTextSize(5);
    canvas.printf("%6.2f kg", currentWeight);
    tft.drawBitmap(0, 65, canvas.getBuffer(), 330, 50, ILI9341_WHITE, ILI9341_BLACK); // Copy to screen
  #ifdef DEBUG
    Serial.println("outputCurrentWeight() END");
  #endif
}

void checkTouch() {
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
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println();
  Serial.println("DoggoScale!");

  // TFT starten
  tft.begin();
  Serial.print("Display initialisiert: ");
  tft.setFont(&FreeSans9pt7b);
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
  #ifdef DRYRUN
    Serial.println("DRYRUN Modus: Simuliere HX711 Waage");
  #else
    tft.setCursor(0, 50);
    tft.println("Warte auf Waage...");
    Serial.println("Initialisiere Waage");
    scale.begin(HX711_DT, HX711_SCK);

    while (!scale.is_ready()) {
      Serial.println("Warte auf Waage...");
      delay(100);
    }

    scalePresent = true;
    scale.set_scale(calib.scaleCalibration);
    scale.tare();
    Serial.println("HX711 bereit, Nullpunkt gesetzt.");
    tft.println("Waage bereit...       ");
  #endif
  delay(500);
  tft.fillScreen(ILI9341_BLACK);

  drawGui();
}

void loop() {
  checkTouch();
  getCurrentWeight();
  outputCurrentWeight();  
}
