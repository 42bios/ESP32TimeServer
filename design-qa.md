# Device-themed web interface QA

final result: passed

Reviewed 17 October 2026 against the user's photograph of the assembled black,
white and cyan rack module. This is a visual adaptation of a physical enclosure,
not a pixel-for-pixel webpage recreation. Status and settings share the same
embedded theme and source-derived satellite asset.

## Visual evidence

- Source: supplied assembled-device photograph; front artwork from V24/V25 CAD.
- Implementation: [device status capture](docs/images/web-status.png), served by
  the actual ESP after an Ethernet firmware update.
- A side-by-side source/implementation comparison was inspected locally.
- Default desktop view: 808 × 785 pixels. The frame, logo, white/cyan heading,
  dark display bezel, readable UTC digits and aligned status cells fit the viewport.
- Mobile status and authenticated settings checked at 375 × 812 CSS pixels.
  Document width remained 375 pixels; no horizontal overflow. Fields stack on
  mobile, navigation remains visible, and controls retain readable labels.
- Focused settings inspection covered login, network fields, OLED values and
  action buttons. No clipped labels, overlapping controls or missing logo.
- Logo loaded on the device with the image content policy enabled. No browser
  warning or error logs were reported during the final device-page check.

## Interaction and firmware checks

- Status/settings navigation and administrator login/logout work in the browser.
- DHCP toggle enables/disables static fields; returning to DHCP and saving the
  original values succeeded. Brightness remains 100, dimmed brightness 80,
  dim delay 60 seconds. No password change was made.
- English copy includes the former untranslated status labels and accessibility
  labels. Synchronizing, unavailable and stale-data states remain supported.
  The stale state was observed while the device restarted during OTA.
- JavaScript syntax checks and ESP-IDF v6.1 firmware build passed.
- Firmware update over Ethernet succeeded; GNSS/PPS synchronization returned.
- Eight subsequent NTPv3/v4 requests passed with stratum 1, leap indicator 0 and
  GPS reference. This is a functional regression check, not absolute timing QA.

No actionable P0/P1/P2 visual issues remained. Physical surface texture and the
printed font are intentionally represented by clean screen typography rather
than a raster photograph. No external fonts, image services or CDN requests.

## HTTPS transport follow-up

The device now serves the same interface over native TLS 1.2, with a unique local
certificate. Certificate/IP verification, authenticated management and firmware
update over HTTPS passed. HTTP only redirects GET requests; plaintext login and
update POST requests are unavailable. The settings transport notice now identifies
the local HTTPS certificate. Browser trust is a manual user step.
