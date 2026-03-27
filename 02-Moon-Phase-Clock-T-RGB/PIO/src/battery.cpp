#include "battery.h"
#include <LilyGo_RGBPanel.h>
#include <lvgl.h>

extern LilyGo_RGBPanel panel;
extern SemaphoreHandle_t mutex;
extern lv_obj_t * ui_Arc_Battery;
extern lv_obj_t * ui_Label_BatteryIcon;
extern bool manualCharging;
extern bool isStandby;

void updateBattery(unsigned long currentMillis) {
    static unsigned long last_v_update = 0;
    static unsigned long last_anim_update = 0;
    static bool autoCharging = false;
    static float filtered_volt = 0;
    static float last_v = 0;

    // 1. Smooth Animation (every 20ms = 50fps)
    if (currentMillis - last_anim_update >= 20) {
        last_anim_update = currentMillis;
        
        if (xSemaphoreTake(mutex, portMAX_DELAY)) {
            if ((autoCharging || manualCharging) && !isStandby) {
                // Smooth frame update (2 degrees per 20ms = 100 deg/sec)
                static int start_angle = 0;
                start_angle = (start_angle + 2) % 360;
                lv_arc_set_angles(ui_Arc_Battery, start_angle, (start_angle + 60) % 360);
                lv_obj_clear_flag(ui_Arc_Battery, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(ui_Arc_Battery, LV_OBJ_FLAG_HIDDEN);
            }
            xSemaphoreGive(mutex);
        }
    }

    // 2. Robust USB Power Detection (every 200ms)
    if (currentMillis - last_v_update >= 200) {
        last_v_update = currentMillis;

        int32_t raw_volt = panel.getBattVoltage();

        if (filtered_volt == 0) filtered_volt = (float)raw_volt;
        // Light filter for detection responsiveness
        filtered_volt = (filtered_volt * 0.90f) + (raw_volt * 0.10f);

        float v = filtered_volt / 1000.0;
        
        // Calibration Offset (+0.07V) to compensate for load and ADC error
        float v_corr = v + 0.07; 
        
        // Realistic percentage mapping based on corrected voltage
        int32_t percentage;
        if (v_corr >= 4.2) percentage = 100;
        else if (v_corr >= 4.0) percentage = 80 + (v_corr - 4.0) * 100;
        else if (v_corr >= 3.7) percentage = 40 + (v_corr - 3.7) * 133;
        else if (v_corr >= 3.5) percentage = 10 + (v_corr - 3.5) * 150;
        else percentage = (v_corr - 3.3) * 50;
        
        if (percentage < 0) percentage = 0;
        if (percentage > 100) percentage = 100;

        // DEBUG: Print to Serial Monitor
        static unsigned long last_print = 0;
        if (currentMillis - last_print >= 5000) {
            last_print = currentMillis;
            Serial.printf("[BATT] Raw: %dmV, Corr: %.2fV, Perc: %d%%\n", raw_volt, v_corr, percentage);
        }

        // 🔥 ROBUST "PORT-CHECK" LOGIC (Proxy for VBUS)
        float diff = v - last_v;
        last_v = v;

        // Detect Plug-In (Voltage Jump or High Threshold)
        if (diff > 0.008 || v > 4.25) {
            autoCharging = true;
        } 
        // Detect Unplug (Voltage Drop or Low Threshold)
        else if (diff < -0.010 || v < 4.05) {
            autoCharging = false;
        }
        // Special case: Battery is Full (4.2V) and plugged in
        // If voltage stays high and stable near 4.2V, we keep autoCharging TRUE 
        // to show the charging icon, but the animation loop handles the visual.

        if (xSemaphoreTake(mutex, portMAX_DELAY)) {
            lv_obj_clear_flag(ui_Label_BatteryIcon, LV_OBJ_FLAG_HIDDEN);
            
            if (autoCharging || manualCharging) {
                // Charging Icon (Blue Lightning/Symbol)
                lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_CHARGE);
                lv_obj_set_style_text_color(ui_Label_BatteryIcon, lv_color_hex(0x00A0FF), LV_PART_MAIN);
            } else {
                // Static Battery Icon based on percentage
                if (percentage > 80) lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_FULL);
                else if (percentage > 55) lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_3);
                else if (percentage > 30) lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_2);
                else if (percentage > 10) lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_1);
                else lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_EMPTY);

                lv_obj_set_style_text_color(ui_Label_BatteryIcon,
                    percentage > 15 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000),// 15% is red, 16% is green
                    LV_PART_MAIN);
            }
            xSemaphoreGive(mutex);
        }
    }
}
