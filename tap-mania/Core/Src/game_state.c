/**
 ******************************************************************************
 * @file           : game_state.c
 * @brief          : Game State Machine Implementation
 ******************************************************************************
 */

#include "game_state.h"
#include "uart_debug.h"
#include <stdio.h>

static SystemState system_state = SYSTEM_STATE_INIT;

extern UART_HandleTypeDef huart2;

/**
 * @brief Change system state
 */
void ChangeSystemState(SystemState new_state) { system_state = new_state; }

/**
 * @brief Get current system state
 */
SystemState GetCurrentSystemState(void) { return system_state; }

/**
 * @brief Wait for user to select sequence
 */
uint8_t WaitForSequenceSelection(void) {
  char uart_buf[256];
  int uart_buf_len;
  uint8_t byte;

  uart_buf_len = sprintf(uart_buf, "=== SEQUENCE SELECTION ===\r\n"
                                   "1-4: Hardcoded test sequences\r\n"
                                   "  5: Loaded TSQ sequence\r\n"
                                   "Select sequence (1-5): ");
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);

  while (1) {
    if (UART_ReadBytes(&byte, 1) > 0) {
      HAL_UART_Transmit(&huart2, &byte, 1, 100);
      HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n\r\n", 4, 100);

      if (byte >= '1' && byte <= '5') {
        return byte - '0';
      } else {
        uart_buf_len =
            sprintf(uart_buf, "Invalid selection. Please enter 1-5: ");
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
      }
    }
    HAL_Delay(10);
  }
}
