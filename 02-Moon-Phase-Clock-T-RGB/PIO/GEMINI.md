# Projekt: Lilygo T-RGB Moon Phase Clock 🌕

## Persönliche Präferenzen & Kontext
- **Nutzer:** Ich bin Ed.
- **Sprache:** Code-Kommentare, Dokumentationen, Walktrough, Implemetation Plan und Debug-Ausgaben bevorzugt in **Deutsch** halten.
- **Interessen:** Vielseitiger Entwickler mit Fokus auf:
  - **Python:** Automatisierung, KI-Integration, Datenverarbeitung.
  - **Webdesign:** Moderne UIs (HTML/JS), Ästhetik & User Experience.
  - **Android-Apps:** Mobile Anwendungsentwicklung.
  - **Microcontroller:** ESP32/Arduino (IoT & Smart Home).

## Hardware-Kontext
- **Board:** Lilygo T-RGB (ESP32-S3) mit kreisförmigem 2.1 Zoll Display.
- **Display-Treiber:** XL9535 (GPIO Expander) für Panel-Initialisierung (XL9535_Init()).
- **Sensoren/Module:** Batteriestand-Messung (ADC am Pin 4), Sprachsteuerung (SenseVoice), Moon Phase Berechnungen.

## Programmier-Richtlinien
- **Framework:** Arduino mit PlatformIO.
- **UI:** LVGL (Version 8.x), oft generiert via SquareLine Studio (SLS).
- **Sprache:** Code-Kommentare, Walktrough, Implemetation Plan und Debug-Ausgaben bevorzugt in **Deutsch** halten.
- **Stil:** 
  - Nutze `lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN)` zum Verbergen von Elementen im Standby.
  - Verwende `Serial.printf()` für Debugging.
  - Achte auf effizientes Power-Management (Light/Deep Sleep).
  - UI-Elemente sollten mit dem kreisförmigen Display harmonieren (Zentrierung!).

## Wichtige Befehle & Logik
- **Voice Commands:** Identifier für Pomodoro (Start=5, Stop=6, Reset=7).
- **Batterie-Thresholds:** 4.1V (100%), 3.4V (Kritisch).
- **Standby:** Bei Standby (Light Sleep) wird das Display gedimmt und nur die Batterie-Anzeige (Icon + Prozent) mittig angezeigt.

