/**
 * @file attack_bt_spam.h
 * @author Raghu Saxena (poiasdpoiasd@live.com) and Willy-JL, ECTO-1A, Spooks4576
 * @brief BLE Spam Attack interface
 *
 */


#ifndef ATTACK_BT_SPAM_H
#define ATTACK_BT_SPAM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
    #endif

    /**
     * BLE spam configuration
     */
    typedef struct {

        /**
         * Device / packet type
         *
         * 1-8   = Apple Audio
         * 9-13  = Apple Setup
         * 14-19 = Samsung Buds
         * 20-24 = Google Fast Pair
         * 25    = Random mix
         */
        int device_type;

        /**
         * Legacy compatibility field
         *
         * Old code may still use this.
         * Not used by the new fast timing system.
         */
        int delay_seconds;

        /**
         * Extra delay between advertisement bursts
         *
         * 0   = maximum speed
         * 10  = very fast
         * 50  = moderate
         * 100 = slower / safer
         */
        uint16_t delay_ms;

        /**
         * Advertisement type
         *
         * Reserved for compatibility/UI usage.
         * Currently unused internally.
         */
        int adv_type;

    } bt_spam_config_t;

    /**
     * Initialize BLE spam module
     */
    void attack_bt_spam_init(void);

    /**
     * Start BLE spam
     */
    void attack_bt_spam_start(bt_spam_config_t *config);

    /**
     * Stop BLE spam
     */
    void attack_bt_spam_stop(void);

    /**
     * Check running state
     */
    bool attack_bt_spam_is_running(void);

    #ifdef __cplusplus
}
#endif

#endif /* ATTACK_BT_SPAM_H */
