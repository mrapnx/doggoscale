#include <Arduino.h>
#include <SPI.h>
#include <HX711.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <FreeSans48pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

//#define DEBUG   // Sammelt mehr Werte und macht die serielle Ausgabe gesprächiger
//#define DRYRUN  // Simuliert ein eingeschlossenes HX711 ohne dass es angeschlossen ist 

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

// Datenstruktur für Gewichtsmessungen
struct WeightSample {
  float         value;
  unsigned long ts;
};



// -- Speicher --------------------
// Speicheradresse im EEPROM
const int EEPROM_ADDR = 0;

// EEPROM-Größe festlegen (ESP8266 max 4096)
const int EEPROM_SIZE = 512;

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
float           newWeight         = 0.0;

double          value             = 0;
long            raw               = 0;

// --- Auto-Tare Parameter ---
unsigned long   tareStableSince     = 0;      // Zeitpunkt, seit dem Gewicht stabil ist
unsigned long   tareCheckTime       = 3000;   // ms: wie lange Gewicht stabil sein muss
float           tareCheckTolerance  = 0.2;  // kg: maximale Abweichung
const uint16_t  MAX_SAMPLES         = 100;
uint16_t        sampleCount         = 0;
unsigned long   stableSince         = 0;
WeightSample    weightSamples[MAX_SAMPLES];


// -- TFT + Touch Objekte --------------------
// 320x240 ILI9341 Display
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas1 canvasWeight(250, 85);
XPT2046_Touchscreen ts(TOUCH_CS);
int currentScreen = 0; // 0=Init-Bildschirm, 1=Hauptbildschirm, 2=Konfigurationsbildschirm

// --- GUI Struktur --------------------
typedef void (*CallbackFn)();

struct GuiBox {
  int x, y, w, h, textSize = 2;
  const char *label;
  uint16_t color;
  CallbackFn onTouch;
};

struct GuiBoxSet {
    GuiBox* boxes;      // Array der GuiBoxen
    size_t count;       // Anzahl Elemente
};

// Forward-Deklarationen der Callback-Funktionen
void onBoxTara();
void onBoxConf();
void onBoxConfigSave();
void onBoxConfigCancel();
void onBoxConfigInc();
void onBoxConfigDec();
void onBoxConfigTouchCalib();
void onBoxConfigScaleCalib();

// --- Zentrales Array mit allen Boxen ---
GuiBox setupBoxes[] = {
};

GuiBox guiBoxes[] = {
//   x,   y,   w,   h, labelSize,  label,         color,    onTouch
  {  5,  170, 120, 60, 2,          "Tara", ILI9341_GREEN,  onBoxTara},
  {190,  170, 120, 60, 2,          "Set",  ILI9341_GREEN,  onBoxConf},

};

GuiBox configBoxes[] = {
//   x,   y,   w,   h, labelSize, label,          color,          onTouch
  {  5,  170, 140, 60, 1,         "Speichern",    ILI9341_GREEN,  onBoxConfigSave},
  {170,  170, 140, 60, 1,         "Abbrechen",    ILI9341_GREEN,  onBoxConfigCancel},
  {170,   20,  60, 60, 2,         "+",            ILI9341_GREEN,  onBoxConfigInc},
  {250,   20,  60, 60, 2,         "-",            ILI9341_GREEN,  onBoxConfigDec},
  {  5,   95, 140, 60, 1,         "Waage Kalib.", ILI9341_GREEN,  onBoxConfigScaleCalib},
  {170,   95, 140, 60, 1,         "Touch Kalib.", ILI9341_GREEN,  onBoxConfigTouchCalib},
};

GuiBoxSet menu[] = {
    { setupBoxes,   sizeof(setupBoxes)  / sizeof(GuiBox) },
    { guiBoxes,     sizeof(guiBoxes)    / sizeof(GuiBox) },
    { configBoxes,  sizeof(configBoxes) / sizeof(GuiBox) },
};

const size_t SET_COUNT = sizeof(menu) / sizeof(GuiBoxSet);

// --- Forward-Deklarationen --------------------
void calibrateTouch();
void calibrateScale();
void drawBox(const GuiBox &box);
void drawGui();
void handleTouch(int tx, int ty);
void doTare();
void manualTare();
void getCurrentWeight();
void outputCurrentWeight();
void checkTouch();
void checkAutoTare();
void configMenu();
void outputNewWeight();
void drawConfigMenu();
void loadOrInitConfigData();
bool loadConfigData();
void saveConfigData();
void setupTft();
void setupTouch();
void setupScale();
void getAllScaleValue();
void pruneOldSamples(unsigned long now);

