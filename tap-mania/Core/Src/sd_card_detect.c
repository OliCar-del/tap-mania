/**
 ******************************************************************************
 * @file           : sd_card_detect.c
 * @brief          : SD Card Detection Implementation (CubeMX Compatible)
 ******************************************************************************
 * @attention
 *
 * This implementation assumes GPIO is already configured by STM32CubeMX.
 * No GPIO initialization is performed here.
 *
 ******************************************************************************
 */

#include "sd_card_detect.h"
#include <stdarg.h>
#include <stdio.h>

/* Private Variables */
static uint32_t insertion_count = 0;

/* External UART handle (for debug messages) */
extern UART_HandleTypeDef huart2;

/**
 * @brief Simple printf wrapper for UART
 */
static void sd_printf(const char *format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (len > 0) {
    HAL_UART_Transmit(&huart2, (uint8_t *)buffer, len, 1000);
  }
}

/**
 * @brief Check if SD card is physically present
 */
bool SDCard_IsPresent(void) {
  /* Card Detect pin is active high (configured in CubeMX):
   * - GPIO_PIN_RESET (1) = Card present
   * - GPIO_PIN_SET (0) = No card
   */
  GPIO_PinState pin_state = HAL_GPIO_ReadPin(SD_CD_GPIO_Port, SD_CD_Pin);
  return (pin_state == GPIO_PIN_RESET);
}

/**
 * @brief Check card presence with debouncing
 */
bool SDCard_IsPresent_Debounced(void) {
#define DEBOUNCE_SAMPLES 5
#define DEBOUNCE_DELAY_MS 10

  uint8_t present_count = 0;

  /* Take multiple samples to filter bounce */
  for (int i = 0; i < DEBOUNCE_SAMPLES; i++) {
    if (SDCard_IsPresent()) {
      present_count++;
    }
    HAL_Delay(DEBOUNCE_DELAY_MS);
  }

  /* Card is present if majority of samples agree */
  return (present_count >= (DEBOUNCE_SAMPLES / 2 + 1));
}

/**
 * @brief Wait for SD card to be inserted
 */
bool SDCard_WaitForInsertion(uint32_t timeout_ms) {
  uint32_t start_tick = HAL_GetTick();
  bool card_detected = false;

  sd_printf("\r\n");
  sd_printf("╔════════════════════════════════════════════════╗\r\n");
  sd_printf("║                                                ║\r\n");
  sd_printf("║          NO SD CARD DETECTED                   ║\r\n");
  sd_printf("║                                                ║\r\n");
  sd_printf("║   Please insert an SD card to continue...     ║\r\n");
  sd_printf("║                                                ║\r\n");
  sd_printf("╚════════════════════════════════════════════════╝\r\n");
  sd_printf("\r\n");

  /* Wait for card with periodic checking */
  while (!card_detected) {
    /* Check if card is present (with debouncing) */
    if (SDCard_IsPresent_Debounced()) {
      card_detected = true;
      sd_printf("\r\n✓ SD card detected!\r\n\r\n");
      insertion_count++;
      break;
    }

    /* Check timeout */
    if (timeout_ms > 0) {
      uint32_t elapsed = HAL_GetTick() - start_tick;
      if (elapsed >= timeout_ms) {
        sd_printf("\r\n✗ Timeout waiting for SD card\r\n");
        return false;
      }
    }

    /* Wait before next check */
    HAL_Delay(500);

    /* Show waiting indicator */
    sd_printf(".");
  }

  /* Small delay to ensure card is fully seated */
  HAL_Delay(200);

  return true;
}

/**
 * @brief Display SD card error message
 */
void SDCard_DisplayError(void) {
  sd_printf("\r\n");
  sd_printf("╔════════════════════════════════════════════════╗\r\n");
  sd_printf("║                                                ║\r\n");
  sd_printf("║              ⚠ ERROR ⚠                        ║\r\n");
  sd_printf("║                                                ║\r\n");
  sd_printf("║         NO SD CARD DETECTED                    ║\r\n");
  sd_printf("║                                                ║\r\n");
  sd_printf("║  The device requires a microSD card with      ║\r\n");
  sd_printf("║  sequence files (.tsq) and audio files        ║\r\n");
  sd_printf("║  to operate.                                   ║\r\n");
  sd_printf("║                                                ║\r\n");
  sd_printf("║  Please:                                       ║\r\n");
  sd_printf("║  1. Power off the device                       ║\r\n");
  sd_printf("║  2. Insert a formatted microSD card            ║\r\n");
  sd_printf("║  3. Power on the device                        ║\r\n");
  sd_printf("║                                                ║\r\n");
  sd_printf("╚════════════════════════════════════════════════╝\r\n");
  sd_printf("\r\n");
}

/**
 * @brief Get card insertion event count
 */
uint32_t SDCard_GetInsertionCount(void) { return insertion_count; }
