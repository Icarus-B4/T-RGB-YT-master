#include <LilyGo_RGBPanel.h> 
#include <lvgl.h>
#include <LV_Helper.h>
#include <ui.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include "DFRobot_DF2301Q.h"
#include "battery.h"
#include "moon.h"


#include "credential.h"

LilyGo_RGBPanel panel;
TFT_eSPI tft = TFT_eSPI();
DFRobot_DF2301Q_I2C asr;

// Define the time zone
const char* ntpServer = "pool.ntp.org";
const char* timeZone = "CET-1CEST,M3.5.0,M10.5.0/3";

SemaphoreHandle_t mutex;
SemaphoreHandle_t asrMutex;
TaskHandle_t updateTimeTaskHandle = NULL;
TaskHandle_t syncMoonDataTaskHandle = NULL;
unsigned long previousMillis = 0;
const long interval = 1000;
const long ntpSyncInterval = 60000;
const unsigned long AUTO_SCREEN_1_DURATION = 30 * 60 * 1000; // 30 Minuten Mond
const unsigned long AUTO_SCREEN_2_DURATION = 30 * 60 * 1000; // 30 Minuten Countdown
unsigned long lastScreenChangeMillis = 0;
unsigned long previousNtpMillis = 0;
unsigned long previousMoonUpdateMillis = 0;

// Function declarations
void connectToWiFi();
void updateTime(void * parameter);
void syncMoonData(void * parameter);
void updateUILabels(String formattedTime, String ampm, String formattedDate);
void updateMoonData();
void wakeup();
void recoverVoiceModule();
void updateAppUI();


extern const lv_img_dsc_t *ui_imgset_moon_[30];

enum AppState { STATE_CLOCK, STATE_POMODORO, STATE_BREAK };
lv_obj_t * ui_Arc_Battery;
lv_obj_t * ui_Label_BatteryIcon;
lv_obj_t * ui_Label_StandbyIcon;
lv_obj_t * ui_Label_StandbyPerc;
bool manualCharging = false;
bool isStandby = false;

AppState currentAppState = STATE_CLOCK;
int32_t pomodoroSeconds = 0;
uint32_t ignoreAasrUntil = 0; 
void wakeup_event_cb(lv_event_t * e) {
    if (isStandby) {
        isStandby = false;
    }
}

void screen1_swipe_cb(lv_event_t * e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT) {
        lv_scr_load_anim(ui_Screen2, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        lastScreenChangeMillis = millis(); // Reset timer on manual swipe
    }
}

void screen2_swipe_cb(lv_event_t * e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        lv_scr_load_anim(ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
        lastScreenChangeMillis = millis(); // Reset timer on manual swipe
    }
}


void pomodoro_event_cb(lv_event_t * e) {
    if (currentAppState == STATE_CLOCK) {
        currentAppState = STATE_POMODORO;
        // pomodoroSeconds = 1 * 10; // 10 Sekunden Test
        pomodoroSeconds = 25 * 60; // 25 Minuten
        lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFF4500), LV_PART_MAIN); 
        if (xSemaphoreTake(asrMutex, portMAX_DELAY)) {
            asr.playByCMDID(5); // "Starte Pomodoro" Sprachausgabe
            ignoreAasrUntil = millis() + 4000; // Eigene Stimme 4 Sek. ignorieren
            xSemaphoreGive(asrMutex);
        }
        updateAppUI();
    } else {
        currentAppState = STATE_CLOCK;
        lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        updateAppUI();
    }
}

