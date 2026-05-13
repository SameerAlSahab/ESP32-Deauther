/**
 * @file attack.c
 * @author risinek (risinek@gmail.com), SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 8-5-2026
 * @copyright Copyright (c) 2026
 *
 * @brief Implements the central attack wrapper and lifecycle management.
 * Handles attack requests, timeouts, and status updates for all modules.
 */

#include "attack.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_timer.h"

#include "webserver.h"
#include "wifi_controller.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "attack_probe.h"
#include "attack_eviltwin.h"
#include "bt_payload_attack.h"
#include "management_helper.h"
#include "attack_pmkid.h"
#include "attack_handshake.h"
#include "attack_dos.h"
#include "attack_method.h"
#include "attack_beacon_spam.h"
#include "attack_bt_spam.h"
#include "hydra_ssd1306_display.h"

static const char* TAG = "attack";
static attack_status_t attack_status = { .state = READY, .type = -1, .content_size = 0, .content = NULL };
static esp_timer_handle_t attack_timeout_handle;

const attack_status_t *attack_get_status() {
    return &attack_status;
}

void attack_update_status(attack_state_t state) {
    attack_status.state = state;
    if (state == FINISHED || state == TIMEOUT) {
        if (esp_timer_is_active(attack_timeout_handle)) {
            esp_timer_stop(attack_timeout_handle);
        }
        hydra_display_set_attack_timeout(0);
    }
}

void attack_append_status_content(uint8_t *buffer, unsigned size){
    if(size == 0) return;
    char *reallocated_content = realloc(attack_status.content, attack_status.content_size + size);
    if(reallocated_content == NULL) return;
    memcpy(&reallocated_content[attack_status.content_size], buffer, size);
    attack_status.content = reallocated_content;
    attack_status.content_size += size;
}

char *attack_alloc_result_content(unsigned size) {
    attack_status.content_size = size;
    attack_status.content = (char *) malloc(size);
    return attack_status.content;
}


static void attack_timeout(void* arg){
    ESP_LOGD(TAG, "Attack timed out");
    attack_update_status(TIMEOUT);

    switch(attack_status.type) {
        case ATTACK_TYPE_PMKID:
            attack_pmkid_stop();
            break;
        case ATTACK_TYPE_HANDSHAKE:
            attack_handshake_stop();
            break;
        case ATTACK_TYPE_DOS:
            attack_dos_stop();
            break;
        case ATTACK_TYPE_BEACON_SPAM:
            attack_beacon_spam_stop();
            break;
        case ATTACK_TYPE_PROBE:
            attack_probe_stop();
            wifictl_mgmt_ap_start();
            break;
        case ATTACK_TYPE_EVIL_TWIN:
            restore_management_system();
            break;
        case ATTACK_TYPE_BT_SPAM:
            ESP_LOGI(TAG, "Abort BT SPAM attack...");
            attack_bt_spam_stop();
            break;
        case ATTACK_TYPE_CLONE:
            attack_method_super_clone_stop();
            wifictl_mgmt_ap_start();
            wifictl_restore_ap_mac();
            break;
        case ATTACK_TYPE_BT_PAYLOAD:
            bt_payload_attack_stop();
            break;
        default:
            ESP_LOGE(TAG, "Unknown attack type. Cleanup skipped.");
    }
}


