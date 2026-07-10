/**
 ******************************************************************************
 * @file           : beat_event.c
 * @brief          : Beat Event Processing Implementation
 ******************************************************************************
 */

#include "beat_event.h"
#include "SH1106.h"
#include "button_handler.h"
#include "game_state.h"
#include "led_control.h"
#include "led_display.h"
#include "scoring.h"
#include "sequence.h"
#include "uart_debug.h"

#include <stdio.h>

extern volatile bool beat_event_flag;
extern volatile uint32_t beat_count;
extern volatile uint16_t current_bpm;
extern uint32_t sequence_length;
extern UART_HandleTypeDef huart2;
extern LEDControl led_control;
extern uint32_t beat_led_on_tick;
extern uint32_t beat_led_off_tick;

#define BEAT_LED_BEFORE_MS 50
#define BEAT_LED_AFTER_MS 50

/**
 * @brief Process beat event
 */
void ProcessBeatEvent(TSQMetadata tsq_metadata) {
  if (!beat_event_flag)
    return;
  if (beat_count % 10 == 1) {
    myprintf("BEAT_EVENT_DEBUG: Beat %lu, IsHalfBeat()=%d\r\n", beat_count,
             Sequence_IsHalfBeat());
  }

  beat_event_flag = false;
  uint32_t current_timestamp_ms = HAL_GetTick();
  char uart_buf[256];
  int uart_buf_len;
  uint16_t spacing = 2;
  uint16_t h = Font_7x10.FontHeight;
  // Advance sequence to keep in sync with timer-driven beat_count
  Sequence_AdvanceBeat();

  // Update LED matrix
  LEDDisplay_UpdateOnBeat(beat_count);

  // Update hold state and check for missed actions
  Sequence_UpdateHoldState(beat_count);
  if (beat_count > 1) {
    ButtonHandler_CheckMissedBeats(beat_count);
  }

  // LED Scheduling Logic
  uint32_t beat_period_ms = (60 * 1000) / current_bpm;
  uint32_t ok_radius_ms = Scoring_GetOkRadius();
  // *** NEW: Determine if this is an "on-beat" for the beat LED ***
  // An on-beat is:
  // 1. Any beat in full-beat mode.
  // 2. Odd-numbered beats (1, 3, 5...) in half-beat mode.
  bool is_on_beat = !Sequence_IsHalfBeat() || (beat_count % 2 == 0);

  // 1. Schedule OFF times for CURRENT beat
  // *** MODIFIED: Only schedule beat LED off on an "on-beat" ***
  if (is_on_beat) {
    beat_led_off_tick = current_timestamp_ms + BEAT_LED_AFTER_MS;
  }
  BeatData *current_beat_data = Sequence_GetBeatData(beat_count);
  if (current_beat_data) {
    for (int i = 0; i < BUTTON_COUNT; i++) {
      BeatData *next_beat_data = Sequence_GetBeatData(beat_count + 1);

      bool is_press = (current_beat_data->actions[i] == SEQ_ACTION_PRESS);
      bool is_hold_end = (current_beat_data->actions[i] == SEQ_ACTION_HOLD) &&
                         (next_beat_data == NULL ||
                          next_beat_data->actions[i] != SEQ_ACTION_HOLD);

      if (is_press || is_hold_end) {
        led_control.led_off_tick[i] = current_timestamp_ms + ok_radius_ms;
      }
    }
  }

  // 2. Schedule ON times for NEXT beat
  uint32_t next_beat_num = beat_count + 1;

  if (next_beat_num <= sequence_length) {
    uint32_t next_beat_judgment_time = current_timestamp_ms + beat_period_ms;
    bool next_is_on_beat = !Sequence_IsHalfBeat() || (next_beat_num % 2 == 0);
    // Only schedule beat LED on for an "on-beat
    if (next_is_on_beat) {
      beat_led_on_tick = next_beat_judgment_time - BEAT_LED_BEFORE_MS;
    }

    BeatData *next_beat_data = Sequence_GetBeatData(next_beat_num);
    if (next_beat_data) {
      for (int i = 0; i < BUTTON_COUNT; i++) {
        bool is_press = (next_beat_data->actions[i] == SEQ_ACTION_PRESS);
        bool is_hold_start = (next_beat_data->actions[i] == SEQ_ACTION_HOLD) &&
                             (current_beat_data == NULL ||
                              current_beat_data->actions[i] != SEQ_ACTION_HOLD);

        if (is_press || is_hold_start) {
          led_control.led_on_tick[i] = next_beat_judgment_time - ok_radius_ms;
          led_control.led_scheduled[i] = true;
        }
      }
    }
  }

  // Print beat information
  BeatData *current_beat_for_print = Sequence_GetBeatData(beat_count);
  if (current_beat_for_print) {
    extern volatile uint32_t last_beat_timestamp;
    uart_buf_len = sprintf(
        uart_buf,
        "\r\n*** BEAT %lu @ %lu us (BPM: %u) [G:%d R:%d B:%d Y:%d] ***\r\n",
        beat_count, last_beat_timestamp, (unsigned int)current_bpm,
        current_beat_for_print->actions[BUTTON_ID_GREEN],
        current_beat_for_print->actions[BUTTON_ID_RED],
        current_beat_for_print->actions[BUTTON_ID_BLUE],
        current_beat_for_print->actions[BUTTON_ID_YELLOW]);
  } else {
    extern volatile uint32_t last_beat_timestamp;
    uart_buf_len =
        sprintf(uart_buf,
                "\r\n*** BEAT %lu @ %lu us (BPM: %u) [END OF SEQUENCE] ***\r\n",
                beat_count, last_beat_timestamp, (unsigned int)current_bpm);
  }
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);

  // Check if sequence is complete
  if (Sequence_IsComplete()) {
    extern TIM_HandleTypeDef htim6, htim7, htim2;

    // Turn off all LEDs
    HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(YELLOW_LED_GPIO_Port, YELLOW_LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BEAT_LED_GPIO_Port, BEAT_LED_Pin, GPIO_PIN_SET);

    LEDDisplay_Clear();

    uart_buf_len = sprintf(uart_buf,
                           "\r\n=== SEQUENCE COMPLETE ===\r\n"
                           "Final Score: %ld\r\n"
                           "Max Combo: %lu\r\n"
                           "========================\r\n",
                           Scoring_GetTotal(), Scoring_GetMaxCombo());
    UART_Transmit((uint8_t *)uart_buf, uart_buf_len);

    char totalScoreBuf[18];
    char maxComboBuf[18];
    char diffBuf[18];
    char bpmBuf[18];
    char durationBuf[18];

    snprintf(totalScoreBuf, sizeof totalScoreBuf, "Final Score: %d",
             Scoring_GetTotal());
    snprintf(maxComboBuf, sizeof maxComboBuf, "Max Combo: %d",
             Scoring_GetMaxCombo());
    //  snprintf(diffBuf, sizeof songNameBuf, "Diff: %s", metadata->difficulty);
    //  snprintf(bpmBuf, sizeof songNameBuf, "BPM: %u", metadata->file_bpm);
    //  snprintf(durationBuf, sizeof songNameBuf, "Duration: %02lu:%02lu",
    //  minutes, seconds);
    SH1106_Fill(SH1106_COLOR_BLACK);

    SH1106_GotoXY(1, 0);
    SH1106_Puts(totalScoreBuf, &Font_7x10, 1);

    SH1106_GotoXY(1, h + spacing);
    SH1106_Puts(maxComboBuf, &Font_7x10, 1);
    SH1106_UpdateScreen();
    InitLEDControl();

    // Stop game timers
    HAL_TIM_Base_Stop_IT(&htim6);
    HAL_TIM_Base_Stop_IT(&htim7);
    HAL_TIM_Base_Stop(&htim2);

    ChangeSystemState(SYSTEM_STATE_COMPLETE);
  }
}

