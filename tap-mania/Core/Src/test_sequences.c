/**
 ******************************************************************************
 * @file           : test_sequences.c
 * @brief          : Test Sequence Data and Loading Implementation
 ******************************************************************************
 */

#include "test_sequences.h"
#include "button_handler.h"
#include "led_control.h"
#include "scoring.h"
#include "uart_debug.h"
#include <stdio.h>

extern volatile uint16_t current_bpm;
extern uint32_t sequence_length;
extern LEDControl led_control;
extern uint32_t beat_led_on_tick;
extern UART_HandleTypeDef huart2;

// Test sequences - GRBY order (Green, Red, Blue, Yellow)
static const uint8_t test_sequence_simple[][4] = {
    {1, 0, 0, 0}, // Beat 1: Green press
    {0, 1, 0, 0}, // Beat 2: Red press
    {0, 0, 1, 0}, // Beat 3: Blue press
    {0, 0, 0, 1}, // Beat 4: Yellow press
    {1, 1, 0, 0}, // Beat 5: Green + Red press
    {0, 0, 1, 1}, // Beat 6: Blue + Yellow press
    {1, 1, 1, 1}, // Beat 7: All buttons press
    {0, 0, 0, 0}, // Beat 8: Rest
};

static const uint8_t test_sequence_holds[][4] = {
    {0, 0, 0, 0}, // Beat 1: Rest
    {2, 0, 0, 0}, // Beat 2: Green hold start
    {2, 0, 0, 0}, // Beat 3: Green hold continue
    {2, 0, 0, 0}, // Beat 4: Green hold end
    {0, 2, 2, 0}, // Beat 5: Red + Blue hold start
    {0, 2, 2, 0}, // Beat 6: Red + Blue hold continue
    {1, 0, 0, 1}, // Beat 7: Green + Yellow press
    {0, 0, 0, 0}, // Beat 8: Rest
};

static const uint8_t test_sequence_complex[][4] = {
    {0, 0, 0, 0}, // Beat 1: Rest
    {1, 0, 1, 0}, // Beat 2: Green + Blue press
    {0, 0, 0, 0}, // Beat 3: Rest
    {0, 1, 0, 1}, // Beat 4: Red + Yellow press
    {2, 0, 0, 0}, // Beat 5: Green hold start
    {2, 1, 0, 0}, // Beat 6: Green hold + Red press
    {2, 0, 1, 0}, // Beat 7: Green hold + Blue press
    {0, 0, 2, 2}, // Beat 8: Blue + Yellow hold start (Green released)
    {0, 1, 2, 2}, // Beat 9: Red press + Blue/Yellow hold
    {1, 1, 0, 0}, // Beat 10: Green + Red press (Blue/Yellow released)
    {0, 0, 0, 0}, // Beat 11: Rest
    {1, 1, 1, 1}, // Beat 12: All buttons press
    {1, 1, 1, 1}, // Beat 13: All buttons press
    {0, 0, 0, 0}, // Beat 14: Rest
    {0, 0, 0, 0}, // Beat 15: Rest
    {0, 0, 0, 0}, // Beat 16: Rest
    {1, 1, 1, 1}, // Beat 17: All buttons press
    {1, 1, 1, 1}, // Beat 18: All buttons press
    {1, 1, 1, 1}, // Beat 19: All buttons press
    {2, 0, 0, 0}, // Beat 20: Green hold start
    {2, 1, 0, 0}, // Beat 21: Green hold + Red press
    {2, 0, 0, 0}, // Beat 22: Green hold
    {2, 1, 0, 0}, // Beat 23: Green hold + Red press
    {2, 0, 0, 0}, // Beat 24: Green hold
    {2, 1, 0, 0}, // Beat 25: Green hold + Red press
    {2, 0, 0, 0}, // Beat 26: Green hold
    {2, 1, 0, 0}, // Beat 27: Green hold + Red press
};

static const uint8_t test_sequence_all[][4] = {
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
};

/**
 * @brief Load a test sequence
 */
void LoadTestSequence(uint8_t sequence_num) {
  char uart_buf[128];
  int uart_buf_len;

  Sequence_Init();
  Scoring_Reset();
  ButtonHandler_Init();

  const char *sequence_name = "";

  switch (sequence_num) {
  case 1:
    Sequence_LoadFromArray(test_sequence_simple, 8);
    sequence_length = 8;
    Sequence_SetMetadata("Simple Test", "System", current_bpm, 0, "EASY");
    sequence_name = "SIMPLE";
    break;

  case 2:
    Sequence_LoadFromArray(test_sequence_holds, 8);
    sequence_length = 8;
    Sequence_SetMetadata("Hold Test", "System", current_bpm, 0, "MEDIUM");
    sequence_name = "HOLDS";
    break;

  case 3:
    Sequence_LoadFromArray(test_sequence_complex, 27);
    sequence_length = 27;
    Sequence_SetMetadata("Complex Test", "System", current_bpm, 0, "HARD");
    sequence_name = "COMPLEX";
    break;

  case 4:
    Sequence_LoadFromArray(test_sequence_all, 100);
    sequence_length = Sequence_GetMetadata()->total_beats;

    uart_buf_len =
        sprintf(uart_buf,
                "Test sequence %u loaded: %lu beats (includes 16 lead-in)\r\n",
                sequence_num, sequence_length);
    UART_Transmit((uint8_t *)uart_buf, uart_buf_len);

    Sequence_SetMetadata("All Press Test", "System", current_bpm, 0, "HARD");
    sequence_name = "ALL PRESS";
    break;

  default:
    Sequence_LoadFromArray(test_sequence_simple, 8);
    sequence_length = 8;
    Sequence_SetMetadata("Default Test", "System", current_bpm, 0, "EASY");
    sequence_name = "DEFAULT";
    break;
  }

  uart_buf_len =
      sprintf(uart_buf, "\r\n=== Loaded %s sequence (%lu beats) ===\r\n",
              sequence_name, sequence_length);
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);

  Sequence_PrintCurrentState();

  extern volatile uint32_t beat_count;
  beat_count = 0;
  uint32_t ok_radius_ms = Scoring_GetOkRadius();

  if (sequence_length > 0) {
    uint32_t beat_period_ms = (60 * 1000) / current_bpm;
    uint32_t first_beat_timestamp = HAL_GetTick() + beat_period_ms;

    BeatData *first_beat_data = Sequence_GetBeatData(1);
    if (first_beat_data) {
      for (int i = 0; i < BUTTON_COUNT; i++) {
        bool is_press = first_beat_data->actions[i] == SEQ_ACTION_PRESS;
        bool is_hold_start = first_beat_data->actions[i] == SEQ_ACTION_HOLD;
        if (is_press || is_hold_start) {
          led_control.led_on_tick[i] = first_beat_timestamp - ok_radius_ms;
          led_control.led_scheduled[i] = true;
        }
      }
    }
#define BEAT_LED_BEFORE_MS 50
    beat_led_on_tick = first_beat_timestamp - BEAT_LED_BEFORE_MS;
  }
}
