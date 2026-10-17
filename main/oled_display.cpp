#include "oled_display.h"
#include "web_status.h"
#include "device_config.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static_assert(sizeof(u8g2_uint_t) >= 2, "SH1122 requires 16-bit coordinates");

namespace {
U8G2_SH1122_256X64_F_4W_SW_SPI panel(U8G2_R2, 14, 15, 18, 17, 16);
portMUX_TYPE boot_mux = portMUX_INITIALIZER_UNLOCKED;
char boot_lines[2][32] = {"Starting NTP server", "Waiting for GNSS"};


void display_task(void *) {
    panel.begin();
    panel.setContrast(device_display_brightness(false));
    uint8_t previous_state = 255;
    int64_t changed_us = esp_timer_get_time();
    uint8_t previous_contrast = device_display_brightness(false);
    unsigned log_tick = 0;
    for (;;) {
        const auto s = read_oled_status();
        web_status_publish(s, esp_timer_get_time());
        const uint8_t state = (s.synchronized ? 1 : 0) | (s.pps ? 2 : 0) |
                              (s.link ? 4 : 0) | (s.gnss_valid ? 8 : 0) |
                              (s.ip[0] ? 16 : 0);
        const int64_t now_us = esp_timer_get_time();
        if (state != previous_state) { previous_state = state; changed_us = now_us; }
        const uint32_t dim_after = device_display_dim_seconds();
        const uint8_t contrast = device_display_brightness(dim_after && now_us - changed_us >= int64_t(dim_after) * 1000000LL);
        if (contrast != previous_contrast) { panel.setContrast(contrast); previous_contrast = contrast; }
        panel.clearBuffer();
        panel.setFont(u8g2_font_5x7_tf);
        panel.drawStr(3, 8, "UTC");
        panel.drawStr(35, 8, s.synchronized ? "GNSS / PPS LOCKED" : "NOT SYNCHRONIZED");
        panel.setFont(u8g2_font_logisoso24_tn);
        panel.drawStr(3, 38, s.utc);
        panel.drawVLine(169, 0, 42);
        panel.setFont(u8g2_font_6x10_tf);
        char text[64];
        std::snprintf(text, sizeof(text), "STRATUM %u", unsigned(s.stratum));
        panel.drawStr(175, 10, text);
        panel.setFont(u8g2_font_5x7_tf);
        panel.drawStr(175, 20, s.serving ? "NTP READY" : "NTP WAIT");
        panel.drawStr(175, 30, s.link ? "ETH LINK UP" : "ETH NO LINK");
        std::snprintf(text, sizeof(text), "REQ %lu", (unsigned long)s.requests);
        panel.drawStr(175, 40, text);
        panel.drawHLine(0, 44, 256);
        if (s.ip[0]) std::snprintf(text, sizeof(text), "IP %s", s.ip);
        else std::snprintf(text, sizeof(text), "%s", s.link ? "DHCP: WAITING FOR ADDRESS" : "CONNECT ETHERNET");
        panel.drawStr(3, 53, text);
        if (s.satellites >= 0) std::snprintf(text, sizeof(text), "GNSS %s SAT %02d", s.gnss_valid ? "OK" : "WAIT", s.satellites);
        else std::snprintf(text, sizeof(text), "GNSS WAIT SAT --");
        panel.drawStr(3, 63, text);
        panel.drawStr(130, 63, s.pps ? "PPS OK" : "PPS WAIT");
        std::snprintf(text, sizeof(text), "UP %luh", (unsigned long)(s.uptime / 3600));
        panel.drawStr(194, 63, text);
        if (!s.synchronized && !s.serving) {
            char lines[2][32];
            portENTER_CRITICAL(&boot_mux);
            std::memcpy(lines, boot_lines, sizeof(lines));
            portEXIT_CRITICAL(&boot_mux);
            panel.setDrawColor(0); panel.drawBox(0, 12, 168, 30); panel.setDrawColor(1);
            panel.setFont(u8g2_font_5x7_tf);
            panel.drawStr(3, 24, lines[0]); panel.drawStr(3, 36, lines[1]);
        }
        panel.sendBuffer();
        // Low-rate commissioning/status log, never in PPS or NTP callbacks.
        if (++log_tick >= 10) {
            log_tick = 0;
            ESP_LOGI("ntp_status", "UTC=%s stratum=%u GNSS=%u SAT=%d PPS=%u ETH=%u IP=%s req=%lu",
                     s.utc, s.stratum, s.gnss_valid, s.satellites, s.pps, s.link,
                     s.ip[0] ? s.ip : "none", (unsigned long)s.requests);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
}
void oled_boot_line(uint8_t row, const char *text) {
    if (row != 1 && row != 2) return;
    char line[32];
    std::snprintf(line, sizeof(line), "%s", text ? text : "");
    portENTER_CRITICAL(&boot_mux);
    std::memcpy(boot_lines[row - 1], line, sizeof(line));
    portEXIT_CRITICAL(&boot_mux);
}
void oled_start() {
    if (xTaskCreatePinnedToCore(display_task, "oled_status", 6144, nullptr, 1, nullptr, 0) != pdPASS)
        ESP_LOGE("oled", "Display task unavailable; NTP startup continues");
}
