# Battery Brain Recovery - LilyGo T-RGB Moon Clock

In diesem Dokument ist das Wissen über die Akku-Logik und die Stromspar-Möglichkeiten des LilyGo T-RGB Moon Clock Projekts zusammengefasst.

## 1. Problemstellung & Lösung (März 2026)
**Symptom:** Akku wird bei 3,62V als "Rot" (unter 20%) angezeigt, obwohl er noch Kapazität haben sollte.

**Ursache:**
- **Lastabfall:** Das Display und das WiFi ziehen ca. 200-250mA. Unter dieser Last sinkt die Spannung an den ADC-Pins des ESP32 tiefer als am unbelasteten Voltmeter.
- **ADC-Toleranz:** Der ESP32 S3 hat keine exakte interne Kalibrierung ab Werk.
- **Laufzeit:** Ein 2100mAh Akku reicht bei 210mA Last nur für etwa 8-10 Stunden Betrieb (100% bis 10%).

**Implementierte Lösung (in `src/battery.cpp`):**
- **Kalibrierung:** Ein Software-Offset von **+0.07V** (`v_corr = v + 0.07`) gleicht die Differenz zum Voltmeter aus.
- **Farbschwelle:** Die Anzeige springt erst bei **unter 15%** auf Rot (vorher 20%).
- **Logging:** Der Serielle Monitor gibt alle 5s Werte aus: `[BATT] Raw: 3550mV, Corr: 3.62V, Perc: 28%`.

## 2. Stromverbrauch-Analyse
- **LilyGo T-RGB (aktiv):** ~130 mA (mit WiFi und Display).
- **DFRobot Voice Module:** ~60-100 mA (im Standby-Modus aktiv lauschend).
- **Gesamt:** ~200 - 250 mA.
- **2100 mAh Akku:** Hält ca. 9-10 Stunden (bei 90% Nutzung).

## 3. Stromspar-Optionen (Deep vs. Light Sleep)
Um die Laufzeit massiv zu verlängern, kann das Gerät schlafen gelegt werden:

### A. Deep Sleep (Touch Wakeup)
- **Vorteil:** Hält Wochen/Monate.
- **Nachteil:** Gerät macht kompletten Neustart (`setup()`) bei Berührung.
- **Status:** Vorbereitet in der Library (`panel.enableTouchWakeup()`).

### B. Light Sleep (Touch Wakeup)
- **Vorteil:** Wacht in Millisekunden auf ohne Reboot.
- **Nachteil:** Verbraucht ca. 10-20mA (hält den RAM aktiv), Akku hält damit ~3-4 Tage.

## 4. Wichtige Pins & Parameter (T-RGB)
- **Baudrate:** 115200 (Serieller Monitor).
- **ADC-Pin:** Wird über die LilyGo Library (`panel.getBattVoltage()`) ausgelesen.
- **Brightness:** Wert 0 bis 16 (oder 255 je nach Panel-Typ). Senken der Helligkeit spart massiv Strom.
