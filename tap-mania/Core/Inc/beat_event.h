/**
 ******************************************************************************
 * @file           : beat_event.h
 * @brief          : Beat Event Processing Functions
 ******************************************************************************
 */

#ifndef BEAT_EVENT_H
#define BEAT_EVENT_H

#include "main.h"
#include "tsq_parser.h"
#include <stdint.h>

/**
 * @brief Process beat event (called from main loop when beat flag is set)
 */
void ProcessBeatEvent(TSQMetadata tsq_metadata);

/**
 * @brief Initialize first beat LED scheduling
 * @param sequence_length Total beats in sequence
 */
void PrimeFirstBeatLEDs(uint32_t sequence_length);

#endif /* BEAT_EVENT_H */