// --- Funktionen --------------------

void pruneOldSamples(unsigned long now) {
uint16_t j = 0;
  for (uint16_t i = 0; i < sampleCount; i++) {
    if (now - weightSamples[i].ts <= tareCheckTime) {
      weightSamples[j++] = weightSamples[i];
    }
  }
  sampleCount = j;
}


void loadOrInitConfigData() {
  if (!loadConfigData()) {
    Serial.println("Nutze Standard-Konfiguration und speichere diese.");
    saveConfigData();
  } else {
    Serial.println("Konfiguration erfolgreich geladen: ");
    Serial.print("Touchscreen MinX: ");
    Serial.println(calib.tsMinX);
    Serial.print("Touchscreen MaxX: ");
    Serial.println(calib.tsMaxX);
    Serial.print("Touchscreen MinY: ");
    Serial.println(calib.tsMinY);
    Serial.print("Touchscreen MaxY: ");
    Serial.println(calib.tsMaxY);
    Serial.print("Scale Calibration: ");
    Serial.println(calib.scaleCalibration);
  }
}

bool loadConfigData() {
  EEPROM.begin(EEPROM_SIZE);

  Serial.println("Loading configuration data...");

  CalibrationData temp;

  // Aus EEPROM lesen
  uint8_t* ptr = (uint8_t*)&temp;
  for (unsigned int i = 0; i < sizeof(CalibrationData); i++) {
    ptr[i] = EEPROM.read(EEPROM_ADDR + i);
  }

  EEPROM.end();

  // Validität prüfen
  if (strncmp(temp.head, "MRAb", 4) != 0) {
    Serial.println("ERROR: HEADER mismatch -> Data invalid!");
    return false;
  }

  if (strncmp(temp.foot, "MRAe", 4) != 0) {
    Serial.println("ERROR: FOOTER mismatch -> Data invalid!");
    return false;
  }

  // Alles ok → übernehmen
  calib = temp;

  Serial.println("Calibration data loaded successfully.");
  return true;
}

void saveConfigData() {
  EEPROM.begin(EEPROM_SIZE);

  Serial.println("Saving calibration data...");

  // calib ins EEPROM schreiben
  uint8_t* ptr = (uint8_t*)&calib;
  for (unsigned int i = 0; i < sizeof(CalibrationData); i++) {
    EEPROM.write(EEPROM_ADDR + i, ptr[i]);
  }

  EEPROM.commit();
  EEPROM.end();

  Serial.println("Calibration data saved.");
}

void drawBox(const GuiBox &box) {
int16_t   x1, y1;
uint16_t  w, h;
  tft.setTextSize(box.textSize);
  tft.setTextColor(box.color);
  tft.getTextBounds(box.label, box.x, box.y, &x1, &y1, &w, &h);
  tft.drawRect(box.x, box.y, box.w, box.h, box.color);
  tft.setCursor(box.x + ((box.w - w)/2), box.y + box.h / 1.5);
  tft.print(box.label);
}

void drawBoxes(const int menuNo) {
  tft.fillScreen(ILI9341_BLACK);
  for (int i = 0; i < menu[menuNo].count; i++) {
    drawBox(menu[menuNo].boxes[i]);
  } 
}

void drawGui() {
  tft.setFont(&FreeSans9pt7b);
  drawBoxes(1); // Hauptmenü
  tft.setCursor(250, 95);
  tft.setTextColor(ILI9341_WHITE);
  tft.print("kg");
}

void drawConfigMenu() {
  drawBoxes(2); // Konfigurationsmenü
}

void handleTouch(int tx, int ty) {
  for (int i = 0; i < menu[currentScreen].count; i++) {
    GuiBox &b = menu[currentScreen].boxes[i];
    if (tx >= b.x && tx <= b.x + b.w &&
        ty >= b.y && ty <= b.y + b.h) {
      if (b.onTouch) b.onTouch();
    }
  }
}

