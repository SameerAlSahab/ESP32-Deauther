/**
 * @file attack_eviltwin.h
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @brief Provides interface for evil twin attack.
 */

#ifndef ATTACK_EVILTWIN_H
#define ATTACK_EVILTWIN_H

#include "esp_wifi_types.h"

void attack_method_broadcast(const wifi_ap_record_t *ap_record, unsigned period_sec);
void attack_method_broadcast_stop();


void attack_method_evil_twin(const wifi_ap_record_t *ap_record);

void attack_method_evil_twin_stop(void);
bool is_evil_twin_active(void);
const char* get_evil_twin_password(void);
int get_wrong_attempts_count(void);
void get_wrong_passwords(char *buffer, size_t max_len);
#endif
