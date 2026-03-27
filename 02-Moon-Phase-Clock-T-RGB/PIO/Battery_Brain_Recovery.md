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

## 3. Stromspar-Optionen (Deep vs. Standby)
Um die Laufzeit massiv zu verlängern, kann das Gerät schlafen gelegt werden:

### A. Deep Sleep (Touch Wakeup)
- **Vorteil:** Hält Wochen/Monate.
- **Nachteil:** Gerät macht kompletten Neustart (`setup()`) bei Berührung.
- **Status:** IMPLEMENTED (Long-press BOT button > 2s). Wakes on Button or Touch.

### B. Standby (Visible Mode - Always-On)
- **Vorteil:** Wacht sofort auf, zeigt im Ruhezustand den Akkustand zentriert an.
- **Nachteil:** Verbraucht etwas mehr als Light Sleep (da Display refresh aktiv bleibt), aber spart massiv Backlight-Strom.
- **Status:** IMPLEMENTED (Voice CMD 104 "Turn off the Light"). Wakes on Touch, Button or Voice.

## 4. Wichtige Pins & Parameter (T-RGB)
- **Baudrate:** 115200 (Serieller Monitor).
- **ADC-Pin:** Wird über die LilyGo Library (`panel.getBattVoltage()`) ausgelesen.
- **Brightness:** Wert 0 bis 255. Standard im Betrieb: 160. Im Standby: 5. 

## 5. Standby Batterie-Anzeige (März 2026)
**Feature:** Wenn das Gerät in den Standby geschickt wird (CMDID 104), erscheint ein großes, zentriertes Batterie-Icon und der Akkustand in %.

**Technische Details:**
- **Modus:** "Fake-Standby" statt Light Sleep. CPU bleibt wach, um Refresh für RGB-Panel zu senden.
- **Optik:** Hintergrund schwarz (`LV_OBJ_FLAG_HIDDEN` für alle UI-Elemente), Helligkeit auf **5** reduziert.
- **UI-Elemente:** `ui_Label_StandbyIcon` (🔋/⚡) und `ui_Label_StandbyPerc` (%), beide in **Montserrat 48**.
- **Wiederherstellung:** Beim Aufwachen werden alle UI-Elemente wieder eingeblendet und die Helligkeit auf **160** gesetzt. Dies geschieht robust über ein State-Transition-System in der `loop()`.
- **Lade-Animation:** Im Standby wird nur das statische Icon (⚡ bei USB-Power) gezeigt. Im Normal-Modus bleibt die blaue Ring-Animation (`ui_Arc_Battery`) bei USB-Power aktiv.

## 6. Dynamische Lade-Kompensation (Update 27.03.2026)
**Problem:** Beim Einstecken des Ladekabels sprang die Anzeige sofort auf 100%, da die Ladespannung den Messwert verfälscht.

**Lösung:**
- **Differenzierte Offsets:** 
    - **Laden:** `v_corr = v - 0.30` (Kompensiert den Spannungsanstieg durch das Ladegerät).
    - **Entladen:** `v_corr = v + 0.07` (Kompensiert den Lastabfall durch das Display).
- **Prozent-Dämpfung:** Der Prozentwert wird über einen Tiefpass-Filter geglättet (`smoothed_perc`), sodass die Anzeige langsam ansteigt/sinkt (ca. 2.5% pro Sekunde max). Dies verhindert nervöse Sprünge in der UI.
- **Log:** Der Serielle Monitor gibt nun auch den geglätteten Wert aus: `Perc: 65%, Smooth: 50%`.