void setup(void)
{
    Serial.begin(115200); 
    delay(2000); // Give serial some time
    Serial.println("\n[BOOT] Starting Lilygo T-RGB Moon Clock...");

    connectToWiFi();
    configTzTime(timeZone, ntpServer);

    int retryPanel = 0;
    while (!panel.begin()) {
        retryPanel++;
        Serial.printf("[BOOT] Error, failed to initialize T-RGB Panel (XL9555) - Attempt %d\n", retryPanel);
        if (retryPanel > 5) {
            Serial.println("[BOOT] CRITICAL: Panel initialization failed after 5 attempts. Rebooting...");
            delay(5000);
            ESP.restart();
        }
        delay(1000);
    }
    Serial.println("[BOOT] Panel initialized successfully!");
    
    beginLvglHelper(panel);
    ui_init();

    delay(200);
    Wire.setClock(100000); 
    delay(100);

    asrMutex = xSemaphoreCreateMutex();

    if (xSemaphoreTake(asrMutex, portMAX_DELAY)) {
        if (!asr.begin()) {
            Serial.println("Voice recognition module not found!");
        } else {
            asr.setVolume(15); // Testweise auf 15 (Bereich 1-20)
            asr.setMuteMode(0); 
            asr.setWakeTime(18);
        }
        xSemaphoreGive(asrMutex);
    }

    ui_Arc_Battery = lv_arc_create(lv_scr_act());
    lv_obj_set_size(ui_Arc_Battery, 478, 478);
    lv_arc_set_bg_angles(ui_Arc_Battery, 0, 360);
    lv_arc_set_angles(ui_Arc_Battery, 0, 30); 
    lv_obj_center(ui_Arc_Battery);
    lv_obj_remove_style(ui_Arc_Battery, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(ui_Arc_Battery, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Arc_Battery, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_arc_width(ui_Arc_Battery, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_Arc_Battery, lv_color_hex(0x00A0FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(ui_Arc_Battery, 0, LV_PART_MAIN);

    ui_Label_BatteryIcon = lv_label_create(ui_Screen1);
    lv_obj_set_x(ui_Label_BatteryIcon, 185);
    lv_obj_set_y(ui_Label_BatteryIcon, 25);
    lv_obj_set_align(ui_Label_BatteryIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_font(ui_Label_BatteryIcon, &lv_font_montserrat_20, LV_PART_MAIN); 
    lv_obj_add_flag(ui_Label_BatteryIcon, LV_OBJ_FLAG_HIDDEN);

    // Standby Battery UI
    ui_Label_StandbyIcon = lv_label_create(ui_Screen1);
    lv_obj_set_align(ui_Label_StandbyIcon, LV_ALIGN_CENTER);
    lv_obj_set_y(ui_Label_StandbyIcon, -30);
    lv_label_set_text(ui_Label_StandbyIcon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_font(ui_Label_StandbyIcon, &lv_font_montserrat_48, LV_PART_MAIN); 
    lv_obj_add_flag(ui_Label_StandbyIcon, LV_OBJ_FLAG_HIDDEN);

    ui_Label_StandbyPerc = lv_label_create(ui_Screen1);
    lv_obj_set_align(ui_Label_StandbyPerc, LV_ALIGN_CENTER);
    lv_obj_set_y(ui_Label_StandbyPerc, 50);
    lv_label_set_text(ui_Label_StandbyPerc, "100%");
    lv_obj_set_style_text_font(ui_Label_StandbyPerc, &lv_font_montserrat_48, LV_PART_MAIN); 
    lv_obj_add_flag(ui_Label_StandbyPerc, LV_OBJ_FLAG_HIDDEN);

    lv_task_handler();
    panel.setBrightness(128);
    panel.enableTouchWakeup(); // Allow touch to wake from Deep Sleep

    lv_obj_add_flag(ui_Label_time, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Label_time, pomodoro_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_add_flag(ui_Screen1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Screen1, wakeup_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_Screen1, screen1_swipe_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(ui_Screen2, screen2_swipe_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(ui_Screen2, LV_OBJ_FLAG_SCROLLABLE);


    mutex = xSemaphoreCreateMutex();
    xTaskCreate(updateTime, "UpdateTime", 8192, NULL, 1, &updateTimeTaskHandle);
    xTaskCreate(syncMoonData, "SyncMoonData", 8192, NULL, 1, &syncMoonDataTaskHandle);

    // updateMoonData(); // Redundant, wird bereits vom syncMoonData Task beim Start erledigt
    pinMode(0, INPUT_PULLUP);
}

void loop()
{
    lv_task_handler();

    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'c') manualCharging = !manualCharging;
    }

     // Check for voice commands
    uint8_t CMDID = 0;
    if (millis() > ignoreAasrUntil && xSemaphoreTake(asrMutex, 10)) {
        CMDID = asr.getCMDID();
        xSemaphoreGive(asrMutex);
    }

    if (CMDID != 0) {
        Serial.printf("VOICE CMD received: %d\n", CMDID);
        
        if (CMDID == 5) { // "Starte Pomodoro"
            if (currentAppState == STATE_CLOCK) {
                currentAppState = STATE_POMODORO;
                pomodoroSeconds = 25 * 60; 
                lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFF4500), LV_PART_MAIN); 
                updateAppUI();
            }
        } else if (CMDID == 6) { // "Stop Pomodoro"
            if (currentAppState != STATE_CLOCK) {
                currentAppState = STATE_CLOCK;
                lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
                updateAppUI();
            }
        } else if (CMDID == 7) { // "Reset Pomodoro"
            currentAppState = STATE_CLOCK;
            pomodoroSeconds = 0;
            lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            updateAppUI();
        } else if (CMDID == 104) { // "Turn off the Light" -> Standby
            if (!isStandby) {
                isStandby = true;
                Serial.println("STANDBY: ON via Voice (104)");
            }
        } else if (CMDID == 103) { // "Turn on the Light" -> Wakeup
            if (isStandby) {
                isStandby = false;
                Serial.println("STANDBY: OFF via Voice (103)");
            }
        }
    }

    // Check BOT button (GPIO 0) for Deep Sleep / Reset
    static unsigned long btnPressStart = 0;
    if (digitalRead(0) == LOW) {
        if (btnPressStart == 0) btnPressStart = millis();
        // Long press (> 2 seconds) triggers Deep Sleep
        if (millis() - btnPressStart > 2000) {
            Serial.println("[POWER] Button Long Press -> Entering DEEP SLEEP");
            panel.sleep(); // Starts Deep Sleep
        }
    } else {
        btnPressStart = 0;
    }

    static bool lastStandbyState = false;
    if (isStandby != lastStandbyState) {
        if (isStandby) {
            Serial.println("[POWER] Entering STANDBY (Visible Mode)...");
            panel.setBrightness(5); // Very dim for standby
            
            // Switch to Screen 1 if on Screen 2
            if (lv_scr_act() == ui_Screen2) {
                lv_scr_load_anim(ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
            }

            
            // Hide standard UI
            if (lv_obj_is_valid(ui_Img_bg)) lv_obj_add_flag(ui_Img_bg, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Container_time)) lv_obj_add_flag(ui_Container_time, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Panel1)) lv_obj_add_flag(ui_Panel1, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_date)) lv_obj_add_flag(ui_Label_date, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Img_earth)) lv_obj_add_flag(ui_Img_earth, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Img_moon)) lv_obj_add_flag(ui_Img_moon, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_phase)) lv_obj_add_flag(ui_Label_phase, LV_OBJ_FLAG_HIDDEN);
            
            // Show standby UI
            lv_obj_clear_flag(ui_Label_StandbyIcon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_Label_StandbyPerc, LV_OBJ_FLAG_HIDDEN);
            updateAppUI();
        } else {
            Serial.println("[POWER] Woke up from STANDBY");
            panel.setBrightness(160); // Restore brightness
            updateAppUI();
            
            // Show standard UI
            if (lv_obj_is_valid(ui_Img_bg)) lv_obj_clear_flag(ui_Img_bg, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Container_time)) lv_obj_clear_flag(ui_Container_time, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Panel1)) lv_obj_clear_flag(ui_Panel1, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_date)) lv_obj_clear_flag(ui_Label_date, LV_OBJ_FLAG_HIDDEN);
            // Elements below are handled by updateAppUI fine, but loop did it here before
        }
        lastStandbyState = isStandby;
    }

    if (isStandby) {
        // Check for wakeup (Bot button restores)
        if (digitalRead(0) == LOW) {
            isStandby = false;
        }
    }

    // Voice module health check (every 60 seconds)
    static unsigned long lastVoiceCheck = 0;
    if (millis() - lastVoiceCheck > 60000) {
        lastVoiceCheck = millis();
        uint8_t currWake = 0;
        if (xSemaphoreTake(asrMutex, 10)) {
            currWake = asr.getWakeTime();
            xSemaphoreGive(asrMutex);
            if (currWake != 18) {
                Serial.printf("[VOICE] Health check failed (Received %d, expected 18)...\n", currWake);
                recoverVoiceModule();
            }
        }
    }

    // Automatic Screen Switch Logic (Glance Mode)
    if (!isStandby && currentAppState == STATE_CLOCK) {
        unsigned long currentMillis = millis();
        lv_obj_t * active_screen = lv_scr_act();
        
        if (active_screen == ui_Screen1) {
            if (currentMillis - lastScreenChangeMillis >= AUTO_SCREEN_1_DURATION) {
                Serial.println("[UI] Auto-switching to Screen 2 (Countdown)...");
                lv_scr_load_anim(ui_Screen2, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, false);
                lastScreenChangeMillis = currentMillis;
            }
        } else if (active_screen == ui_Screen2) {
            if (currentMillis - lastScreenChangeMillis >= AUTO_SCREEN_2_DURATION) {
                Serial.println("[UI] Auto-switching back to Screen 1 (Moon)...");
                lv_scr_load_anim(ui_Screen1, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, false);
                lastScreenChangeMillis = currentMillis;
            }
        }
    }

    delay(20);
}

