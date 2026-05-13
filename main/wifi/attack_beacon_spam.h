/**
 * @file attack_beacon_spam.h
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 8-5-2026
 * @copyright Copyright (c) 2026
 *
 * @brief Beacon spam attack modes and controls.
 */

#ifndef ATTACK_BEACON_SPAM_H
#define ATTACK_BEACON_SPAM_H

#include <stdint.h>

typedef enum {
    BEACON_MODE_COMMON = 0,
    BEACON_MODE_GARBAGE,
    BEACON_MODE_RICK_ROLL,
    BEACON_MODE_SECURITY
} beacon_spam_mode_t;

/**
 * @brief Starts the beacon spam attack with a specific mode.
 * @param count Number of fake APs to generate (Max 100)
 * @param mode The style of SSIDs to broadcast
 */
void attack_beacon_spam_start(uint8_t count, beacon_spam_mode_t mode);

/**
 * @brief Stops the beacon spam attack.
 */
void attack_beacon_spam_stop();

#endif
