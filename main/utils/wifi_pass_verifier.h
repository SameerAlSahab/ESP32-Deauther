#pragma once
/**
 * @file wifi_pass_verifier.h
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 8-5-2026
 * @copyright Copyright (c) 2026
 */

/**

 * Usage :
 *
 *   #include "wifi_verify.h"
 *
 *   wifi_verify_init();
 *
 *   wifi_verify_result_t r = wifi_verify_password(
 *       "SSID", "Password", NULL, 10000
 *   );
 *
 *   if (r.status == WIFI_VERIFY_CORRECT) {
 *       printf("Password correct! IP: " IPSTR "\n", IP2STR(&r.ip));
 *   }
 *
 * Dependencies in CMakeLists.txt REQUIRES:
 *   esp_wifi nvs_flash esp_event esp_netif
 */

#ifdef __cplusplus
extern "C" {
    #endif

    #include <stdint.h>
    #include <stdbool.h>
    #include "esp_netif.h"


    typedef enum {
        WIFI_VERIFY_CORRECT        = 0,  /**< Password correct, got IP            */
        WIFI_VERIFY_WRONG_PASSWORD = 1,  /**< Auth failed — wrong password        */
        WIFI_VERIFY_AP_NOT_FOUND   = 2,  /**< SSID not found                      */
        WIFI_VERIFY_TIMEOUT        = 3,  /**< Timed out                           */
        WIFI_VERIFY_ERROR          = 4,  /**< Internal ESP-IDF error              */
    } wifi_verify_status_t;


    typedef struct {
        wifi_verify_status_t status;


        esp_ip4_addr_t ip;       /**< Assigned IP address                        */
        esp_ip4_addr_t gateway;  /**< Gateway IP                                 */
        esp_ip4_addr_t netmask;  /**< Subnet mask                                */


        uint8_t disconnect_reason;
        char    ssid[33];
    } wifi_verify_result_t;


    void wifi_verify_init(void);

    /**
     * @brief Check if a password is correct for a given SSID.
     *
     * Uses WPA2 4-way handshake
     *
     * @param ssid        Target network name (max 32 chars)
     * @param password    Password to test   (max 63 chars, NULL = open network)
     * @param bssid
     * @param timeout_ms  Max wait in milliseconds
     *
     * @return wifi_verify_result_t
     */
    wifi_verify_result_t wifi_verify_password(
        const char    *ssid,
        const char    *password,
        const uint8_t *bssid,
        uint32_t       timeout_ms
    );


    const char *wifi_verify_status_str(wifi_verify_status_t status);

    #ifdef __cplusplus
}
#endif
