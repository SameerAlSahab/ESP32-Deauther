/**
 * @file attack_payload_attack.h
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @brief interface for bluetooth payload attacks
 */

#pragma once

#ifdef __cplusplus
extern "C" {
    #endif

    void bt_payload_attack_init(void);
    void bt_payload_attack_start(int payload);
    void bt_payload_attack_stop(void);
    void bt_payload_attack_set_payload(int payload);
    void bt_payload_attack_run_now(void);

    bool bt_payload_is_busy(void);
    bool bt_payload_is_connected(void);

    const char* bt_payload_get_connected_name(void);
    const char* bt_payload_get_connected_mac(void);

    #ifdef __cplusplus
}
#endif
