# Doggoscale!

Einfache Waage für Hunde.

# Funktionen
+ Auto-Tara: Stellt man etwas drauf, wie z.B. einen Napf, tariert sich die Waage anhand eines Gewichts-Schwellwerts automatisch auf 0. Steigt der Hund drauf, misst die Waage das Gewicht und tariert nicht.
+ Touch-Screen
+ Gewichts-Anpassung: Man kann das gemessene Gewicht einfach dem tatsächlichen Gewicht anpassen.
+ Display und Wiegeeinheit getrennt: Ein Ethernet-Verbindungskabel verbindet das Display mit der Wiegeeinheit
+ USB-Speisung

<img width="678" src="https://github.com/user-attachments/assets/66dab49f-7863-4c61-ac8d-f27c7db0dad3" />

# Nachbauen

## 1. Material kaufen
+ 1x Siebdruckplatte in gewünschter Größe und Stärke ([Link](https://www.amazon.de/dp/B01HS0Y0BY))
+ 4x Wägezellen ([Link](https://www.amazon.de/dp/B07FMN1DBN))
+ 1x Wägezellencontroller HX711 ([Link](https://www.amazon.de/dp/B07FMN1DBN))
+ 1x Microcontroller ESP8266 ESP-12F ([Link](https://www.amazon.de/dp/B07Z68HYW1))
+ 1x Display 2,8 Zoll LCD TFT Touch Display, 320x240 Auflösung, ILI9341 Treiber, SPI ([Link](https://www.amazon.de/dp/B0CP7S2X7V))
+ 1x Micro-USB Montageanschluss ([Link](https://www.amazon.de/dp/B0798PW4DB))
+ 1x RJ45 Breakout Board ([Link](https://www.amazon.de/dp/B083C4LYQL))
+ 1x RJ45 Montagekupplung ([Link](https://www.amazon.de/dp/B00B7AGLXM))
+ Lochrasterplatinen
+ Steckleisten
+ Lötkabel
+ Heißkleber
+ Für Gehäuse: 3D-Drucker mit PLA-Filament

## 2. Verkabeln

### Steckbrett
<img width="2798" height="2496" alt="doggoscale breadboard" src="https://github.com/user-attachments/assets/91c72137-a566-4fa0-ab27-0d8a39e3317a" />

Fritzing-File für Steckplatine: [doggoscale breadboard.fzz](https://github.com/mrapnx/doggoscale/blob/main/doggoscale%20breadboard.fzz)

### Streifenplatine
<img width="3144" height="3186" alt="doggoscale strip board" src="https://github.com/user-attachments/assets/6b69abdf-76e3-4edf-99bd-8f8329a60f81" />

Fritzing-File für Streifenplatine: [doggoscalestrip board.fzz](https://github.com/mrapnx/doggoscale/blob/main/doggoscale%20strip%20board.fzz)

## 3. Gehäuse drucken (lassen)

### Wiegeeinheit
#### Füße für die Wägezellen
[housing/doggoscale weighting cell fixture.3mf](https://github.com/mrapnx/doggoscale/blob/main/housing/doggoscale%20weighting%20cell%20fixture.3mf)

#### Gehäuse der Wiegeeinheit
Gehäuse: [housing/doggoscale housing connection box base.3mf](https://github.com/mrapnx/doggoscale/blob/main/housing/doggoscale%20housing%20connection%20box%20base.3mf)

Deckel: [housing/doggoscale housing connection box lid.3mf](https://github.com/mrapnx/doggoscale/blob/main/housing/doggoscale%20housing%20connection%20box%20lid.3mf)

### Display
<img width="678" height="613" alt="display_front" src="https://github.com/user-attachments/assets/5d7144a8-d492-4039-9c17-dc4cc80fd737" />
<img width="678" height="666" alt="display_back" src="https://github.com/user-attachments/assets/f4ad42a3-08fc-43b8-8c8f-d92519a32a71" />

Gehäuse: [housing/doggoscale housing display base.3mf](https://github.com/mrapnx/doggoscale/blob/main/housing/doggoscale%20housing%20display%20base.3mf) 

Deckel: [housing/doggoscale housing display lid.3mf](https://github.com/mrapnx/doggoscale/blob/main/housing/doggoscale%20housing%20display%20lid.3mf) 

## 4. Platine, Display und Anschlüsse montieren (Heißkleber hilft)

## 5. Mikrocontroller flashen
+ VS Code aufsetzen
+ Platform.IO installieren
+ Mikrocontroller anschließen
+ Builden und uploaden
