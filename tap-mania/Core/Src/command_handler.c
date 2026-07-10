/**
 ******************************************************************************
 * @file           : command_handler.c
 * @brief          : UART Command Processing Implementation
 ******************************************************************************
 */

#include "command_handler.h"
#include "button_handler.h"
#include "led_control.h"
#include "led_display.h"
#include "main.h"
#include "scoring.h"
#include "sequence.h"
#include "test_sequences.h"
#include "uart_debug.h"
#include <stdio.h>

extern volatile uint32_t beat_count;
extern volatile uint16_t current_bpm;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart2;
extern LEDControl led_control;

// Forward declarations for command handlers
static void HandleResetCommand(void);
static void HandleScoreCommand(void);
static void HandleSequenceCommand(uint8_t seq_num);
static void HandlePrintCommand(void);
static void HandleLEDTestCommand(void);
static void HandleBPMCommand(void);
static void HandleHelpCommand(void);

/**
 * @brief Process UART commands during gameplay
 */
void ProcessGameCommands(void) {
  uint8_t rx_byte;
  if (UART_ReadBytes(&rx_byte, 1) > 0) {
    switch (rx_byte) {
    case 'r':
    case 'R':
      HandleResetCommand();
      break;
    case 's':
    case 'S':
      HandleScoreCommand();
      break;
    case '1':
    case '2':
    case '3':
      HandleSequenceCommand(rx_byte - '0');
      break;
    case 'p':
    case 'P':
      HandlePrintCommand();
      break;
    case 'l':
    case 'L':
      HandleLEDTestCommand();
      break;
    case 'b':
    case 'B':
      HandleBPMCommand();
      break;
    case 'h':
    case 'H':
      HandleHelpCommand();
      break;
    default:
      // Ignore unknown commands
      break;
    }
  }
}

/**
 * @brief Handle reset command
 */
static void HandleResetCommand(void) {
  char uart_buf[128];
  int uart_buf_len;

  Scoring_Reset();
  ButtonHandler_Init();
  Sequence_Reset();
  InitLEDControl();
  LEDDisplay_Clear();
  LEDDisplay_LoadInitialWindow();
  beat_count = 0;

  uart_buf_len = sprintf(uart_buf, "\r\n=== SCORING & SEQUENCE RESET ===\r\n");
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);
}

/**
 * @brief Handle score display command
 */
static void HandleScoreCommand(void) {
  char uart_buf[256];
  int uart_buf_len;

  uart_buf_len =
      sprintf(uart_buf,
              "\r\n=== SCORE SUMMARY ===\r\n"
              "Total Score: %ld\r\n"
              "Current Combo: %lu\r\n"
              "Max Combo: %lu\r\n"
              "Beat: %lu\r\n"
              "LED States: G=%d R=%d B=%d Y=%d\r\n"
              "===================\r\n",
              Scoring_GetTotal(), Scoring_GetCombo(), Scoring_GetMaxCombo(),
              beat_count, led_control.led_is_on[BUTTON_ID_GREEN],
              led_control.led_is_on[BUTTON_ID_RED],
              led_control.led_is_on[BUTTON_ID_BLUE],
              led_control.led_is_on[BUTTON_ID_YELLOW]);
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);
}

/**
 * @brief Handle sequence loading command
 */
static void HandleSequenceCommand(uint8_t seq_num) {
  LoadTestSequence(seq_num);
  InitLEDControl();
}

/**
 * @brief Handle print sequence state command
 */
static void HandlePrintCommand(void) { Sequence_PrintCurrentState(); }

/**
 * @brief Handle LED test command
 */
static void HandleLEDTestCommand(void) {
  char uart_buf[128];
  int uart_buf_len;

  uart_buf_len = sprintf(uart_buf, "\r\nTesting all LEDs...\r\n");
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);

  // Turn all on
  for (int i = 0; i < BUTTON_COUNT; i++) {
    SetButtonLED(i, true);
  }
  HAL_GPIO_WritePin(BEAT_LED_GPIO_Port, BEAT_LED_Pin, GPIO_PIN_RESET);
  HAL_Delay(1000);

  // Turn all off
  for (int i = 0; i < BUTTON_COUNT; i++) {
    SetButtonLED(i, false);
  }
  HAL_GPIO_WritePin(BEAT_LED_GPIO_Port, BEAT_LED_Pin, GPIO_PIN_SET);

  uart_buf_len = sprintf(uart_buf, "LED test complete\r\n");
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);
}

/**
 * @brief Handle BPM change command
 */
static void HandleBPMCommand(void) {
  char uart_buf[128];
  int uart_buf_len;
  uint8_t rx_byte;

  uart_buf_len = sprintf(uart_buf, "\r\nEnter new BPM (60-220): ");
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);

  uint8_t bpm_buf[4] = {0};
  int idx = 0;
  uint32_t timeout = HAL_GetTick() + 5000;

  while (idx < 3 && HAL_GetTick() < timeout) {
    if (UART_ReadBytes(&rx_byte, 1) > 0) {
      if (rx_byte >= '0' && rx_byte <= '9') {
        bpm_buf[idx++] = rx_byte;
        UART_Transmit(&rx_byte, 1);
      } else if (rx_byte == '\r' || rx_byte == '\n') {
        break;
      }
    }
    HAL_Delay(10);
  }

  if (idx > 0) {
    uint16_t new_bpm = 0;
    for (int i = 0; i < idx; i++) {
      new_bpm = new_bpm * 10 + (bpm_buf[i] - '0');
    }
    if (new_bpm >= 60 && new_bpm <= 220) {
      extern void Update_BPM_Timer(uint16_t bpm);
      current_bpm = new_bpm;
      Update_BPM_Timer(current_bpm);
      uart_buf_len =
          sprintf(uart_buf, "\r\nBPM changed to %u\r\n", current_bpm);
    } else {
      uart_buf_len = sprintf(uart_buf, "\r\nInvalid BPM (must be 60-220)\r\n");
    }
    UART_Transmit((uint8_t *)uart_buf, uart_buf_len);
  }
}

/**
 * @brief Handle help command
 */
static void HandleHelpCommand(void) {
  char uart_buf[256];
  int uart_buf_len;

  uart_buf_len = sprintf(uart_buf, "\r\n=== HELP MENU ===\r\n"
                                   "r/R - Reset game\r\n"
                                   "s/S - Show score\r\n"
                                   "1   - Load simple sequence\r\n"
                                   "2   - Load holds sequence\r\n"
                                   "3   - Load complex sequence\r\n"
                                   "p/P - Print sequence state\r\n"
                                   "l/L - Test LEDs\r\n"
                                   "b/B - Change BPM\r\n"
                                   "h/H - Show this help\r\n"
                                   "==================\r\n");
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);
}
