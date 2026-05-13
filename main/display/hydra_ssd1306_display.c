/**
 * @file hydra_ssd1306_display.c
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 8-5-2026
 * @copyright Copyright (c) 2026
 *
 * @brief Hydra Attack timeout or logging via SSD1306 display for user interface (optional)
 *
 * All u8g2 fonts: https://github.com/olikraus/u8g2/wiki/fntlistall
 */

#include "hydra_ssd1306_display.h"
#include "attack.h"
#include "hydra_logo.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "u8g2.h"
#include "u8g2_esp32_hal.h"   /* ESP-IDF HAL bundled with u8g2_esp_idf component */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>


static const char *TAG = "hydra_display";

static u8g2_t      u8g2;
static bool        display_enabled   = false;
static int         current_timeout   = 0;
static int         elapsed_seconds   = 0;

static TickType_t  popup_expire_ticks = 0;
static bool        is_popup_active    = false;

static char oled_buf_head[32]  = "";
static char oled_buf_line1[32] = "";
static char oled_buf_line2[32] = "";

static const char *OLED_SLOT_TAG[3] = { "HEAD ", "LINE1", "LINE2" };



static bool detect_ssd1306(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_GPIO,
        .scl_io_num       = I2C_SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    if (i2c_param_config(I2C_NUM_0, &conf) != ESP_OK)  return false;
    if (i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0) != ESP_OK) return false;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SSD1306_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    esp_err_t rc = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));

    i2c_cmd_link_delete(cmd);
    i2c_driver_delete(I2C_NUM_0);

    return (rc == ESP_OK);
}



static void draw_centered(const u8g2_uint_t y, const char *str)
{
    u8g2_uint_t w = u8g2_GetStrWidth(&u8g2, str);
    u8g2_DrawStr(&u8g2, (OLED_WIDTH - w) / 2, y, str);
}


static void draw_title_bar(const char *label)
{

    u8g2_uint_t fh = u8g2_GetMaxCharHeight(&u8g2);
    u8g2_DrawBox(&u8g2, 0, 0, OLED_WIDTH, fh + 2);
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_uint_t w = u8g2_GetStrWidth(&u8g2, label);
    u8g2_DrawStr(&u8g2, (OLED_WIDTH - w) / 2, fh, label);
    u8g2_SetDrawColor(&u8g2, 1);
}


static const char *get_attack_name(uint8_t type)
{
    switch (type) {
        case ATTACK_TYPE_PASSIVE:     return "Passive Cap";
        case ATTACK_TYPE_HANDSHAKE:   return "WPA Handshake";
        case ATTACK_TYPE_PMKID:       return "PMKID Cap";
        case ATTACK_TYPE_DOS:         return "Deauth";
        case ATTACK_TYPE_BEACON_SPAM: return "Beacon Spam";
        case ATTACK_TYPE_PROBE:       return "Probe Attack";
        case ATTACK_TYPE_EVIL_TWIN:   return "Evil Twin";
        case ATTACK_TYPE_BT_SPAM:     return "BT Spam";
        case ATTACK_TYPE_CLONE:       return "AP Clone";
        case ATTACK_TYPE_BT_PAYLOAD:  return "BT Payload";
        default:                      return "Unknown";
    }
}



