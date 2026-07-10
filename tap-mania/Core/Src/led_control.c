/**
 ******************************************************************************
 * @file           : led_control.c
 * @brief          : LED Control Functions Implementation
 ******************************************************************************
 */

#include "led_control.h"
#include "scoring.h"

extern LEDControl led_control;
extern uint32_t beat_led_on_tick;
extern uint32_t beat_led_off_tick;

/**
 * @brief Set LED state for a button
 */
void SetButtonLED(ArcadeButtonID button_id, bool state) {
  GPIO_PinState pin_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;

  switch (button_id) {
  case BUTTON_ID_GREEN:
    HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, pin_state);
    break;
  case BUTTON_ID_RED:
    HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, pin_state);
    break;
  case BUTTON_ID_BLUE:
    HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, pin_state);
    break;
  case BUTTON_ID_YELLOW:
    HAL_GPIO_WritePin(YELLOW_LED_GPIO_Port, YELLOW_LED_Pin, pin_state);
    break;
  }

  led_control.led_is_on[button_id] = state;
}

/**
 * @brief Initialize LED control system
 */
void InitLEDControl(void) {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    led_control.led_on_tick[i] = 0;
    led_control.led_off_tick[i] = 0;
    led_control.led_scheduled[i] = false;
    led_control.led_is_on[i] = false;
    SetButtonLED(i, false);
  }
  HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(YELLOW_LED_GPIO_Port, YELLOW_LED_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(BEAT_LED_GPIO_Port, BEAT_LED_Pin, GPIO_PIN_SET);
  beat_led_on_tick = 0;
  beat_led_off_tick = 0;
}

/**
 * @brief Process scheduled LED events
 */
void ProcessLEDSchedule(void) {
  uint32_t current_tick = HAL_GetTick();

  for (int button = 0; button < BUTTON_COUNT; button++) {
    // Process ON Events
    if (led_control.led_scheduled[button]) {
      if (!led_control.led_is_on[button] &&
          led_control.led_on_tick[button] > 0 &&
          current_tick >= led_control.led_on_tick[button]) {

        SetButtonLED(button, true);
        led_control.led_on_tick[button] = 0;
        led_control.led_scheduled[button] = false;
      }
    }

    // Process OFF Events
    if (led_control.led_is_on[button] && led_control.led_off_tick[button] > 0 &&
        current_tick >= led_control.led_off_tick[button]) {

      SetButtonLED(button, false);
      led_control.led_off_tick[button] = 0;
    }
  }
}

/**
 * @brief Calculate timestamp for a future beat
 */
uint32_t CalculateBeatTimestamp(uint32_t beat_number, uint32_t current_beat_num,
                                uint32_t current_beat_timestamp_ms) {
  if (beat_number <= current_beat_num) {
    return current_beat_timestamp_ms;
  }

  extern volatile uint16_t current_bpm;
  uint32_t beats_ahead = beat_number - current_beat_num;
  uint32_t beat_period_ms = (60 * 1000) / current_bpm;
  return current_beat_timestamp_ms + (beats_ahead * beat_period_ms);
}

/**
 * @brief Process beat LED on/off scheduling
 */
void ProcessBeatLED(void) {
  uint32_t current_loop_tick = HAL_GetTick();

  // Check if it's time to turn the beat LED on
  if (beat_led_on_tick > 0 && current_loop_tick >= beat_led_on_tick) {
    HAL_GPIO_WritePin(BEAT_LED_GPIO_Port, BEAT_LED_Pin, GPIO_PIN_RESET);
    beat_led_on_tick = 0;
  }

  // Check if it's time to turn the beat LED off
  if (beat_led_off_tick > 0 && current_loop_tick >= beat_led_off_tick) {
    HAL_GPIO_WritePin(BEAT_LED_GPIO_Port, BEAT_LED_Pin, GPIO_PIN_SET);
    beat_led_off_tick = 0;
  }
}

/**
 * @brief Schedule LED timings for next beat
 */
void ScheduleNextBeatLEDs(uint32_t next_beat_num,
                          uint32_t next_beat_judgment_time,
                          BeatData *next_beat_data,
                          BeatData *current_beat_data) {
  extern LEDControl led_control;
  extern uint32_t beat_led_on_tick;
  extern volatile uint16_t current_bpm;

  uint32_t ok_radius_ms = Scoring_GetOkRadius();
  uint32_t beat_period_ms = (60 * 1000) / current_bpm;

// Schedule beat LED
#define BEAT_LED_BEFORE_MS 50
  beat_led_on_tick = next_beat_judgment_time - BEAT_LED_BEFORE_MS;

  // Schedule arcade button LEDs
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

/**
 * @brief Schedule LED off timings for current beat
 */
void ScheduleCurrentBeatLEDsOff(uint32_t current_beat_num,
                                uint32_t current_timestamp_ms,
                                BeatData *current_beat_data) {
  extern LEDControl led_control;
  extern uint32_t beat_led_off_tick;

  uint32_t ok_radius_ms = Scoring_GetOkRadius();

#define BEAT_LED_AFTER_MS 50
  beat_led_off_tick = current_timestamp_ms + BEAT_LED_AFTER_MS;

  if (current_beat_data) {
    for (int i = 0; i < BUTTON_COUNT; i++) {
      extern uint32_t sequence_length;
      BeatData *next_beat_data = Sequence_GetBeatData(current_beat_num + 1);

      bool is_press = (current_beat_data->actions[i] == SEQ_ACTION_PRESS);
      bool is_hold_end = (current_beat_data->actions[i] == SEQ_ACTION_HOLD) &&
                         (next_beat_data == NULL ||
                          next_beat_data->actions[i] != SEQ_ACTION_HOLD);

      if (is_press || is_hold_end) {
        led_control.led_off_tick[i] = current_timestamp_ms + ok_radius_ms;
      }
    }
  }
}
