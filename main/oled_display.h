#pragma once
#include <cstdint>

struct OledStatus {
    char utc[9] = "--:--:--";
    char ip[46] = "";
    uint32_t requests = 0;
    uint32_t uptime = 0;
    uint8_t stratum = 16;
    int satellites = -1;
    bool synchronized = false;
    bool gnss_valid = false;
    bool pps = false;
    bool link = false;
    bool serving = false;
    bool dhcp = true;
};

// Implemented beside the original state: no receiver access from the OLED task.
OledStatus read_oled_status();
void oled_start();
void oled_boot_line(uint8_t row, const char *text);