void manualTare() {
int       x = 10;
int       y = 140;  
int16_t  x1, y1;
uint16_t w, h;

  doTare();
  tft.setCursor(x, y);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setFont(&FreeSans9pt7b);
  tft.println("Tariert");
  delay(1000);
  tft.setCursor(x, y);
  tft.getTextBounds("Tariert", x, y, &x1, &y1, &w, &h);
  tft.fillRect(x1, y1, w, h, ILI9341_BLACK); // Lösche "Tariert" wieder
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

void onBoxConfigTouchCalib() {
  calibrateTouch();
}

void onBoxConfigScaleCalib() {
  calibrateScale();
}

void onBoxTara() {
  manualTare();
}

void onBoxConf() {
  configMenu();
}

void calibrateScale() {
float measuredUnits = 0.0;

  Serial.print("Starte Waagen-Kalibrierung auf");
  Serial.printf("%6.1f kg\n", newWeight);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(10, 10);
  tft.setTextSize(1);
  // tft.setFont(&FreeSans9pt7b);
  tft.print("Waage Kalibrieren auf ");
  tft.printf("%6.1f kg\n", newWeight);
  tft.println("Bitte Gewicht in 5 Sek. entfernen");
  Serial.println("Warte auf Entfernen des Gewichts...");
  delay(5000);
  Serial.println("Nulle");
  scale.tare();
  scale.set_scale();
  tft.print("Genullt, jetzt Gewicht von");
  tft.printf("%6.1f kg ", newWeight);
  tft.println("auflegen (5 Sek.)");
  Serial.println("Warte auf Auflegen des Gewichts...");
  delay(5000);
  tft.println("Messe...");
  Serial.println("Messe...");
  getAllScaleValue();
  Serial.println("scale.get_units");
  measuredUnits = scale.get_units(10);
  Serial.print("measuredUnits: ");
  Serial.println(measuredUnits);
  tft.print("Units: ");
  tft.printf("%6.2f\n", measuredUnits);
  tft.print("Raw: ");
  tft.printf("%ld\n", raw);
  tft.print("Value: ");
  tft.printf("%f\n", value);
}

void onBoxConfigSave() {
float diff = 0;

  Serial.println("Einstellungen Speichern:");
  diff = currentWeight / newWeight;
  Serial.print("currentWeight: ");
  Serial.print(currentWeight);
  Serial.print(" / newWeight: "); 
  Serial.print(newWeight);
  Serial.print(" = diff:");
  Serial.print(diff);
  Serial.print(" => neuer Kalibrierwert: calib.scaleCalibration ");
  Serial.print(calib.scaleCalibration);
  Serial.print(" * diff ");
  Serial.print(diff);
  Serial.print(" = ");
  Serial.println(calib.scaleCalibration * diff);
  calib.scaleCalibration = calib.scaleCalibration * diff;
  #ifdef DEBUG
    Serial.print("Alter Kalibrierwert: ");
    calib.scaleCalibration = -21; // Reset auf Standardwert vor Berechnung
    Serial.println(calib.scaleCalibration);
  #endif
  saveConfigData();
  scale.set_scale(-20.9);
  currentScreen = 1;
}

void onBoxConfigCancel() {
  Serial.println("Einstellungen abbrechen");
  currentScreen = 1;
}

void onBoxConfigInc() {
  Serial.println("+");
  newWeight = newWeight + 0.1;
  outputNewWeight();
}

void onBoxConfigDec() {
  Serial.println("-");
  newWeight = newWeight - 0.1;
  outputNewWeight();
}

void outputNewWeight() {
  tft.setCursor(8, 40);
  tft.print("=>");
  tft.setCursor(40, 40);
  tft.printf("%6.1f", newWeight);
}

void configMenu() {
  Serial.println("Einstellungen aufrufen");
  currentScreen = 2;
  drawConfigMenu();
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont();
  tft.setCursor(40, 10);
  tft.setTextSize(3);
  tft.printf("%6.1f", currentWeight);
  newWeight = currentWeight;
  outputNewWeight();
  while (currentScreen == 2) {
    checkTouch();
    delay(100);
  }
  drawGui();
  Serial.println("Einstellungen verlassen");
}

void calibrateTouch() {
int rx1, ry1, rx2, ry2, rx3, ry3, rx4, ry4;

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
  unsigned long now         = millis();
  float         difference  = 0;
  float         wMin        = 0;
  float         wMax        = 0;
  float         spread      = 0;

  // neuen Messwert ablegen
  if (sampleCount < MAX_SAMPLES) {
    weightSamples[sampleCount++] = { currentWeight, now };
  }

  // alte Samples verwerfen (Zeitfenster)
  pruneOldSamples(now);

  if (sampleCount < 2) {
    stableSince = 0;
    return;
  }

  wMin = weightSamples[0].value;
  wMax = weightSamples[0].value;

  for (uint16_t i = 1; i < sampleCount; i++) {
    wMin = min(wMin, weightSamples[i].value);
    wMax = max(wMax, weightSamples[i].value);
  }

  spread = wMax - wMin;

#ifdef DEBUG
  Serial.printf("Stability(time): samples=%u spread=%.3f tareCheckTolerance=%.2f \n", sampleCount, spread, tareCheckTolerance);
  Serial.printf("now=%lu - weightSamples[0].ts)=%lu = %lu tarecheckTime=%lu \n", now, weightSamples[0].ts, now - weightSamples[0].ts, tareCheckTime);
#endif

  // --- Stabilitätslogik ---
  if (spread <= tareCheckTolerance) {

    // Stabilität beginnt JETZT
    if (stableSince == 0) {
      stableSince = now;

      #ifdef DEBUG
      Serial.println("Stabilitätsphase gestartet");
    #endif
    }

    // Stabil genug lange?
    if (now - stableSince >= tareCheckTime) {
      Serial.printf("Auto-Tare Check: currentWeight: %.2f - lastWeight: %.2f", currentWeight, lastWeight);
      difference = currentWeight - lastWeight;
      if (difference < 0) {
        // ins Positive drehen
        difference = -difference;
      } 

      Serial.printf(" -> difference: %.2f", difference);
      if (difference < tareThreshold) { 
        Serial.printf(" < tareThreshold: %.2f => Automatisch tariert. \n", tareThreshold);
        sampleCount = 0;
        stableSince = 0;
        doTare();
      } else {
        Serial.printf(" >= tareThreshold: %.2f kein Tare.\n", tareThreshold);
      }
    }

  } else {
    // Instabil → Reset
    stableSince = 0;
  }
}


void getAllScaleValue() {
  Serial.print("scale.read_average: ");
  raw = scale.read_average(samplesPerReading);
  Serial.printf(" Raw: %ld \n", raw);

  Serial.print("scale.get_value:");
  value = scale.get_value(samplesPerReading);
  Serial.printf(" Value: %.5f \n", value);
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
          getAllScaleValue();
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
      Serial.println("scalePresent == false");
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
    canvasWeight.fillScreen(ILI9341_BLACK);
    canvasWeight.setCursor(0, canvasWeight.height()-15); // Die 0-Position ist unten links aber auf Höhe des Punktes
    canvasWeight.setFont(&FreeSans48pt7b);
    canvasWeight.printf("%6.1f", currentWeight);
    tft.drawBitmap(0, 20, canvasWeight.getBuffer(), canvasWeight.width(), canvasWeight.height(), ILI9341_WHITE, ILI9341_BLACK); 
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

void setupTouch() {
  if (ts.begin()) {
    Serial.println("XPT2046 bereit");
    ts.setRotation(3);
  } else {
    Serial.println("XPT2046 nicht gefunden!");
  }
}

void setupScale() {
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
    scale.set_gain(128); // Kanal A, Gain 128
    scale.set_scale(calib.scaleCalibration); // 
    scale.tare();
    Serial.println("HX711 bereit, Nullpunkt gesetzt.");
    tft.println("Waage bereit...       ");
  #endif
}

void setupTft() {
  tft.begin();
  Serial.print("Display initialisiert: ");
  tft.setFont(&FreeSans9pt7b);
  Serial.print(tft.width());
  Serial.print("x");  
  Serial.println(tft.height()); 
  tft.setRotation(1); // Querformat
  tft.fillScreen(ILI9341_BLACK);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println();
  Serial.println("DoggoScale!");

  // Konfig laden
  loadOrInitConfigData();

  // TFT starten
  
  setupTft();
  // Touch initialisieren
  setupTouch();

  // HX711 initialisieren
  setupScale();

  delay(500);
  drawGui();
  currentScreen = 1;
}

void loop() {
  checkTouch();
  getCurrentWeight();
  outputCurrentWeight();  
}