/**
 * @brief Initialize first beat LED scheduling
 */
void PrimeFirstBeatLEDs(uint32_t seq_length) {
  if (seq_length == 0) {
    myprintf("Warning: Sequence length is 0, cannot prime LEDs.\r\n");
    return;
  }

  uint32_t current_tick_init = HAL_GetTick();
  uint32_t beat_period_ms = (60 * 1000) / current_bpm;
  uint32_t first_beat_judgment_time = current_tick_init + beat_period_ms;
  uint32_t ok_radius_ms = Scoring_GetOkRadius();

  BeatData *first_beat_data = Sequence_GetBeatData(1);
  if (first_beat_data) {
    myprintf("Priming LEDs for Beat 1 (Est. @ %lu ms):\r\n",
             first_beat_judgment_time);
    for (int i = 0; i < BUTTON_COUNT; i++) {
      bool is_press = (first_beat_data->actions[i] == SEQ_ACTION_PRESS);
      bool is_hold_start = (first_beat_data->actions[i] == SEQ_ACTION_HOLD);

      if (is_press || is_hold_start) {
        led_control.led_on_tick[i] = first_beat_judgment_time - ok_radius_ms;
        led_control.led_scheduled[i] = true;
        myprintf("  Button %d: ON scheduled @ %lu ms\r\n", i,
                 led_control.led_on_tick[i]);
      }
    }
  } else {
    myprintf("Warning: No data found for Beat 1 during priming.\r\n");
  }

  beat_led_on_tick = first_beat_judgment_time - BEAT_LED_BEFORE_MS;
  myprintf("  Beat LED: ON scheduled @ %lu ms\r\n", beat_led_on_tick);
}
