/**
 * @file attack_probe.h
 * @brief Provides interface for Probe Request response/spam attack.
 */
#ifndef ATTACK_PROBE_H
#define ATTACK_PROBE_H

#include "attack.h"

/**
 * @brief Starts Probe Response/Beacon Spam attack.
 *
 * It listens for probe requests and spawns fake beacons based on what devices are looking for.
 */
void attack_probe_start(attack_config_t *attack_config);

/**
 * @brief Stops the attack.
 */
void attack_probe_stop();

#endif