static void draw_running_timeout(const char *attack_name)
{
    char buf[32];
    int remaining = current_timeout - elapsed_seconds;
    if (remaining < 0) remaining = 0;


    u8g2_SetFont(&u8g2, u8g2_font_logisoso16_tf); 
    draw_title_bar(attack_name);

  
    u8g2_SetFont(&u8g2, u8g2_font_logisoso16_tr);
    if (remaining >= 60) {
        snprintf(buf, sizeof(buf), "%02d:%02d", remaining / 60, remaining % 60);
    } else {
        snprintf(buf, sizeof(buf), "%ds", remaining);
    }
    draw_centered(42, buf);

    /* Progress bar */
    u8g2_SetFont(&u8g2, u8g2_font_5x7_mr);
    int bar_x    = 4;
    int bar_y    = 48;
    int bar_w    = 120;
    int bar_h    = 6;
    int filled   = (current_timeout > 0)
    ? (elapsed_seconds * bar_w) / current_timeout
    : 0;
    if (filled > bar_w) filled = bar_w;

    u8g2_DrawFrame(&u8g2, bar_x, bar_y, bar_w, bar_h);
    if (filled > 0) {
        u8g2_DrawBox(&u8g2, bar_x, bar_y, filled, bar_h);
    }

   
    int el = elapsed_seconds, tot = current_timeout;
    if (tot >= 60) {
        snprintf(buf, sizeof(buf), "%02d:%02d / %02d:%02d",
                 el / 60, el % 60, tot / 60, tot % 60);
    } else {
        snprintf(buf, sizeof(buf), "%ds / %ds", el, tot);
    }
    u8g2_DrawStr(&u8g2, 4, 63, buf);

    if (elapsed_seconds < current_timeout) elapsed_seconds++;
}


static void draw_running_infinite(const char *attack_name)
{
    u8g2_SetFont(&u8g2, u8g2_font_profont22_mr);
    draw_title_bar(attack_name);

    u8g2_SetFont(&u8g2, u8g2_font_profont12_mr);
    u8g2_DrawStr(&u8g2, 4, 30, "Attack running.");
    u8g2_DrawStr(&u8g2, 4, 44, "Power off ESP32");
    u8g2_DrawStr(&u8g2, 4, 58, "to stop.");
}


static void draw_standby_disconnected(void)
{
    u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
    draw_title_bar("HYDRA-ESP");

    u8g2_SetFont(&u8g2, u8g2_font_profont12_mr);
    u8g2_DrawStr(&u8g2, 4, 30, "Not connected");
    u8g2_DrawStr(&u8g2, 4, 44, "Connect Wi-Fi:");
    u8g2_DrawStr(&u8g2, 4, 57, "192.168.4.1");
}


static void draw_standby_connected(int frame)
{
    u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
    draw_title_bar("CONNECTED");


    int bar_heights[] = {8, 14, 20, 26};
    int pulse = (frame % 8);

    for (int i = 0; i < 4; i++) {
        int x = 90 + i * 9;
        int h = bar_heights[i];
        int y = 50 - h;
        if (i < (pulse / 2) + 1) {
            u8g2_DrawBox(&u8g2, x, y, 7, h);
        } else {
            u8g2_DrawFrame(&u8g2, x, y, 7, h);
        }
    }

    u8g2_SetFont(&u8g2, u8g2_font_profont12_mr);
    u8g2_DrawStr(&u8g2, 4, 28, "HYDRA ONLINE");

    u8g2_SetFont(&u8g2, u8g2_font_5x7_mr);
    u8g2_DrawStr(&u8g2, 4, 42, "AP: 192.168.4.1");

    // Blinking dot
    u8g2_DrawStr(&u8g2, 4, 56, "ARMED");
    if (frame % 2 == 0) {
        u8g2_DrawBox(&u8g2, 38, 50, 4, 4);
    }
}


static void draw_popup(void)
{

    if (oled_buf_head[0]) {
        u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
        draw_title_bar(oled_buf_head);
    }

    u8g2_SetFont(&u8g2, u8g2_font_profont12_mr);

    if (oled_buf_line1[0]) {
        draw_centered(30, oled_buf_line1);
    }
    if (oled_buf_line2[0]) {
        draw_centered(46, oled_buf_line2);
    }
}

static void draw_logo(void)
{
    u8g2_DrawXBM(&u8g2, 0, 0, OLED_WIDTH, OLED_HEIGHT, epd_bitmap_hydra_logo);
}

