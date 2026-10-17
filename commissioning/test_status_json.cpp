#include "../main/status_json.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
    OledStatus s{};
    std::snprintf(s.utc, sizeof(s.utc), "14:32:09");
    std::snprintf(s.ip, sizeof(s.ip), "192.168.30.146");
    s.synchronized = s.gnss_valid = s.pps = s.link = s.serving = true;
    s.stratum = 1; s.satellites = 7; s.requests = UINT32_MAX; s.uptime = 86462;
    char json[768];
    assert(format_status_json(json, sizeof(json), s, 5000, true) > 0);
    assert(std::strstr(json, "\"synchronized\":true"));
    assert(std::strstr(json, "\"stratum\":1,"));
    assert(std::strstr(json, "4294967295"));
    std::puts(json); // Browser fixture is explicitly simulated, not device data.
    format_status_json(json, sizeof(json), s, 5001, true);
    assert(std::strstr(json, "\"stale\":true"));
    assert(std::strstr(json, "\"synchronized\":false"));
    assert(std::strstr(json, "\"stratum\":16,"));
    assert(std::strstr(json, "\"pps\":false"));
    assert(std::strstr(json, "\"satellites\":null"));
    assert(std::strstr(json, "--:--:--"));
    format_status_json(json, sizeof(json), s, 0, false);
    assert(std::strstr(json, "\"available\":false"));
    assert(std::strstr(json, "\"synchronized\":false"));
    s.satellites = -1;
    format_status_json(json, sizeof(json), s, 0, true);
    assert(std::strstr(json, "\"satellites\":null"));
    char tiny[4];
    assert(format_status_json(tiny, sizeof(tiny), s, 0, true) >= int(sizeof(tiny)));
}
