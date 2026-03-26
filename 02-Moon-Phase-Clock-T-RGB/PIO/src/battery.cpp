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
    // We update the arc rotation independently from the voltage polling
    if (currentMillis - last_anim_update >= 20) {
        last_anim_update = currentMillis;
        
        if (xSemaphoreTake(mutex, portMAX_DELAY)) {
            if ((autoCharging || manualCharging) && !isStandby) {
                // Charging animation active
                static int start_angle = 0;
                start_angle = (start_angle + 2) % 360;
                lv_arc_set_angles(ui_Arc_Battery, start_angle, (start_angle + 60) % 360);
                lv_obj_clear_flag(ui_Arc_Battery, LV_OBJ_FLAG_HIDDEN);
            } else {
                // Not charging or in Standby: hide the arc
                lv_obj_add_flag(ui_Arc_Battery, LV_OBJ_FLAG_HIDDEN);
            }
            xSemaphoreGive(mutex);
        }
    }

    // 2. Voltage & State Update (every 150ms)
    // Slower polling for stability and power efficiency
    if (currentMillis - last_v_update >= 150) {
        last_v_update = currentMillis;

        int32_t raw_volt = panel.getBattVoltage();

        if (filtered_volt == 0) filtered_volt = (float)raw_volt;
        filtered_volt = (filtered_volt * 0.92f) + (raw_volt * 0.08f);

        float v = filtered_volt / 1000.0;
        int32_t volt = (int32_t)filtered_volt;

        // Realistic percentage calculation
        int32_t percentage;
        if (v >= 4.2) percentage = 100;
        else if (v >= 4.0) percentage = 80 + (v - 4.0) * 100;
        else if (v >= 3.7) percentage = 40 + (v - 3.7) * 133;
        else if (v >= 3.5) percentage = 10 + (v - 3.5) * 150;
        else percentage = (v - 3.3) * 50;

        if (percentage < 0) percentage = 0;
        if (percentage > 100) percentage = 100;

        // 🔥 Trend-based charging detection
        float diff = v - last_v;
        last_v = v;

        if (diff > 0.003) autoCharging = true;
        else if (diff < -0.003) autoCharging = false;

        if (xSemaphoreTake(mutex, portMAX_DELAY)) {
            if (autoCharging || manualCharging) {
                lv_obj_clear_flag(ui_Label_BatteryIcon, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_CHARGE);
                lv_obj_set_style_text_color(ui_Label_BatteryIcon, lv_color_hex(0x00A0FF), LV_PART_MAIN);
            } else {
                lv_obj_clear_flag(ui_Label_BatteryIcon, LV_OBJ_FLAG_HIDDEN);

                if (percentage > 80) lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_FULL);
                else if (percentage > 55) lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_3);
                else if (percentage > 30) lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_2);
                else if (percentage > 10) lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_1);
                else lv_label_set_text(ui_Label_BatteryIcon, LV_SYMBOL_BATTERY_EMPTY);

                lv_obj_set_style_text_color(ui_Label_BatteryIcon,
                    percentage > 20 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000),
                    LV_PART_MAIN);
            }
            xSemaphoreGive(mutex);
        }
    }
}