void recoverVoiceModule() {
    Serial.println("[VOICE] Resetting module and I2C connection...");
    if (xSemaphoreTake(asrMutex, portMAX_DELAY)) {
        // Wire.begin() is called inside asr.begin() for this library
        if (asr.begin()) {
            asr.setVolume(15);
            asr.setMuteMode(0);
            asr.setWakeTime(18);
            Serial.println("[VOICE] Recovery successful!");
        } else {
            Serial.println("[VOICE] CRITICAL: Voice module unresponsive on I2C bus.");
        }
        xSemaphoreGive(asrMutex);
    }
}

void updateAppUI() {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        if (isStandby) {
            // Im Standby wird alles außer der Standby-Anzeige ausgeblendet
            if (lv_obj_is_valid(ui_Img_bg)) lv_obj_add_flag(ui_Img_bg, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Container_time)) lv_obj_add_flag(ui_Container_time, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Panel1)) lv_obj_add_flag(ui_Panel1, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_date)) lv_obj_add_flag(ui_Label_date, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Img_earth)) lv_obj_add_flag(ui_Img_earth, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Img_moon)) lv_obj_add_flag(ui_Img_moon, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_phase)) lv_obj_add_flag(ui_Label_phase, LV_OBJ_FLAG_HIDDEN);
            
            lv_obj_clear_flag(ui_Label_StandbyIcon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_Label_StandbyPerc, LV_OBJ_FLAG_HIDDEN);
        } else {
            // Normal-Modus
            lv_obj_add_flag(ui_Label_StandbyIcon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_Label_StandbyPerc, LV_OBJ_FLAG_HIDDEN);
            
            if (lv_obj_is_valid(ui_Img_bg)) lv_obj_clear_flag(ui_Img_bg, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Container_time)) lv_obj_clear_flag(ui_Container_time, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Panel1)) lv_obj_clear_flag(ui_Panel1, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_date)) lv_obj_clear_flag(ui_Label_date, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_BatteryIcon)) lv_obj_clear_flag(ui_Label_BatteryIcon, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Arc_Battery)) lv_obj_clear_flag(ui_Arc_Battery, LV_OBJ_FLAG_HIDDEN);

            // Mond, Erde und Mondphase bleiben im NormalModus immer sichtbar
            if (lv_obj_is_valid(ui_Img_earth)) lv_obj_clear_flag(ui_Img_earth, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Img_moon)) lv_obj_clear_flag(ui_Img_moon, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_phase)) lv_obj_clear_flag(ui_Label_phase, LV_OBJ_FLAG_HIDDEN);
        }
        xSemaphoreGive(mutex);
    }
}

