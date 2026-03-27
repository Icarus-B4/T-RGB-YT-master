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
TaskHandle_t updateTimeTaskHandle = NULL;
TaskHandle_t syncMoonDataTaskHandle = NULL;
unsigned long previousMillis = 0;
const long interval = 1000;
const long ntpSyncInterval = 60000;
unsigned long previousNtpMillis = 0;
unsigned long previousMoonUpdateMillis = 0;

// Function declarations
void connectToWiFi();
void updateTime(void * parameter);
void syncMoonData(void * parameter);
void updateUILabels(String formattedTime, String ampm, String formattedDate);
void updateMoonData();
void wakeup();

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
void wakeup_event_cb(lv_event_t * e) {
    if (isStandby) {
        isStandby = false;
    }
}

void pomodoro_event_cb(lv_event_t * e) {
    if (currentAppState == STATE_CLOCK) {
        currentAppState = STATE_POMODORO;
        pomodoroSeconds = 25 * 60; 
        lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFF4500), LV_PART_MAIN); 
        asr.playByCMDID(1);
    } else {
        currentAppState = STATE_CLOCK;
        lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
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

    if (!asr.begin()) {
        Serial.println("Voice recognition module not found!");
    } else {
        asr.setVolume(10);
        asr.setMuteMode(0); 
        asr.setWakeTime(18);
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

    mutex = xSemaphoreCreateMutex();
    xTaskCreate(updateTime, "UpdateTime", 8192, NULL, 1, &updateTimeTaskHandle);
    xTaskCreate(syncMoonData, "SyncMoonData", 8192, NULL, 1, &syncMoonDataTaskHandle);

    updateMoonData();
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
    uint8_t CMDID = asr.getCMDID();
    if (CMDID != 0) {
        Serial.printf("VOICE CMD received: %d\n", CMDID);
        
        if (CMDID == 5) { // "Starte Pomodoro"
            if (currentAppState == STATE_CLOCK) {
                currentAppState = STATE_POMODORO;
                pomodoroSeconds = 25 * 60; 
                lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFF4500), LV_PART_MAIN); 
            }
        } else if (CMDID == 6) { // "Stop Pomodoro"
            if (currentAppState != STATE_CLOCK) {
                currentAppState = STATE_CLOCK;
                lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            }
        } else if (CMDID == 7) { // "Reset Pomodoro"
            currentAppState = STATE_CLOCK;
            pomodoroSeconds = 0;
            lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
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
        } else {
            Serial.println("[POWER] Woke up from STANDBY");
            panel.setBrightness(160); // Restore brightness
            
            // Show standard UI
            if (lv_obj_is_valid(ui_Img_bg)) lv_obj_clear_flag(ui_Img_bg, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Container_time)) lv_obj_clear_flag(ui_Container_time, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Panel1)) lv_obj_clear_flag(ui_Panel1, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_date)) lv_obj_clear_flag(ui_Label_date, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Img_earth)) lv_obj_clear_flag(ui_Img_earth, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Img_moon)) lv_obj_clear_flag(ui_Img_moon, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_phase)) lv_obj_clear_flag(ui_Label_phase, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Label_BatteryIcon)) lv_obj_clear_flag(ui_Label_BatteryIcon, LV_OBJ_FLAG_HIDDEN);
            if (lv_obj_is_valid(ui_Arc_Battery)) lv_obj_clear_flag(ui_Arc_Battery, LV_OBJ_FLAG_HIDDEN);
            
            // Hide standby UI
            lv_obj_add_flag(ui_Label_StandbyIcon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_Label_StandbyPerc, LV_OBJ_FLAG_HIDDEN);
        }
        lastStandbyState = isStandby;
    }

    if (isStandby) {
        // Check for wakeup (Bot button restores)
        if (digitalRead(0) == LOW) {
            isStandby = false;
        }
    }

    delay(20);
}

void connectToWiFi() {
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        retryCount++;
        if (retryCount > 30) {
            Serial.println("\n[WiFi] Connection FAILED. Check credentials.");
            return;
        }
    }
    Serial.print("\n[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
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
                        asr.playByCMDID(5); 
                    } else {
                        currentAppState = STATE_CLOCK;
                        lv_obj_set_style_text_color(ui_Label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
                        asr.playByCMDID(1); 
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
                    xSemaphoreGive(mutex);
                }
            }
        }

        if (currentMillis - previousNtpMillis >= ntpSyncInterval) {
            previousNtpMillis = currentMillis;
            configTzTime(timeZone, ntpServer);
            Serial.println("Synced with NTP server");
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