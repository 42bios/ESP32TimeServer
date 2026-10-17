#include "web_status.h"
#include "web_admin.h"
#include "device_config.h"
#include "status_json.h"
#include "esp_https_server.h"
#include "local_tls_identity.h"
#include <cstring>
#include <cstdio>
#include <cctype>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

namespace {
portMUX_TYPE cache_mux = portMUX_INITIALIZER_UNLOCKED;
OledStatus cached{};
int64_t cached_us = 0;
bool available = false;
httpd_handle_t server = nullptr;
httpd_handle_t redirect_server = nullptr;
const char page[] =
#include "web_page.inc"
;
void headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; img-src 'self' data:; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'");
}
esp_err_t index_handler(httpd_req_t *req) {
    headers(req);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, page, sizeof(page) - 1);
}
// Port 80 has no settings, login or update handlers. Only safe GET redirects.
esp_err_t redirect_handler(httpd_req_t *req) {
    char host[96];
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing host");
    if (char *port = std::strchr(host, ':')) *port = 0;
    if (!host[0]) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid host");
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(host); *p; ++p)
        if (!std::isalnum(*p) && *p != '.' && *p != '-')
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid host");
    const char *path = std::strcmp(req->uri, "/settings") == 0 ? "/settings" :
                       std::strcmp(req->uri, "/api/status") == 0 ? "/api/status" : "/";
    char location[128];
    std::snprintf(location, sizeof(location), "https://%s%s", host, path);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, "Use HTTPS for this device.");
}
void start_redirect() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = 1; config.core_id = 0;
    config.stack_size = 4096; config.max_open_sockets = 2;
    config.max_uri_handlers = 1; config.lru_purge_enable = true;
    config.recv_wait_timeout = 2; config.send_wait_timeout = 2;
    config.uri_match_fn = httpd_uri_match_wildcard;
    if (httpd_start(&redirect_server, &config) != ESP_OK) {
        ESP_LOGW("web", "HTTP redirect unavailable; use HTTPS directly");
        return;
    }
    httpd_uri_t redirect{};
    redirect.uri = "/*"; redirect.method = HTTP_GET; redirect.handler = redirect_handler;
    if (httpd_register_uri_handler(redirect_server, &redirect) != ESP_OK) {
        httpd_stop(redirect_server); redirect_server = nullptr;
    }
}
esp_err_t status_handler(httpd_req_t *req) {
    OledStatus snapshot;
    int64_t updated;
    bool have;
    portENTER_CRITICAL(&cache_mux);
    snapshot = cached; updated = cached_us; have = available;
    portEXIT_CRITICAL(&cache_mux);
    const int64_t elapsed = esp_timer_get_time() - updated;
    char json[768];
    const int length = format_status_json(json, sizeof(json), snapshot,
                                         elapsed > 0 ? uint64_t(elapsed / 1000) : 0, have);
    if (length < 0 || size_t(length) >= sizeof(json))
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Status unavailable");
    headers(req);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, length);
}
}
void web_status_publish(const OledStatus &status, int64_t monotonic_us) {
    portENTER_CRITICAL(&cache_mux);
    cached = status; cached_us = monotonic_us; available = true;
    portEXIT_CRITICAL(&cache_mux);
}
void web_status_start() {
    httpd_ssl_config_t tls = HTTPD_SSL_CONFIG_DEFAULT();
    tls.httpd.task_priority = 1;
    tls.httpd.core_id = 0;
    tls.httpd.stack_size = 16384;
    tls.httpd.max_open_sockets = 3;
    tls.httpd.max_uri_handlers = 12;
    tls.httpd.lru_purge_enable = true;
    tls.httpd.recv_wait_timeout = 5;
    tls.httpd.send_wait_timeout = 5;
    tls.servercert = reinterpret_cast<const uint8_t *>(local_tls_cert);
    tls.servercert_len = sizeof(local_tls_cert);
    tls.prvtkey_pem = reinterpret_cast<const uint8_t *>(local_tls_key);
    tls.prvtkey_len = sizeof(local_tls_key);
    tls.tls_handshake_timeout_ms = 5000;
    tls.tls_version = ESP_TLS_VER_TLS_1_2;
    if (httpd_ssl_start(&server, &tls) != ESP_OK) {
        ESP_LOGE("web", "Status page unavailable; NTP startup continues");
        return;
    }
    httpd_uri_t index{};
    index.uri = "/"; index.method = HTTP_GET; index.handler = index_handler;
    httpd_uri_t status{};
    status.uri = "/api/status"; status.method = HTTP_GET; status.handler = status_handler;
    if (httpd_register_uri_handler(server, &index) != ESP_OK ||
        httpd_register_uri_handler(server, &status) != ESP_OK) {
        httpd_ssl_stop(server); server = nullptr;
        ESP_LOGE("web", "Unable to register status routes");
        return;
    }
    web_admin_register(server);
    device_web_ready();
    start_redirect();
    ESP_LOGI("web", "Status and protected settings on HTTPS port 443; HTTP redirects only");
}
