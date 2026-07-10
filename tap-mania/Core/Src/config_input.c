/**
 ******************************************************************************
 * @file           : config_input.c
 * @brief          : Simple configuration input implementation
 ******************************************************************************
 */

#include "config_input.h"
#include "SH1106.h"
#include "button_handler.h"
#include "config_storage.h"
#include "scoring.h"
#include "uart_debug.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External UART handle */
extern UART_HandleTypeDef huart2;

/* External UART functions */
extern uint32_t UART_ReadBytes(uint8_t *dest, uint32_t max_len);

/**
 * @brief Parse SETWIN command
 *
 * Format: SETWIN:100,150,200,250
 */
bool Config_ParseInput(const char *input, ScoringWindows *windows) {
  if (!input || !windows) {
    return false;
  }

  // Check if starts with SETWIN:
  if (strncmp(input, "SETWIN:", 7) != 0) {
    return false;
  }

  // Parse four comma-separated integers
  const char *params = input + 7;
  int p, g, o, po;
  int parsed = sscanf(params, "%d,%d,%d,%d", &p, &g, &o, &po);

  if (parsed != 4) {
    return false;
  }

  // Validate range (1-500ms inclusive)
  if (p < 1 || p > 500 || g < 1 || g > 500 || o < 1 || o > 500 || po < 1 ||
      po > 500) {
    return false;
  }

  // Validate ordering (Perfect < Good < OK < Poor)
  if (p >= g || g >= o || o >= po) {
    return false;
  }

  // Valid - populate output
  windows->perfect_radius_ms = (uint32_t)p;
  windows->good_radius_ms = (uint32_t)g;
  windows->ok_radius_ms = (uint32_t)o;
  windows->poor_radius_ms = (uint32_t)po;

  return true;
}

bool ReadLine(char *buffer, uint32_t max_len) {
  uint32_t index = 0;
  uint8_t byte;

  while (index < (max_len - 1)) {
    // Wait for character
    while (UART_ReadBytes(&byte, 1) == 0) {
      HAL_Delay(1);
    }

    // Handle Enter
    if (byte == '\r' || byte == '\n') {
      // Echo newline
      HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 100);
      buffer[index] = '\0';
      return true;
    }

    // Handle backspace
    if (byte == 0x08 || byte == 0x7F) {
      if (index > 0) {
        index--;
        // Echo backspace sequence
        HAL_UART_Transmit(&huart2, (uint8_t *)"\b \b", 3, 100);
      }
      continue;
    }

    // Normal character - add to buffer and echo
    if (byte >= 0x20 && byte < 0x7F) {
      buffer[index++] = byte;
      HAL_UART_Transmit(&huart2, &byte, 1, 100);
    }
  }

  // Buffer full
  buffer[max_len - 1] = '\0';
  return true;
}

/**
 * @brief Wait for configuration input
 */
bool Config_WaitForInput(void) {
  char uart_buf[256];
  char input_buf[CONFIG_INPUT_MAX_LEN];
  int uart_buf_len;

  // Display current windows
  const ScoringWindows *current = Scoring_GetWindows();
  uart_buf_len = sprintf(
      uart_buf,
      "\r\n=== GAME CONFIGURATION ===\r\n"
      "Current windows: Perfect=±%u Good=±%u OK=±%u Poor=±%u ms\r\n"
      "\r\n"
      "Enter new config (SETWIN:P,G,O,Po) or press Enter to continue:\r\n> ",
      current->perfect_radius_ms, current->good_radius_ms,
      current->ok_radius_ms, current->poor_radius_ms);
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

  // Read input line
  if (!ReadLine(input_buf, sizeof(input_buf))) {
    return false;
  }

  // Check if empty (just Enter pressed)
  if (strlen(input_buf) == 0) {
    uart_buf_len = sprintf(uart_buf, "Using current windows.\r\n\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    return false;
  }

  // Try to parse as SETWIN command
  ScoringWindows new_windows;
  if (Config_ParseInput(input_buf, &new_windows)) {
    // Valid config - apply it
    if (Scoring_SetWindows(&new_windows)) {
      uart_buf_len =
          sprintf(uart_buf, "✓ Windows set to: P=±%u G=±%u O=±%u Po=±%u ms\r\n",
                  new_windows.perfect_radius_ms, new_windows.good_radius_ms,
                  new_windows.ok_radius_ms, new_windows.poor_radius_ms);
      HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

      // Save to Flash
      uart_buf_len = sprintf(uart_buf, "Saving to Flash... ");
      HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

      if (Config_Save()) {
        uart_buf_len = sprintf(uart_buf, "✓ Saved!\r\n\r\n");
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
      } else {
        uart_buf_len = sprintf(
            uart_buf, "✗ Save failed (will use for this session)\r\n\r\n");
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
      }

      return true;
    } else {
      uart_buf_len = sprintf(uart_buf, "✗ Failed to apply windows\r\n\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
      return false;
    }
  } else {
    // Invalid format
    uart_buf_len = sprintf(
        uart_buf, "✗ Invalid format. Use: SETWIN:P,G,O,Po (e.g., "
                  "SETWIN:80,120,180,240)\r\n"
                  "  Values must be 1-500ms and Perfect < Good < OK < Poor\r\n"
                  "Using current windows.\r\n\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    return false;
  }
}

/**
 * @brief Send current window configuration via UART (for GUI)
 */
void Config_SendCurrentWindows(void) {
  const ScoringWindows *w = Scoring_GetWindows();
  char uart_buf[128];
  int uart_buf_len;

  // Send in format: WINDOWS:P,G,O,Po\r\n
  uart_buf_len =
      sprintf(uart_buf, "WINDOWS:%u,%u,%u,%u\r\n", w->perfect_radius_ms,
              w->good_radius_ms, w->ok_radius_ms, w->poor_radius_ms);

  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
}
