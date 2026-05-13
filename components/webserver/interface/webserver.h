#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdint.h>
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(WEBSERVER_EVENTS);

enum {
    WEBSERVER_EVENT_ATTACK_REQUEST,
    WEBSERVER_EVENT_ATTACK_RESET
};

#define MAX_ATTACK_TARGETS 16

/**
 * Binary layout (20 bytes fixed — HTML must match exactly):
 * [0]      type
 * [1]      method
 * [2]      timeout
 * [3]      ap_count
 * [4..19]  ap_record_ids (16 slots, unused = 0)
 */
/**
 * Binary layout (21 bytes fixed — HTML must match exactly):
 * [0]      type
 * [1]      method
 * [2..3]   timeout (uint16_t, Little Endian)
 * [4]      ap_count
 * [5..20]  ap_record_ids (16 slots)
 */
typedef struct {
    uint8_t type;
    uint8_t method;
    uint16_t timeout;
    uint8_t ap_count;
    uint8_t ap_record_ids[MAX_ATTACK_TARGETS];
} __attribute__((packed)) attack_request_t; // এখন সাইজ ঠিক ২১ বাইট হবে

void webserver_run();
// webserver.h ফাইলের শেষে যোগ করুন
void webserver_stop(void);


#endif
