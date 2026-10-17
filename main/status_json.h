#pragma once
#include "oled_display.h"
#include <cinttypes>
#include <cstdio>
#include <cstddef>

// Hardware-independent serializer: stale snapshots must never claim valid sync.
inline int format_status_json(char *buffer, size_t size, const OledStatus &s,
                              uint64_t age_ms, bool available) {
    const bool stale = !available || age_ms > 5000;
    char satellites[16] = "null";
    if (!stale && s.satellites >= 0)
        std::snprintf(satellites, sizeof(satellites), "%d", s.satellites);
    // utc and ip are internal numeric strings, never supplied by HTTP clients.
    return std::snprintf(buffer, size,
        "{\"dhcp\":%s,\"available\":%s,\"stale\":%s,\"age_ms\":%" PRIu64 ","
        "\"utc\":\"%s\",\"ip\":\"%s\",\"uptime_s\":%lu,"
        "\"stratum\":%u,\"synchronized\":%s,\"gnss_valid\":%s,"
        "\"pps\":%s,\"link\":%s,\"serving\":%s,\"satellites\":%s,\"requests\":%lu}",
        s.dhcp ? "true" : "false", available ? "true" : "false", stale ? "true" : "false", age_ms,
        !stale && s.synchronized ? s.utc : "--:--:--", s.ip,
        (unsigned long)s.uptime, stale ? 16 : unsigned(s.stratum),
        !stale && s.synchronized ? "true" : "false",
        !stale && s.gnss_valid ? "true" : "false",
        !stale && s.pps ? "true" : "false",
        !stale && s.link ? "true" : "false",
        !stale && s.serving ? "true" : "false", satellites, (unsigned long)s.requests);
}
