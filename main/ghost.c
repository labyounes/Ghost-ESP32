#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "string.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

// UART2 configuration — connected to external device via GPIO16/17
#define UART_PORT      UART_NUM_2
#define UART_TX_PIN    17
#define UART_RX_PIN    16
#define UART_BAUD_RATE 115200
#define BUF_SIZE       1024

// Initialize UART2 with 8N1 configuration
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

// Send a string over UART2
void send(const char* msg) {
    uart_write_bytes(UART_PORT, msg, strlen(msg));
}

// Initialize WiFi in station mode (no connection, scanning only)
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

// Scan for nearby WiFi access points and send results over UART and serial monitor
void scan_ap() {
    send("SCAN:START\n");
    printf("SCAN:START\n");

    // Scan all channels, show hidden networks
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
    };
    esp_wifi_scan_start(&scan_cfg, true);

    // Get number of found networks
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);

    char countbuf[32];
    snprintf(countbuf, sizeof(countbuf), "Found %d networks\n", count);
    send(countbuf);
    printf("%s", countbuf);

    // Retrieve and send each network: SSID, RSSI, channel
    wifi_ap_record_t *list = malloc(count * sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&count, list);

    char buf[128];
    for (int i = 0; i < count; i++) {
        snprintf(buf, sizeof(buf), "AP:%s,%d,%d\n",
                 (char*)list[i].ssid,
                 list[i].rssi,
                 list[i].primary);
        send(buf);
        printf("%s", buf);
    }
    free(list);

    send("SCAN:DONE\n");
    printf("SCAN:DONE\n");
}

// Parse and handle incoming UART commands
void handle_command(char* cmd) {
    // Strip newlines and carriage returns
    int len = strlen(cmd);
    for (int i = 0; i < len; i++) {
        if (cmd[i] == '\n' || cmd[i] == '\r') {
            cmd[i] = 0;
        }
    }
    printf("Handling: [%s]\n", cmd);

    if (strncmp(cmd, "CMD:SCAN_AP", 11) == 0) {
        scan_ap();
    } else {
        printf("Unknown: [%s]\n", cmd);
        send("ERR:UNKNOWN\n");
    }
}

// Background task that continuously listens for incoming UART commands
void uart_rx_task(void *arg) {
    uint8_t data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_PORT, data, BUF_SIZE - 1,
                                  20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = 0;
            printf("CMD: %s\n", (char*)data);
            handle_command((char*)data);
        }
    }
}

void app_main(void) {
    printf("Ghost booting...\n");

    uart_init();   // Set up UART2 for communication
    wifi_init();   // Start WiFi in station mode

    send("GHOST:READY\n");

    // Start UART listener task
    xTaskCreate(uart_rx_task, "uart_rx", 8192, NULL, 5, NULL);
    printf("Ghost ready.\n");

    // Auto scan on boot
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    scan_ap();
}