void connectToWiFi() {
    Serial.println("[WiFi] Starte Verbindungssuche...");
    for (int i = 0; i < numNetworks; i++) {
        Serial.printf("[WiFi] Versuche Verbindung mit SSID: '%s'...\n", networks[i].ssid);
        WiFi.begin(networks[i].ssid, networks[i].password);
        
        int retryCount = 0;
        while (WiFi.status() != WL_CONNECTED) {
            delay(1000); 
            Serial.print(".");
            retryCount++;
            if (retryCount > 15) { 
                Serial.printf("\n[WiFi] Keine Antwort von %s nach 15s.\n", networks[i].ssid);
                WiFi.disconnect();
                break;
            }
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi] ERFOLGREICH verbunden mit: %s\n", networks[i].ssid);
            Serial.print("[WiFi] IP-Adresse: ");
            Serial.println(WiFi.localIP());
            return;
        }
    }
    Serial.println("\n[WiFi] KRITISCH: Konnte zu KEINEM der gespeicherten Netzwerke verbinden.");
    Serial.println("[WiFi] Hinweis: Prüfe Hotspot-Frequenz (nur 2.4GHz!) und Batteriestand.");
}

// ================= BACKGROUND TASK: CLOCK & SYSTEM =================
void updateTime(void * parameter) {
    for (;;) {
        unsigned long currentMillis = millis();

        updateBattery(currentMillis);

        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis;

            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);

            // Auto-brightness Fix: enforce target brightness
            int targetBrightness = (timeinfo.tm_hour >= 22 || timeinfo.tm_hour < 7) ? 20 : 160;
            static int currentAppliedBrightness = -1;
            if (targetBrightness != currentAppliedBrightness) {
                currentAppliedBrightness = targetBrightness;
                panel.setBrightness(targetBrightness);
            }

            bool showClock = true;
            if (currentAppState == STATE_POMODORO || currentAppState == STATE_BREAK) {
                pomodoroSeconds--;
                if (pomodoroSeconds < 0) {
                    if (currentAppState == STATE_POMODORO) {
                        currentAppState = STATE_BREAK;
                        pomodoroSeconds = 5 * 60; 
                        lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0x00FF00), LV_PART_MAIN); 
                        Serial.println("[VOICE] Playing Pause Start (CMD 104)...");
                        if (xSemaphoreTake(asrMutex, portMAX_DELAY)) {
                            asr.playByCMDID(104); // "I'm off now" (Test mit Licht-Aus ID)
                            ignoreAasrUntil = millis() + 4000;
                            xSemaphoreGive(asrMutex);
                        }
                        updateAppUI();
                    } else {
                        currentAppState = STATE_CLOCK;
                        lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
                        Serial.println("[VOICE] Playing Timer Done (CMD 23)...");
                        if (xSemaphoreTake(asrMutex, portMAX_DELAY)) {
                            asr.playByCMDID(23); // "Done"
                            delay(50); // 50 ms warten
                            asr.playByCMDID(103); // "Done"
                            ignoreAasrUntil = millis() + 4000;
                            xSemaphoreGive(asrMutex);
                        }
                        updateAppUI();
                    }
                }
                
                if (currentAppState != STATE_CLOCK) {
                    showClock = false;
                    int mins = pomodoroSeconds / 60;
                    int secs = pomodoroSeconds % 60;
                    char buf[10];
                    snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
                    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
                        updateUILabels(String(buf), "", currentAppState == STATE_POMODORO ? "FOKUS" : "PAUSE");
                        xSemaphoreGive(mutex);
                    }
                }
            }

            if (showClock && !isStandby) {
                int hours = timeinfo.tm_hour;
                int minutes = timeinfo.tm_min;
                int seconds = timeinfo.tm_sec;
                String ampm = (hours >= 12) ? "PM" : "AM";
                char timeStr[12];
                snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hours, minutes, seconds);
                char dateStr[11];
                strftime(dateStr, sizeof(dateStr), "%d-%b-%y", &timeinfo);

                if (xSemaphoreTake(mutex, portMAX_DELAY)) {
                    updateUILabels(String(timeStr), ampm, String(dateStr));
                    
                    // Update Countdown for Screen 2
                    struct tm targetDate = {0};
                    targetDate.tm_year = 2026 - 1900; 
                    targetDate.tm_mon  = 5 - 1; // May
                    targetDate.tm_mday = 6;
                    targetDate.tm_hour = 0;
                    targetDate.tm_min  = 0;
                    targetDate.tm_sec  = 0;

                    time_t targetTimestamp = mktime(&targetDate);
                    double diffSeconds = difftime(targetTimestamp, now);

                    if (diffSeconds > 0) {
                        long diffSecsInt = (long)diffSeconds;
                        int days = diffSecsInt / 86400;
                        diffSecsInt %= 86400;
                        int hours = diffSecsInt / 3600;
                        diffSecsInt %= 3600;
                        int minutes = diffSecsInt / 60;
                        int remainingSeconds = diffSecsInt % 60;

                        char buf[10];
                        if (lv_obj_is_valid(ui_Label_day)) {
                            snprintf(buf, sizeof(buf), "%02d", days);
                            lv_label_set_text(ui_Label_day, buf);
                        }
                        if (lv_obj_is_valid(ui_Label_hrs)) {
                            snprintf(buf, sizeof(buf), "%02d", hours);
                            lv_label_set_text(ui_Label_hrs, buf);
                        }
                        if (lv_obj_is_valid(ui_Label_min)) {
                            snprintf(buf, sizeof(buf), "%02d", minutes);
                            lv_label_set_text(ui_Label_min, buf);
                        }
                        if (lv_obj_is_valid(ui_Label_sec2)) {
                            snprintf(buf, sizeof(buf), "%02d", remainingSeconds);
                            lv_label_set_text(ui_Label_sec2, buf);
                        }
                    } else {
                        if (lv_obj_is_valid(ui_Label_day)) lv_label_set_text(ui_Label_day, "00");
                        if (lv_obj_is_valid(ui_Label_hrs)) lv_label_set_text(ui_Label_hrs, "00");
                        if (lv_obj_is_valid(ui_Label_min)) lv_label_set_text(ui_Label_min, "00");
                        if (lv_obj_is_valid(ui_Label_sec2)) lv_label_set_text(ui_Label_sec2, "00");
                    }

                    
                    xSemaphoreGive(mutex);
                }
            }
        }

        if (currentMillis - previousNtpMillis >= ntpSyncInterval) {
            previousNtpMillis = currentMillis;
            if (WiFi.status() == WL_CONNECTED) {
                configTzTime(timeZone, ntpServer);
                Serial.println("[NTP] Zeit-Synchronisierung angefordert...");
            } else {
                Serial.println("[NTP] Keine Synchronisierung möglich (WLAN getrennt).");
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void updateUILabels(String formattedTime, String ampm, String formattedDate) {
    lv_label_set_text(ui_Label_time, formattedTime.c_str());
    if (lv_obj_is_valid(ui_Label_ampm)) lv_label_set_text(ui_Label_ampm, ampm.c_str());
    if (lv_obj_is_valid(ui_Label_date)) lv_label_set_text(ui_Label_date, formattedDate.c_str());
}

void updateMoonData() {
    if (WiFi.status() != WL_CONNECTED || isStandby) return;

    MoonData data;
    if (fetchMoonData(data)) {
        if (xSemaphoreTake(mutex, portMAX_DELAY)) {
            if (lv_obj_is_valid(ui_Label_phase)) {
                lv_label_set_text(ui_Label_phase, data.phaseName.c_str());
            }
            if (lv_obj_is_valid(ui_Img_moon)) {
                lv_img_set_src(ui_Img_moon, ui_imgset_moon_[data.imageIndex]);
            }
            if (lv_obj_is_valid(ui_Image6)) {
                lv_img_set_src(ui_Image6, ui_imgset_moon_[data.imageIndex]);
            }
            xSemaphoreGive(mutex);
        }
    }
}

// Removed helper functions (now in moon.cpp)

void syncMoonData(void * parameter) {
    for (;;) {
        updateMoonData();
        vTaskDelay(600000 / portTICK_PERIOD_MS); // Update every 10 minutes
    }
}