/**
 ******************************************************************************
 * @file           : sd_card_detect.h
 * @brief          : SD Card Detection (CubeMX Compatible)
 ******************************************************************************
 * @attention
 *
 * This module uses GPIO configuration from STM32CubeMX .ioc file.
 * No GPIO initialization is performed here - CubeMX handles it.
 *
 * Pin Configuration (in your .ioc file):
 * - Pin: PC13 (or your configured pin)
 * - Mode: GPIO_Input
 * - Pull: No pull-up/pull-down (external hardware pull-up present)
 * - Label: SD_CD (generates SD_CD_Pin and SD_CD_GPIO_Port defines)
 *
 * Hardware:
 * - External pull-up resistor (4.7kΩ - 10kΩ) to 3.3V
 * - Card Detect pin active LOW (card present = 0V, no card = 3.3V)
 *
 ******************************************************************************
 */

#ifndef SD_CARD_DETECT_H
#define SD_CARD_DETECT_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Check if SD card is physically present
 *
 * Reads the Card Detect pin configured in CubeMX.
 * Pin is active-low: LOW = card present, HIGH = no card
 *
 * @retval true if card is present, false otherwise
 */
bool SDCard_IsPresent(void);

/**
 * @brief Check card presence with debouncing
 *
 * Takes multiple samples with delays to filter mechanical bounce.
 * More reliable for detecting card insertion/removal events.
 *
 * @retval true if card is stably present, false otherwise
 */
bool SDCard_IsPresent_Debounced(void);

/**
 * @brief Wait for SD card to be inserted
 *
 * Blocks execution until an SD card is detected.
 * Polls every 500ms and prints status messages via UART.
 *
 * @param timeout_ms Maximum time to wait in milliseconds (0 = wait forever)
 * @retval true if card detected within timeout, false if timeout expired
 */
bool SDCard_WaitForInsertion(uint32_t timeout_ms);

/**
 * @brief Display SD card error message
 *
 * Shows user-friendly error message when no SD card is detected.
 * Called automatically when card is missing at startup.
 *
 * @retval None
 */
void SDCard_DisplayError(void);

/**
 * @brief Get card insertion event count (diagnostic)
 *
 * Returns number of times a card has been detected since boot.
 * Useful for debugging intermittent connections.
 *
 * @retval Number of insertion events detected
 */
uint32_t SDCard_GetInsertionCount(void);

#endif /* SD_CARD_DETECT_H */