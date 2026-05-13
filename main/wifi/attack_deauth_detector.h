/**
 * @file attack_deauth_detector.h
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 8-5-2026
 * @copyright Copyright (c) 2026
 */

#ifndef ATTACK_DEAUTH_DETECTOR_H
#define ATTACK_DEAUTH_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

#define DEAUTH_THRESHOLD     10
#define DEAUTH_WINDOW_MS   1000
#define MAX_TRACKED_BSSIDS   16

typedef struct {
    uint8_t  bssid[6];
    uint16_t count;
    int64_t  window_start_ms;
    bool     alerting;

    int64_t  last_alert_ms;
} deauth_track_entry_t;

typedef struct {
    deauth_track_entry_t entries[MAX_TRACKED_BSSIDS];
    uint8_t              count;
    bool                 running;
} deauth_detector_status_t;

void                           deauth_detector_start();
void                           deauth_detector_stop();
const deauth_detector_status_t *deauth_detector_get_status();

#endif
