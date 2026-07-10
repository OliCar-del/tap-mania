/* button_handler.h - Required additions/verification
 *
 * Ensure your button_handler.h includes these declarations
 * Add any missing ones to your existing header file
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include "main.h"
#include "scoring.h"
#include <stdbool.h>

// ===== REQUIRED DEFINITIONS =====
// These should already exist in your header

// Button count
#define BUTTON_COUNT 4

// Button IDs
typedef enum {
    BUTTON_ID_RED = 0,
    BUTTON_ID_GREEN = 1,
    BUTTON_ID_BLUE = 2,
    BUTTON_ID_YELLOW = 3
} ArcadeButtonID;
extern bool pbSelPressed;
// ===== REQUIRED FUNCTION DECLARATIONS =====

/**
 * @brief Initialize button handler system
 * Must be called before button handler is used
 */
void ButtonHandler_Init(void);

/**
 * @brief Process all queued button events
 * Call this regularly from main loop
 */
void ButtonHandler_ProcessQueue(void);

/**
 * @brief Check for missed beats and apply penalties
 * @param current_beat The current beat number to check
 * Call this at beat boundaries
 */
void ButtonHandler_CheckMissedBeats(uint32_t current_beat);

/**
 * @brief Get button name string for display/logging
 * @param id Button identifier
 * @return Pointer to button name string
 */
const char* GetButtonName(ArcadeButtonID id);

// ===== NEW FUNCTION (add to your header) =====

/**
 * @brief Get count of dropped events due to queue overflow
 * @return Number of events that were dropped
 * Use for diagnostics - if this increases, consider increasing ACTION_QUEUE_SIZE
 */
uint32_t ButtonHandler_GetDroppedEventCount(void);

void poll_buttons(void);
int get_choose_song(void);
bool get_scrolled(void);
bool get_selected_song(void);
bool get_pb_sel(void);
bool get_press_right(void);
// ===== GPIO PIN DEFINITIONS =====
// These should be defined in main.h but verify they exist:
/*
#define RED_PB_Pin GPIO_PIN_0
#define RED_PB_GPIO_Port GPIOB
#define GREEN_PB_Pin GPIO_PIN_1
#define GREEN_PB_GPIO_Port GPIOB
#define BLUE_PB_Pin GPIO_PIN_2
#define BLUE_PB_GPIO_Port GPIOB
#define YELLOW_PB_Pin GPIO_PIN_4
#define YELLOW_PB_GPIO_Port GPIOB
*/

#endif /* BUTTON_HANDLER_H */

/* ===== INTEGRATION NOTES =====
 *
 * 1. If ButtonHandler_GetDroppedEventCount() is not in your header,
 *    add it - this is a new diagnostic function.
 *
 * 2. Verify all pin definitions match your hardware configuration.
 *
 * 3. The button handler expects buttons to be active-low (pressed = RESET).
 *
 * 4. Required includes in button_handler.c:
 *    - "button_handler.h"
 *    - "scoring.h"
 *    - "sequence.h"
 *    - <stdio.h>
 *    - <string.h>
 *    - <stdbool.h>
 *    - <stdlib.h>
 *
 * 5. External variables required (should be in main.c):
 *    - volatile uint32_t last_beat_timestamp
 *    - volatile uint32_t beat_count
 *    - volatile uint16_t current_bpm
 *    - UART_HandleTypeDef huart2
 *
 * 6. External function required (should be in main.c):
 *    - uint32_t GetMicrosecondTimestamp(void)
 */
