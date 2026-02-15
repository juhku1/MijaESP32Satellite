#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#if __has_include("mdns.h")
#include "mdns.h"
#define HAVE_MDNS 1
#else
#define HAVE_MDNS 0
#endif
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"

static const char *TAG = "MAIN";

#define WIFI_SSID "Kontu"
#define WIFI_PASS "8765432A1"
#define DEFAULT_SERVER_URL "http://192.168.68.144/api/satellite-data"
#define DISCOVERY_PORT 19798
#define DISCOVERY_PREFIX "SATMASTER"
#define MDNS_HOSTNAME "ble-master"
#define MDNS_QUERY_INTERVAL_MS 5000

static bool wifi_connected = false;
static char server_url[128] = DEFAULT_SERVER_URL;
static bool server_url_set = false;
#if HAVE_MDNS
static bool mdns_started = false;
#endif

static void update_server_url(const char *ip, int port, const char *source) {
    if (!ip || ip[0] == '\0') {
        return;
    }
    if (port <= 0 || port > 65535) {
        port = 80;
    }

    char new_url[128];
    snprintf(new_url, sizeof(new_url), "http://%s:%d/api/satellite-data", ip, port);
    if (strcmp(new_url, server_url) != 0) {
        snprintf(server_url, sizeof(server_url), "%s", new_url);
        server_url_set = true;
        ESP_LOGI(TAG, "%s: server URL -> %s", source, server_url);
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        wifi_connected = true;
        ESP_LOGI(TAG, "✓ WiFi connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
#if HAVE_MDNS
        if (!mdns_started) {
            esp_err_t err = mdns_init();
            if (err == ESP_OK) {
                mdns_hostname_set("ble-satellite");
                mdns_instance_name_set("BLE Satellite");
                mdns_started = true;
                ESP_LOGI(TAG, "mDNS client started");
            } else {
                ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
            }
        }
#endif
    }
}

static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialized, connecting to %s", WIFI_SSID);
}

static void json_escape_string(const char *in, char *out, size_t out_size) {
    if (!in || !out || out_size == 0) return;
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < out_size; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\';
            out[j++] = (char)c;
        } else if (c >= 0x20 && c < 0x7F) {
            out[j++] = (char)c;
        } else {
            if (j + 2 >= out_size) break;
            out[j++] = '\\';
            out[j++] = 'u';
            if (j + 4 >= out_size) break;
            out[j++] = '0';
            out[j++] = '0';
            out[j++] = '0';
            out[j++] = '0';
        }
    }
    out[j] = '\0';
}

static void send_ble_data(const char *mac, int8_t rssi, const uint8_t *data, uint8_t len, const char *name) {
    if (!wifi_connected) return;
    
    char payload[512];
    char hex_data[256] = {0};
    char name_escaped[96] = {0};
    
    // Muunna data heksaksi
    for (int i = 0; i < len && i < 100; i++) {
        sprintf(hex_data + (i * 2), "%02X", data[i]);
    }
    
    if (name && name[0] != '\0') {
        json_escape_string(name, name_escaped, sizeof(name_escaped));
        snprintf(payload, sizeof(payload),
            "{\"mac\":\"%s\",\"rssi\":%d,\"data\":\"%s\",\"name\":\"%s\"}",
            mac, rssi, hex_data, name_escaped);
    } else {
        snprintf(payload, sizeof(payload),
            "{\"mac\":\"%s\",\"rssi\":%d,\"data\":\"%s\"}",
            mac, rssi, hex_data);
    }
    
    esp_http_client_config_t config = {
        .url = server_url,
        .method = HTTP_METHOD_POST,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "📡 Sent: %s (RSSI: %d) -> %d bytes", mac, rssi, len);
    } else {
        ESP_LOGW(TAG, "❌ HTTP POST failed (%s): %s", server_url, esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
}

static bool has_payload_content(const uint8_t *data, uint8_t len) {
    if (data == NULL || len == 0) return false;
    uint8_t i = 0;
    while (i + 1 < len) {
        uint8_t field_len = data[i];
        if (field_len == 0) break;
        if (i + field_len >= len + 1) {
            break;
        }
        uint8_t type = data[i + 1];
        // Data-bearing types: Service Data (16/32/128-bit) or Manufacturer Data
        if (type == 0x16 || type == 0x20 || type == 0x21 || type == 0xFF) {
            return true;
        }
        // Name fields: Shortened/Complete Local Name
        if (type == 0x08 || type == 0x09) {
            return true;
        }
        i += field_len + 1;
    }
    return false;
}

static void discovery_listen_task(void *param) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Discovery socket create failed");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(DISCOVERY_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Discovery bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Discovery listening on UDP %d", DISCOVERY_PORT);

    char rx[128];
    while (1) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx, sizeof(rx) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        if (len < 0) {
            ESP_LOGW(TAG, "Discovery recv failed");
            continue;
        }
        rx[len] = '\0';
        
        ESP_LOGI(TAG, "📡 Discovery received: %s", rx);

        if (strncmp(rx, DISCOVERY_PREFIX, strlen(DISCOVERY_PREFIX)) == 0) {
            char ip[16] = {0};
            int port = 80;
            if (sscanf(rx + strlen(DISCOVERY_PREFIX), " %15s %d", ip, &port) >= 1) {
                update_server_url(ip, port, "Discovery");
            }
        } else {
            ESP_LOGW(TAG, "Discovery message format unknown");
        }
    }
}