static void hydra_display_task(void *pvParameters)
{
    (void)pvParameters;
    int matrix_frame = 0;

    while (1) {
        if (!display_enabled) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        u8g2_ClearBuffer(&u8g2);

   
        if (is_popup_active) {
            if (xTaskGetTickCount() < popup_expire_ticks) {
                draw_popup();
                u8g2_SendBuffer(&u8g2);
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            } else {
                is_popup_active = false;
                oled_log_clear();
            }
        }

    
        const attack_status_t *status = attack_get_status();

        if (status->state == RUNNING) {
            const char *name = get_attack_name(status->type);
            if (current_timeout > 0) {
                draw_running_timeout(name);
            } else {
                draw_running_infinite(name);
            }
        } else {
          
            elapsed_seconds = 0;
            current_timeout = 0;

            wifi_sta_list_t sta_list = {0};
            esp_wifi_ap_get_sta_list(&sta_list);

            if (sta_list.num == 0) {
                draw_standby_disconnected();
            } else {
                draw_standby_connected(matrix_frame++);
            }
        }

        u8g2_SendBuffer(&u8g2);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void boot_animation(void)
{

    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawXBM(&u8g2, 0, 0, OLED_WIDTH, OLED_HEIGHT, epd_bitmap_hydra_logo);

    u8g2_SendBuffer(&u8g2);
    vTaskDelay(pdMS_TO_TICKS(4000));


    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont22_mr);
    draw_title_bar("HYDRA-ESP");

    u8g2_SendBuffer(&u8g2);
    vTaskDelay(pdMS_TO_TICKS(1800));
}


void hydra_display_init(void)
{
    ESP_LOGI(TAG, "Scanning for SSD1306 OLED...");

    if (!detect_ssd1306()) {
        ESP_LOGW(TAG, "SSD1306 not detected — display disabled");
        display_enabled = false;
        return;
    }

    display_enabled = true;

    /*
     * u8g2 setup for SSD1306 128x64 over hardware I2C.
     *
     * u8g2_esp32_hal_init() configures the IDF I2C driver;
     * the HAL is part of the u8g2_esp_idf component.
     */
    u8g2_esp32_hal_t hal_config = U8G2_ESP32_HAL_DEFAULT;
    hal_config.bus.i2c.sda       = I2C_SDA_GPIO;
    hal_config.bus.i2c.scl       = I2C_SCL_GPIO;

    u8g2_esp32_hal_init(hal_config);
    u8g2_esp32_hal_init(hal_config);

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2,
        U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb
    );

    u8g2_SetI2CAddress(&u8g2, SSD1306_I2C_ADDR << 1);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);    
    u8g2_SetContrast(&u8g2, 0xFF);

    boot_animation();

    xTaskCreate(
        hydra_display_task,
        "hydra_display_task",
        4096,                 
        NULL,
        5,
        NULL
    );
}



void hydra_display_set_attack_timeout(int timeout_sec)
{
    current_timeout = timeout_sec;
    elapsed_seconds = 0;
}


void oled_log(oled_row_t row, int timeout_sec, const char *fmt, ...)
{
    if (!fmt) return;
    if (row > OLED_LINE2)    row = OLED_LINE2;
    if (timeout_sec <= 0) timeout_sec = 3;

    char msg[32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    ESP_LOGI(TAG, "[OLED/%s] %s", OLED_SLOT_TAG[row], msg);

    if (!display_enabled) return;

    char *target = (row == OLED_HEAD)  ? oled_buf_head  :
    (row == OLED_LINE1) ? oled_buf_line1 :
    oled_buf_line2;

    strncpy(target, msg, sizeof(msg) - 1);
    target[sizeof(msg) - 1] = '\0';

    popup_expire_ticks = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_sec * 1000);
    is_popup_active    = true;
}

void oled_log_clear(void)
{
    oled_buf_head[0]  = '\0';
    oled_buf_line1[0] = '\0';
    oled_buf_line2[0] = '\0';
    is_popup_active   = false;
}