static void attack_request_handler(void *args, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    attack_request_t *attack_request = (attack_request_t *) event_data;

    bool needs_ap = (attack_request->type != ATTACK_TYPE_BEACON_SPAM) &&
    (attack_request->type != ATTACK_TYPE_BT_SPAM) &&
    (attack_request->type != ATTACK_TYPE_PROBE) &&
    (attack_request->type != ATTACK_TYPE_BT_PAYLOAD);

    if (needs_ap) {
        if (attack_request->ap_count == 0 || attack_request->ap_count > MAX_ATTACK_TARGETS) {
            ESP_LOGE(TAG, "Invalid ap_count: %d", attack_request->ap_count);
            return;
        }
    }

    attack_config_t attack_config = {
        .type         = attack_request->type,
        .method       = attack_request->method,
        .timeout      = attack_request->timeout,
        .target_count = 0
    };

    for (int i = 0; i < attack_request->ap_count; i++) {
        const wifi_ap_record_t *rec = wifictl_get_ap_record(attack_request->ap_record_ids[i]);
        if (rec != NULL) {
            attack_config.ap_records[attack_config.target_count++] = rec;
        }
    }

    attack_status.state = RUNNING;
    attack_status.type  = attack_config.type;

    if ((attack_config.timeout > 0) && (attack_config.type != ATTACK_TYPE_BT_SPAM) && (attack_config.type != ATTACK_TYPE_EVIL_TWIN)) {
        ESP_ERROR_CHECK(esp_timer_start_once(attack_timeout_handle, (uint64_t)attack_config.timeout * 1000000));
    }

    hydra_display_set_attack_timeout(attack_config.timeout > 0 ? attack_config.timeout : 0);


    switch (attack_config.type) {
        case ATTACK_TYPE_PMKID:
            attack_pmkid_start(&attack_config);
            break;
        case ATTACK_TYPE_HANDSHAKE:
            attack_handshake_start(&attack_config);
            break;
        case ATTACK_TYPE_DOS:
            attack_dos_start(&attack_config);
            break;
        case ATTACK_TYPE_BEACON_SPAM: {
            uint8_t spam_count = attack_config.method;                              // byte 1 = count
            beacon_spam_mode_t mode = (beacon_spam_mode_t)attack_request->ap_count; // byte 4 = mode (repurposed)
            attack_beacon_spam_start(spam_count, mode);
            break;
        }
        case ATTACK_TYPE_PROBE:
            wifictl_mgmt_ap_stop();
            attack_probe_start(&attack_config);
            break;
        case ATTACK_TYPE_EVIL_TWIN:
            attack_method_evil_twin(attack_config.ap_records[0]);
            break;
        case ATTACK_TYPE_BT_SPAM: {

            // Initialize BLE spam system
            attack_bt_spam_init();

            // Convert method into device type
            // Valid range: 1-25
            int total_targets = 25;
            int selected_type = (attack_config.method % total_targets) + 1;

            // New config structure
            bt_spam_config_t cfg = {
                .device_type = selected_type,

                // Legacy field (unused now)
                .delay_seconds = 0,

                // Extra delay between bursts
                // 0 = maximum speed
                // 10-20 = stable fast mode
                // 50+ = slower
                .delay_ms = 10,

                // Reserved currently
                .adv_type = 2
            };

            attack_bt_spam_start(&cfg);

            break;
        }
        case ATTACK_TYPE_CLONE:
            wifictl_mgmt_ap_stop();
            if (attack_config.target_count > 0 && attack_config.ap_records[0] != NULL) {
                attack_method_super_clone(attack_config.ap_records[0]);
            } else {
                attack_update_status(FINISHED);
            }
            break;
        case ATTACK_TYPE_BT_PAYLOAD:
            bt_payload_attack_init();
            bt_payload_attack_start(attack_config.method > 0 ? attack_config.method : 1);
            break;
        default:
            ESP_LOGE(TAG, "Unknown attack type request.");
    }
}


static void attack_reset_handler(void *args, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if(attack_status.content){
        free(attack_status.content);
        attack_status.content = NULL;
    }
    attack_status.content_size = 0;
    attack_status.type = -1;
    attack_status.state = READY;
    hydra_display_set_attack_timeout(0);
}

void attack_init(){
    const esp_timer_create_args_t attack_timeout_args = { .callback = &attack_timeout };
    ESP_ERROR_CHECK(esp_timer_create(&attack_timeout_args, &attack_timeout_handle));
    ESP_ERROR_CHECK(esp_event_handler_register(WEBSERVER_EVENTS, WEBSERVER_EVENT_ATTACK_REQUEST, &attack_request_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WEBSERVER_EVENTS, WEBSERVER_EVENT_ATTACK_RESET, &attack_reset_handler, NULL));
}
