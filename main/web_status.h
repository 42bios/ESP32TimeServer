#pragma once
#include "oled_display.h"
#include <cstdint>

// The OLED owner publishes snapshots; HTTP never reads timing/network mutexes.
void web_status_publish(const OledStatus &status, int64_t monotonic_us);
void web_status_start();
