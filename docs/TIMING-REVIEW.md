# Timing review and measurement plan

Reviewed 18 October 2026. The running firmware is kept unchanged during the baseline.

## Confirmed code properties

- PPS uses MCPWM capture, but `pps_capture_callback` ignores `cap_value` and records
  `esp_timer_get_time()` when the callback executes. Hardware edge capture and ISR
  execution time are different quantities. Callback latency can enter the clock servo.
- External NTP receives use Ethernet hardware RX timestamps when available. The
  transmit field is generated using `gettimeofday()` before hashing, frame copying
  and transmission. A hardware TX completion timestamp is not used for that field.
- `synchronize_hardware_clock` copies system time into the Ethernet PHC with
  `clock_settime(CLOCK_PTP_SYSTEM, ...)`. System time is also slewed with `adjtime`.
  RX and TX timestamp domains/alignment need verification through a PPS cycle.
- The packet handoff uses a preallocated buffer pool and nonblocking queue access.
  No explicit per-request `new`/`malloc` was found in these application handlers;
  driver and crypto internals have not been profiled.
- Client cache and transmit locks use `portMAX_DELAY`. Cache scans are bounded to
  128 entries per address family, but contention/hold time have not been measured.
- Hardware NTP runs on Core 1 at priority 17; HTTPS and OLED run on Core 0 at
  priority 1. This separates ordinary work but does not eliminate shared crypto,
  memory, network-driver or interrupt contention.

These findings are not proof of absolute error. Do not replace the clock servo,
raise task priorities or remove synchronization before measuring the consequences.

## Controlled comparisons

`commissioning/benchmark_timing.py` alternates:

1. Ordinary NTPv3/v4 without injected HTTPS load.
2. Ordinary NTP with one new HTTPS connection per second.
3. Authenticated NTPv4 without injected HTTPS load.
4. Authenticated NTPv4 with HTTPS load.
5. Ordinary NTP recovery phase.

The order of the middle load phases reverses in alternate cycles. NTP is sampled
at two requests per second. HTTPS certificate verification remains enabled; the
benchmark never changes settings or uploads firmware. Keys and raw credentials
are not written to measurements.

The first Mac smoke run used Wi-Fi: all 305 NTP samples passed and there were no
HTTPS errors. WLAN delay variation makes that run unsuitable for judging small
firmware timing improvements. The Docker host provides wired Ethernet via a
router and avoids that Wi-Fi segment; the host's synchronized clock is still not
an independent calibrated UTC reference.

Results include RTT, the apparent offset against the client clock, reported
server timestamp interval, percentiles, rejected packets and synchronization
status. `t3-t2` is not an independently measured CPU execution time. Client prepare
time and host scheduling delay are recorded. Clock steps during an exchange are
flagged; slower changes and steps between exchanges remain a limitation.

## Next implementation priorities

1. Measure PPS callback latency/intervals and system-to-PHC alignment using bounded
   counters sampled outside the interrupt, without logging every packet or pulse.
2. Use captured edge ticks only after establishing their mapping to the clock used
   by the servo, including wrap, clock rate and missed-pulse handling.
3. Evaluate hardware TX timestamping and interleaved timestamps against an actual
   wired client and an independent PPS/time reference. Do not substitute host RTT
   measurements for absolute UTC accuracy.
4. Optimize only demonstrated contention/cost, then repeat the same load sequence.

Reference APIs: [Espressif MCPWM capture](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/mcpwm.html)
and [Ethernet timestamping](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/network/esp_eth.html).