#if HAVE_MDNS
static void mdns_query_task(void *param) {
    while (1) {
        if (wifi_connected) {
            if (!mdns_started) {
                esp_err_t err = mdns_init();
                if (err == ESP_OK) {
                    mdns_hostname_set("ble-satellite");
                    mdns_instance_name_set("BLE Satellite");
                    mdns_started = true;
                    ESP_LOGI(TAG, "mDNS client started");
                } else {
                    ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
                }
            }
            if (mdns_started) {
                ip4_addr_t addr;
                esp_err_t err = mdns_query_a(MDNS_HOSTNAME, 2000, &addr);
                if (err == ESP_OK) {
                    char ip[16];
                    ip4addr_ntoa_r(&addr, ip, sizeof(ip));
                    update_server_url(ip, 80, "mDNS");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(MDNS_QUERY_INTERVAL_MS));
    }
}
#endif

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    if (event->type == BLE_GAP_EVENT_DISC) {
        char mac[18];
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                event->disc.addr.val[0], event->disc.addr.val[1],
                event->disc.addr.val[2], event->disc.addr.val[3],
                event->disc.addr.val[4], event->disc.addr.val[5]);

        char adv_name[32] = {0};
        struct ble_hs_adv_fields fields;
        int parse_res = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        if (parse_res == 0 && fields.name != NULL && fields.name_len > 0) {
            int copy_len = (fields.name_len < (int)sizeof(adv_name) - 1) ? fields.name_len : (int)sizeof(adv_name) - 1;
            memcpy(adv_name, fields.name, copy_len);
            adv_name[copy_len] = '\0';
        }

        if (has_payload_content(event->disc.data, event->disc.length_data)) {
            send_ble_data(mac, event->disc.rssi,
                         event->disc.data, event->disc.length_data,
                         adv_name[0] != '\0' ? adv_name : NULL);
        }
    }
    return 0;
}

static void ble_scan_task(void *param) {
    ESP_LOGI(TAG, "BLE scan started");
    
    struct ble_gap_disc_params disc_params = {
        .itvl = 0x10,
        .window = 0x10,
        .filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
        .limited = 0,
        .passive = 0, // Active scan, jotta saadaan scan response (nimi)
        .filter_duplicates = 0,
    };
    
    while (1) {
        ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void ble_host_task(void *param) {
    nimble_port_run();
}

static void ble_on_sync(void) {
    ESP_LOGI(TAG, "BLE stack synced");
    xTaskCreate(ble_scan_task, "ble_scan", 4096, NULL, 5, NULL);
}

static void ble_init(void) {
    nimble_port_init();
    ble_hs_cfg.sync_cb = ble_on_sync;
    nimble_port_freertos_init(ble_host_task);
    ESP_LOGI(TAG, "BLE initialized");
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-C3 BLE Scanner started");
    
    // NVS init
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // WiFi init
    wifi_init();

    // Discovery listener
    xTaskCreate(discovery_listen_task, "discovery_listen", 4096, NULL, 4, NULL);
#if HAVE_MDNS
    xTaskCreate(mdns_query_task, "mdns_query", 4096, NULL, 4, NULL);
#endif
    
    // Wait for WiFi
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // BLE init
    ble_init();
    
    int counter = 0;
    while (1) {
        ESP_LOGI(TAG, "Running: %d, WiFi: %s, Server: %s", counter++,
                 wifi_connected ? "connected" : "disconnected",
                 server_url_set ? server_url : "(default)");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
