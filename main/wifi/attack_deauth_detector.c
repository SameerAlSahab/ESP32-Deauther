/**
 * @file attack_deauth_detector.c
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 8-5-2026
 * @copyright Copyright (c) 2026
 *
 * @brief 802.11 Deauthentication & Disassociation frame detector for ESP32.
 * IMPORTANT: Monitors management frames in promiscuous mode to identify
 * potential DoS attacks targeting specific BSSIDs.
 */

#include "attack_deauth_detector.h"
#include <string.h>
#include <stdbool.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_timer.h"

static const char *TAG = "deauth_detector";

#define ALERT_HOLD_MS 5000

typedef struct {
    uint8_t  frame_ctrl[2];
    uint16_t duration;
    uint8_t  addr1[6];
    uint8_t  addr2[6];
    uint8_t  addr3[6];
} __attribute__((packed)) wifi_80211_hdr_t;

static deauth_detector_status_t detector_status = { 0 };

static bool is_zero_mac(const uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0x00) return false;
    }
    return true;
}

static deauth_track_entry_t *get_or_create_entry(const uint8_t *bssid) {
    for (int i = 0; i < detector_status.count; i++) {
        if (memcmp(detector_status.entries[i].bssid, bssid, 6) == 0) {
            return &detector_status.entries[i];
        }
    }

    if (detector_status.count >= MAX_TRACKED_BSSIDS) {
        return &detector_status.entries[0];
    }

    deauth_track_entry_t *e = &detector_status.entries[detector_status.count++];
    memset(e, 0, sizeof(*e));

    memcpy(e->bssid, bssid, 6);
    e->window_start_ms = esp_timer_get_time() / 1000;

    return e;
}

static void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *) buf;
    const wifi_80211_hdr_t       *hdr = (wifi_80211_hdr_t *) pkt->payload;

    if (is_zero_mac(hdr->addr3)) return;

    uint8_t frame_type    = hdr->frame_ctrl[0] & 0x0C;
    uint8_t frame_subtype = hdr->frame_ctrl[0] & 0xF0;

    if (frame_type != 0x00 || frame_subtype != 0xC0) return;

    int64_t now_ms = esp_timer_get_time() / 1000;
    deauth_track_entry_t *entry = get_or_create_entry(hdr->addr3);

    if ((now_ms - entry->window_start_ms) > DEAUTH_WINDOW_MS) {
        entry->count = 0;
        entry->window_start_ms = now_ms;
    }

    entry->count++;

    if (entry->count >= DEAUTH_THRESHOLD) {
        entry->alerting = true;
        entry->last_alert_ms = now_ms;

        ESP_LOGW(TAG,
                 "DEAUTH DETECTED: BSSID %02X:%02X:%02X:%02X:%02X:%02X [%d frames]",
                 entry->bssid[0], entry->bssid[1], entry->bssid[2],
                 entry->bssid[3], entry->bssid[4], entry->bssid[5],
                 entry->count);
    }
}

const deauth_detector_status_t *deauth_detector_get_status() {
    int64_t now_ms = esp_timer_get_time() / 1000;

    for (int i = 0; i < detector_status.count; i++) {
        deauth_track_entry_t *e = &detector_status.entries[i];

        if (e->alerting && (now_ms - e->last_alert_ms > ALERT_HOLD_MS)) {
            e->alerting = false;
        }
    }

    return &detector_status;
}

void deauth_detector_start() {
    memset(&detector_status, 0, sizeof(detector_status));
    detector_status.running = true;

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT
    };
    esp_wifi_set_promiscuous_filter(&filter);

    ESP_LOGI(TAG, "Deauth detector started.");
}

void deauth_detector_stop() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);
    detector_status.running = false;

    ESP_LOGI(TAG, "Deauth detector stopped.");
}



