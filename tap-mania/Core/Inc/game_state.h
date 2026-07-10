/**
 ******************************************************************************
 * @file           : game_state.h
 * @brief          : Game State Machine Management
 ******************************************************************************
 */

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "main.h"
#include <stdint.h>

// System state definitions
typedef enum {
    SYSTEM_STATE_INIT,
	SYSTEM_STATE_SEND_CONFIG,
    SYSTEM_STATE_SEQUENCE_SELECT,
	SYSTEM_STATE_WAITING_CONFIG_INPUT, // waits for usart2 output from gui
    SYSTEM_STATE_READY,
    SYSTEM_STATE_PLAYING,
    SYSTEM_STATE_COMPLETE
} SystemState;

/**
 * @brief Change system state
 * @param new_state New state to transition to
 */
void ChangeSystemState(SystemState new_state);

/**
 * @brief Get current system state
 * @return Current SystemState
 */
SystemState GetCurrentSystemState(void);

/**
 * @brief Wait for user to select sequence
 * @return Selected sequence number (1-5)
 */
uint8_t WaitForSequenceSelection(void);

#endif /* GAME_STATE_H */
