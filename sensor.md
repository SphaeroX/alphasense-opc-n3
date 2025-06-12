## OPC-N3 über SPI ansteuern – Technische Anleitung
### 1. Elektrische Grunddaten und Pin-Zuordnung

| OPC-N3 Pin                                                                                        | Funktion                              | ESP32 VSPI-Signal\*        |
| ------------------------------------------------------------------------------------------------- | ------------------------------------- | -------------------------- |
| 1                                                                                                 | Vcc (4 .8 – 5.2 V DC, 180 mA typisch) | 3 .3 V oder 5 V Versorgung |
| 2                                                                                                 | SCK                                   | GPIO 18                    |
| 3                                                                                                 | SDO (Slave Data Out)                  | MISO (GPIO 19)             |
| 4                                                                                                 | SDI (Slave Data In)                   | MOSI (GPIO 23)             |
| 5                                                                                                 | /SS (Chip Select)                     | GPIO 5                     |
| 6                                                                                                 | GND                                   | GND                        |
| \*Andere IO-Pins sind zulässig, solange sie per `spi_bus_config` entsprechend zugewiesen werden.  |                                       |                            |

---

### 2. SPI-Schnittstelle korrekt konfigurieren

| Parameter                                                   | Wert                                                                                  |
| ----------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| Modus                                                       | **Mode 1** (CPOL = 0, CPHA = 1)                                                       |
| Taktfrequenz                                                | 300 kHz … 750 kHz                                                                     |
| CS-Logik                                                    | /SS permanent **LOW** während jeder Übertragung                                       |
| Initial-Delay nach Power-Up                                 | ≥ 2 s vor erstem Befehl                                                               |
| Byte-Timings                                                | ≥ 10 ms zwischen Befehls- und Datenbytes, ≥ 10 µs zwischen aufeinanderfolgenden Bytes |
| Zykluszeit Histogramm                                       | 0 .5 – 20 s, niemals > 60 s                                                           |
| Wartezeit nach FAN/LASER-Einschalten                        | ≥ 600 ms, praxisnah 5 – 10 s                                                          |
| Diese Vorgaben gelten laut Alphasense-Flowchart und Manual. |                                                                                       |

---

### 3. Handshake-Protokoll

| Rückgabebyte                                                                                                                                                              | Bedeutung                                        |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------ |
| 0x31                                                                                                                                                                      | Gerät **busy** – weiter pollen                   |
| 0xF3                                                                                                                                                                      | Gerät **ready** – gewünschte Daten stehen bereit |
| Nach jedem Befehlsbyte pollt der Master mit demselben Byte, bis 0xF3 empfangen wird. Falsches Byte oder Timingfehler entkoppeln den Trigger; danach > 2 s Pause einlegen. |                                                  |

---

### 4. Wichtige SPI-Befehle (Kurzreferenz)

| Funktion                                                           | Cmd      | Folgebyte (Option)                                               | Antwort (Start)                                     |
| ------------------------------------------------------------------ | -------- | ---------------------------------------------------------------- | --------------------------------------------------- |
| Peripherie Power (Fan/Laser/Gain)                                  | **0x03** | 0x02 = Fan OFF, 0x03 = Fan ON, 0x06 = Laser OFF, 0x07 = Laser ON | 0x31 ➔ 0xF3                                         |
| Histogramm lesen + reset                                           | **0x30** | –                                                                | 0x31 ➔ 0xF3, anschließend 86 Bytes Histogramm-Frame |
| PM-Werte lesen + reset                                             | **0x32** | –                                                                | 0x31 ➔ 0xF3, 14 Bytes (3×Float + CRC)               |
| DAC/Power-Status lesen                                             | 0x13     | –                                                                | s. Manual                                           |
| Fan/Laser-Pot einstellen                                           | 0x42     | <Channel>, <PotVal>                                              | –                                                   |
| Bin-Weighting-Index setzen                                         | 0x05     | \<Index 0-10>                                                    | –                                                   |
| Status prüfen                                                      | 0xCF     | –                                                                | 0x31 ➔ 0xF3                                         |
| Ausführliche Tabellen finden sich in Appendix D und im Supplement. |          |                                                                  |                                                     |

---

### 5. Messablauf – Schritt für Schritt

1. **Spannung anlegen** und 2 s warten.
2. **SPI initialisieren** (vgl. Abschnitt 2).
3. **Laser und Fan einschalten**

   ```text
   Send 0x03 0x03   ; Fan ON
   Poll, warte 5 s
   Send 0x03 0x07   ; Laser ON
   ```
4. Erste Histogrammaufnahme **verwerfen** (undefinierte Periodendauer).
5. **Histogramm anfordern**

   ```text
   Send 0x30
   Poll until 0xF3
   Read 86 Bytes
   Prüfe CRC-16 (Polynomial 0xA001)
   ```

   CRC-Berechnung siehe Beispielcode im Manual.&#x20;
6. **Sampling-Intervall einhalten** (0 .5 – 20 s).
7. Optional **PM-Kurzabfrage** (`0x32`) für nur PM₁/₂.₅/₁₀.
8. Nach Abschluss **Fan/Laser ausschalten**, um Strom zu sparen.
9. Bei > 65 s SPI-Stille wechselt das Gerät in Autonom-Modus (SD-Logging).&#x20;

---

### 6. Dateninterpretation

* **Bin-Counts**: 24×16-bit, absolute Partikelzahl je Größenkanal.
* **MToF**: Zeit / 3 µs, charakterisiert Partikelgeschwindigkeit.
* **Sampling Period**: 16-bit, Sekunden × 100
* **Sample Flow Rate**: 16-bit, ml /s × 100
* **Umweltwerte**: Temperatur und rel. Feuchte je 16-bit, Umrechnung:

  $$
  RH\,[\%] \;=\; 100 \times \frac{SRH}{2^{16}-1} \quad\text{und}\quad
  T\,[°C] \;=\; -45 + 175 \times \frac{ST}{2^{16}-1}
  \] :contentReference[oaicite:3]{index=3}  
  $$
* **PM-A/B/C**: IEEE-754 Float (Standard: PM₁, PM₂.₅, PM₁₀).

---

### 7. Best Practices und Fallstricke

* Alle Logikleitungen mit **3 .3 V-Pegel** betreiben, auch wenn 5 V tolerant.
* **CS während gesamter Transaktion niedrig** halten, sonst Datenverlust.
* **Laser-Leistung nicht verändern**, da sonst Kalibrierung ungültig wird.
* **Ventilator langsamer stellen** nur in extrem staubigen Umgebungen.
* Bei Fehler-Timeouts stets ≥ 2 s warten und Puffer leeren.
* Keine scharfen Krümmer oder enge Schläuche am Ein- oder Auslass anbringen; Druckabfall < 40 Pa.&#x20;

---

### 8. Minimaler Pseudocode-Ablauf

```example
# pip install spidev crcmod
# SPI init
spi = spidev.SpiDev(1, 0)           # VSPI
spi.max_speed_hz = 500_000
spi.mode = 0b01                     # CPOL=0, CPHA=1

def poll_ready(cmd):
    while spi.xfer2([cmd])[0] != 0xF3:
        time.sleep(0.01)            # 10 ms

# Fan & Laser ON
spi.xfer2([0x03, 0x03]); poll_ready(0x03)
time.sleep(5)
spi.xfer2([0x03, 0x07]); poll_ready(0x03)
time.sleep(5)

# Read Histogram
spi.xfer2([0x30]); poll_ready(0x30)
data = spi.readbytes(86)            # incl. CRC
```

