/**
 * @file management_helper.h
 * @author Sameer Al Sahab (sameeralsahab54@gmail.com)
 * @date 2026-02-03
 * @copyright Copyright (c) 2026
 *
 * @brief helpers for other utils.
 */


#include "management_helper.h"

#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "webserver.h"
#include "wifi_controller.h"

static const char *TAG = "mgmt_helper";

void restore_management_system(void)
{
    ESP_LOGI(TAG, "Restoring Management System...");

    webserver_stop();
    vTaskDelay(pdMS_TO_TICKS(300));

    esp_wifi_disconnect();
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(300));

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_start();
    vTaskDelay(pdMS_TO_TICKS(200));

    wifictl_mgmt_ap_start();
    vTaskDelay(pdMS_TO_TICKS(500));

    webserver_run();

    ESP_LOGI(TAG, "Management System Restored.");
}

char *load_html_from_spiffs(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }

    rewind(f);

    char *html = malloc((size_t)size + 1);
    if (html == NULL) {
        ESP_LOGE(TAG, "Malloc failed for %s", path);
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(html, 1, (size_t)size, f);
    html[read_size] = '\0';

    fclose(f);

    ESP_LOGI(TAG, "Loaded %s (%ld bytes)", path, size);

    return html;
}
