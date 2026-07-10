#include "button_handler.h"
#include "SH1106.h"
#include "game_state.h"
#include "scoring.h"
#include "sequence.h"
#include "uart_debug.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // For abs()
#include <string.h>

// Struct to hold data for a button event (press or release)
typedef struct {
  ArcadeButtonID button_id;
  uint32_t timestamp_us;
  bool is_press; // true for press, false for release
} ButtonEvent;

#define DEBOUNCE_MS 10 // For arcade game buttons (fast response needed)
#define UI_DEBOUNCE_MS 200
#define ACTION_QUEUE_SIZE 32

// --- Private Variables ---
static volatile ButtonEvent action_queue[ACTION_QUEUE_SIZE];
static volatile uint8_t queue_head = 0;
static volatile uint8_t queue_tail = 0;
static volatile uint32_t last_edge_tick[BUTTON_COUNT] = {0};
static volatile bool button_state[BUTTON_COUNT] = {false};
static volatile uint32_t dropped_event_count = 0;
static uint32_t last_press_up = 0;
static uint32_t last_press_down = 0;
static uint32_t last_press_right = 0;
static uint32_t last_press_left = 0;
static uint32_t last_press_select = 0;
static int selected_song = 0;
static bool scrolled = false;
static bool selectedNewSong = false;
static rightPressed = false;
bool pbSelPressed = false;
extern uint32_t GetMicrosecondTimestamp(void);

// Enhanced button state tracking
typedef struct {
  bool is_pressed;             // Physical button state
  uint32_t press_timestamp;    // When button was pressed
  uint32_t press_beat;         // Which beat the press was attributed to
  bool press_scored;           // Has the press been scored?
  bool expecting_hold_release; // Are we in a hold and waiting for release?
  uint32_t hold_end_beat;      // Which beat should the hold end on?
} ButtonPressState;

static ButtonPressState button_press_state[BUTTON_COUNT] = {{0}};
static bool beat_scored[BUTTON_COUNT][MAX_SEQUENCE_LENGTH] = {{false}};

// --- External variables from main.c ---
extern volatile uint32_t last_beat_timestamp;
extern volatile uint32_t beat_count;
extern volatile uint16_t current_bpm;
extern UART_HandleTypeDef huart2;

/**
 * @brief Initializes button handler variables.
 */
void ButtonHandler_Init(void) {
  queue_head = 0;
  queue_tail = 0;

  for (int i = 0; i < BUTTON_COUNT; i++) {
    button_state[i] = false;
    button_press_state[i].is_pressed = false;
    button_press_state[i].press_timestamp = 0;
    button_press_state[i].press_beat = 0;
    button_press_state[i].press_scored = false;
    button_press_state[i].expecting_hold_release = false;
    button_press_state[i].hold_end_beat = 0;

    for (int j = 0; j < MAX_SEQUENCE_LENGTH; j++) {
      beat_scored[i][j] = false;
    }
  }
}

/**
 * @brief Helper to determine if a note is the start of a hold sequence.
 */
static bool is_hold_start(ArcadeButtonID button, uint32_t beat_num) {
  BeatData *current = Sequence_GetBeatData(beat_num);
  if (!current || current->actions[button] != SEQ_ACTION_HOLD) {
    return false;
  }
  if (beat_num == 1) {
    return true;
  }
  BeatData *previous = Sequence_GetBeatData(beat_num - 1);
  return (!previous || previous->actions[button] != SEQ_ACTION_HOLD);
}

/**
 * @brief Find the end beat of a hold sequence starting at start_beat
 */
static uint32_t find_hold_end_beat(ArcadeButtonID button, uint32_t start_beat) {
  uint32_t end_beat = start_beat;
  uint32_t total_beats = Sequence_GetMetadata()->total_beats;

  // Find the last consecutive beat with HOLD action
  for (uint32_t beat = start_beat; beat <= total_beats; beat++) {
    BeatData *beat_data = Sequence_GetBeatData(beat);
    if (beat_data && beat_data->actions[button] == SEQ_ACTION_HOLD) {
      end_beat = beat;
    } else {
      break;
    }
  }

  return end_beat;
}

/**
 * @brief Fallback function to find the closest beat to a timestamp.
 */
static uint32_t GetBeatForTimestamp(uint32_t timestamp_us) {
  if (beat_count == 0)
    return 1;

  uint32_t beat_period_us = (60 * 1000000) / current_bpm;
  uint32_t current_beat_time = last_beat_timestamp;
  uint32_t next_beat_time = last_beat_timestamp + beat_period_us;

  int32_t delta_current = (int32_t)timestamp_us - (int32_t)current_beat_time;
  int32_t delta_next = (int32_t)timestamp_us - (int32_t)next_beat_time;

  return (abs(delta_current) <= abs(delta_next)) ? beat_count : beat_count + 1;
}

/**
 * @brief Helper to calculate the absolute timestamp of any beat.
 */
static uint32_t CalculateBeatTimestamp(uint32_t target_beat,
                                       uint32_t current_beat_num,
                                       uint32_t current_beat_timestamp_us) {
  if (target_beat <= current_beat_num) {
    return current_beat_timestamp_us;
  }

  uint32_t beat_period_us = (60 * 1000000) / current_bpm;
  int32_t beat_difference = (int32_t)target_beat - (int32_t)current_beat_num;
  return current_beat_timestamp_us + (beat_difference * beat_period_us);
}

/**
 * @brief Process a button press event with intelligent beat association.
 */
static void ProcessButtonPress(ButtonEvent event) {
  char uart_buf[256];
  char windowStrBuf[19];
  char comboStrBuf[19];
  char scoreStrBuf[19];
  int uart_buf_len;
  uint16_t spacing = 2;
  uint16_t h = Font_7x10.FontHeight;
  // Update physical state
  button_press_state[event.button_id].is_pressed = true;
  button_press_state[event.button_id].press_timestamp = event.timestamp_us;

  // --- Intelligent Beat Association Logic ---
  uint32_t current_beat_num = beat_count;
  uint32_t next_beat_num = beat_count + 1;
  uint32_t target_beat = 0;
  int32_t delta_ms = 0;

  const ScoringWindows *windows = Scoring_GetWindows();
  int32_t max_window_radius_us = windows->poor_radius_ms * 1000;

  // 1. Prioritize the CURRENT beat (the one that just passed).
  int32_t delta_vs_current_us =
      (int32_t)event.timestamp_us - (int32_t)last_beat_timestamp;
  BeatData *current_beat_data = Sequence_GetBeatData(current_beat_num);

  if (current_beat_num > 0 && current_beat_data &&
      !beat_scored[event.button_id][current_beat_num - 1] &&
      current_beat_data->actions[event.button_id] != SEQ_ACTION_NONE &&
      abs(delta_vs_current_us) <= max_window_radius_us) {
    target_beat = current_beat_num;
    delta_ms = delta_vs_current_us / 1000;
  }
  // 2. If not for the current beat, check the NEXT upcoming beat.
  else {
    uint32_t beat_period_us = (60 * 1000000) / current_bpm;
    uint32_t next_beat_timestamp = last_beat_timestamp + beat_period_us;
    int32_t delta_vs_next_us =
        (int32_t)event.timestamp_us - (int32_t)next_beat_timestamp;
    BeatData *next_beat_data = Sequence_GetBeatData(next_beat_num);

    if (next_beat_num <= Sequence_GetMetadata()->total_beats &&
        next_beat_data && !beat_scored[event.button_id][next_beat_num - 1] &&
        next_beat_data->actions[event.button_id] != SEQ_ACTION_NONE &&
        abs(delta_vs_next_us) <= max_window_radius_us) {
      target_beat = next_beat_num;
      delta_ms = delta_vs_next_us / 1000;
    }
  }

  // 3. If still no target, it's an extra press. Assign to closest beat for
  // logging.
  bool is_extra_press = (target_beat == 0);
  if (is_extra_press) {
    target_beat = GetBeatForTimestamp(event.timestamp_us);
    uint32_t target_beat_ts = CalculateBeatTimestamp(
        target_beat, current_beat_num, last_beat_timestamp);
    delta_ms = ((int32_t)event.timestamp_us - (int32_t)target_beat_ts) / 1000;
  }

  button_press_state[event.button_id].press_beat = target_beat;

  // --- Scoring Logic ---
  BeatData *beat_data = Sequence_GetBeatData(target_beat);
  //  if (!beat_data) {
  //      // No beat data - just log the press
  //      uart_buf_len = sprintf(uart_buf,
  //          "[%s] PRESS @ Beat %lu | Delta: %+ldms | NO SEQUENCE DATA\r\n",
  //          GetButtonName(event.button_id),
  //          target_beat,
  //          delta_ms);
  //      HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, uart_buf_len, 1000);
  //      return;
  //  }

  SequenceAction expected = beat_data->actions[event.button_id];
  ScoreRating rating = SCORE_RATING_MISS;
  const char *action_desc = "PRESS";
  bool should_score = false;

  // Check if this beat was already scored
  if (target_beat > 0 && beat_scored[event.button_id][target_beat - 1]) {
    action_desc = "PRESS (DUPLICATE)";
    rating = SCORE_RATING_MISS;
    should_score = true;
  }
  // Check if this is a hold start
  else if (is_hold_start(event.button_id, target_beat)) {
    action_desc = "HOLD START";
    rating = Scoring_CalculateRating(delta_ms);
    should_score = true;
    beat_scored[event.button_id][target_beat - 1] = true;
    button_press_state[event.button_id].press_scored = true;

    // Mark that we're expecting a hold release
    button_press_state[event.button_id].expecting_hold_release = true;
    button_press_state[event.button_id].hold_end_beat =
        find_hold_end_beat(event.button_id, target_beat);

    // Also update the sequence hold state
    HoldState *hold_state = Sequence_GetHoldState();
    hold_state->is_holding[event.button_id] = true;
    hold_state->hold_start_beat[event.button_id] = target_beat;
    hold_state->hold_end_beat[event.button_id] =
        button_press_state[event.button_id].hold_end_beat;
    hold_state->hold_scored_press[event.button_id] = true;
    hold_state->hold_scored_release[event.button_id] = false;
  }
  // Check if this is a normal press
  else if (expected == SEQ_ACTION_PRESS) {
    action_desc = "PRESS";
    rating = Scoring_CalculateRating(delta_ms);
    should_score = true;
    beat_scored[event.button_id][target_beat - 1] = true;
    button_press_state[event.button_id].press_scored = true;
    button_press_state[event.button_id].expecting_hold_release = false;
  }
  // Wrong button or extra press
  else {
    action_desc = is_extra_press ? "PRESS (EXTRA)" : "PRESS (WRONG)";
    rating = SCORE_RATING_MISS;
    should_score = true;
  }

  // Update score if needed
  if (should_score) {
    Scoring_Update(rating);
  }

  // Log the action
  //  uart_buf_len = sprintf(uart_buf,
  //      "[%s] %s @ Beat %lu | Delta: %+ldms | %s | Combo: %lu | Score:
  //      %ld\r\n", GetButtonName(event.button_id), action_desc, target_beat,
  //      delta_ms,
  //      Scoring_GetText(rating),
  //      Scoring_GetCombo(),
  //      Scoring_GetTotal());
  uart_buf_len =
      sprintf(uart_buf, "%s Combo: %lu Score: %ld\r\n", Scoring_GetText(rating),
              Scoring_GetCombo(), Scoring_GetTotal());
  //         char windowStrBuf[19];
  // char comboStrBuf[19];
  // char scoreStrBuf[19];
  //    // Game config page title
  SH1106_Fill(SH1106_COLOR_BLACK);

  /* Update screen */
  SH1106_UpdateScreen();
  sprintf(windowStrBuf, "%.18s", uart_buf);
  SH1106_GotoXY(1, 0);
  SH1106_Puts(windowStrBuf, &Font_7x10, 1);

  // Current windows header
  sprintf(comboStrBuf, "%.15s", uart_buf + 18);
  SH1106_GotoXY(1, h + spacing);
  SH1106_Puts(comboStrBuf, &Font_7x10, 1);

  // Perfect and Good window parameters
  // sprintf(scoreStrBuf,"%.18s", uart_buf + 35);
  // SH1106_GotoXY(1, 2 * (h+spacing));
  // SH1106_Puts(scoreStrBuf, &Font_7x10, 1);
  SH1106_UpdateScreen();
  //  HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, uart_buf_len, 1000);
}

/**
 * @brief Process a button release event.
 */
static void ProcessButtonRelease(ButtonEvent event) {
  char uart_buf[256];
  int uart_buf_len;

  if (!button_press_state[event.button_id].is_pressed) {
    // Spurious release without a corresponding press, ignore.
    return;
  }

  // Update physical state
  button_press_state[event.button_id].is_pressed = false;

  // Check if we're in a hold and expecting a release
  if (button_press_state[event.button_id].expecting_hold_release) {
    uint32_t release_beat = GetBeatForTimestamp(event.timestamp_us);
    uint32_t expected_release_beat =
        button_press_state[event.button_id].hold_end_beat;

    // Calculate timing relative to expected release beat
    uint32_t expected_release_time = CalculateBeatTimestamp(
        expected_release_beat, beat_count, last_beat_timestamp);
    int32_t delta_ms =
        ((int32_t)event.timestamp_us - (int32_t)expected_release_time) / 1000;

    ScoreRating rating;
    const char *release_desc;

    // Determine if release timing is correct
    const ScoringWindows *windows = Scoring_GetWindows();
    int32_t max_window_radius_ms = windows->poor_radius_ms;

    if (release_beat == expected_release_beat &&
        abs(delta_ms) <= max_window_radius_ms) {
      // Released at the right beat and within timing window
      rating = Scoring_CalculateRating(delta_ms);
      release_desc = "HOLD RELEASE";
    } else if (release_beat < expected_release_beat) {
      // Released too early (wrong beat)
      rating = SCORE_RATING_MISS;
      release_desc = "HOLD RELEASE (EARLY)";
    } else {
      // Released too late (wrong beat or too far after)
      rating = SCORE_RATING_MISS;
      release_desc = "HOLD RELEASE (LATE)";
    }

    // Score the release
    Scoring_Update(rating);

    // Clear hold state
    button_press_state[event.button_id].expecting_hold_release = false;
    button_press_state[event.button_id].hold_end_beat = 0;

    HoldState *hold_state = Sequence_GetHoldState();
    hold_state->is_holding[event.button_id] = false;
    hold_state->hold_scored_release[event.button_id] = true;

    uart_buf_len =
        sprintf(uart_buf,
                "[%s] %s @ Beat %lu (expected %lu) | Delta: %+ldms | %s | "
                "Combo: %lu | Score: %ld\r\n",
                GetButtonName(event.button_id), release_desc, release_beat,
                expected_release_beat, delta_ms, Scoring_GetText(rating),
                Scoring_GetCombo(), Scoring_GetTotal());
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
  } else {
    // Normal press release - just log it, no scoring
    uint32_t release_beat = GetBeatForTimestamp(event.timestamp_us);
    uint32_t hold_duration_us =
        event.timestamp_us -
        button_press_state[event.button_id].press_timestamp;

    uart_buf_len = sprintf(
        uart_buf, "[%s] RELEASE @ Beat %lu | Duration: %lu ms | Score: %ld\r\n",
        GetButtonName(event.button_id), release_beat, hold_duration_us / 1000,
        Scoring_GetTotal());
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
  }
}

/**
 * @brief Processes all pending button events in the queue.
 */
void ButtonHandler_ProcessQueue(void) {
  while (queue_tail != queue_head) {
    __disable_irq();
    ButtonEvent event = action_queue[queue_tail];
    queue_tail = (queue_tail + 1) % ACTION_QUEUE_SIZE;
    __enable_irq();

    if (event.is_press) {
      ProcessButtonPress(event);
    } else {
      ProcessButtonRelease(event);
    }
  }
}

/**
 * @brief Gets the name of a button from its ID.
 */
const char *GetButtonName(ArcadeButtonID id) {
  switch (id) {
  case BUTTON_ID_GREEN:
    return "GREEN";
  case BUTTON_ID_RED:
    return "RED";
  case BUTTON_ID_BLUE:
    return "BLUE";
  case BUTTON_ID_YELLOW:
    return "YELLOW";
  default:
    return "UNKNOWN";
  }
}

bool get_pb_sel(void) {
  myprintf("get_pb_sel ran\r\n");
  return pbSelPressed;
}
/**
 * @brief  EXTI Line Detection Callback (Button Edge ISR).
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {

  ArcadeButtonID button_id;
  GPIO_TypeDef *gpio_port;
  selectedNewSong = true;
  pbSelPressed = true;
  switch (GPIO_Pin) {
  case GREEN_PB_Pin:
    button_id = BUTTON_ID_GREEN;
    gpio_port = GREEN_PB_GPIO_Port;
    break;
  case RED_PB_Pin:
    button_id = BUTTON_ID_RED;
    gpio_port = RED_PB_GPIO_Port;
    break;
  case BLUE_PB_Pin:
    button_id = BUTTON_ID_BLUE;
    gpio_port = BLUE_PB_GPIO_Port;
    break;
  case YELLOW_PB_Pin:
    button_id = BUTTON_ID_YELLOW;
    gpio_port = YELLOW_PB_GPIO_Port;
    break;
  default:
    return;
  }
  // Debounce first to avoid unnecessary work in the ISR
  uint32_t current_tick = HAL_GetTick();
  if (current_tick - last_edge_tick[button_id] < DEBOUNCE_MS) {
    return;
  }
  last_edge_tick[button_id] = current_tick;

  uint32_t timestamp_us = GetMicrosecondTimestamp();
  bool is_pressed = (HAL_GPIO_ReadPin(gpio_port, GPIO_Pin) == GPIO_PIN_RESET);

  if (is_pressed == button_state[button_id]) {
    return; // No actual state change
  }
  button_state[button_id] = is_pressed;

  // Queue the event
  __disable_irq();
  uint8_t next_head = (queue_head + 1) % ACTION_QUEUE_SIZE;
  if (next_head != queue_tail) {
    action_queue[queue_head].button_id = button_id;
    action_queue[queue_head].timestamp_us = timestamp_us;
    action_queue[queue_head].is_press = is_pressed;
    queue_head = next_head;
  } else {
    dropped_event_count++; // Log that the queue was full
  }
  __enable_irq();
}

/**
 * @brief Check for missed beats and apply penalties.
 */
void ButtonHandler_CheckMissedBeats(uint32_t current_beat) {
  char uart_buf[128];
  int uart_buf_len;

  if (current_beat <= 1)
    return;

  uint32_t check_beat = current_beat - 1;
  BeatData *prev_beat = Sequence_GetBeatData(check_beat);
  if (!prev_beat)
    return;

  for (int button = 0; button < BUTTON_COUNT; button++) {
    // Check for a missed press
    if (prev_beat->actions[button] == SEQ_ACTION_PRESS &&
        !beat_scored[button][check_beat - 1]) {
      Scoring_Update(SCORE_RATING_MISS);
      beat_scored[button][check_beat - 1] =
          true; // Mark as scored to prevent double-counting
      uart_buf_len =
          sprintf(uart_buf, "[%s] MISSED PRESS on beat %lu | Score: %ld\r\n",
                  GetButtonName(button), check_beat, Scoring_GetTotal());
      HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    }
    // Check for a missed hold start
    else if (is_hold_start(button, check_beat) &&
             !beat_scored[button][check_beat - 1]) {
      Scoring_Update(SCORE_RATING_MISS);
      beat_scored[button][check_beat - 1] =
          true; // Mark as scored to prevent double-counting
      uart_buf_len = sprintf(
          uart_buf, "[%s] MISSED HOLD START on beat %lu | Score: %ld\r\n",
          GetButtonName(button), check_beat, Scoring_GetTotal());
      HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    }
  }
}

/**
 * @brief Get dropped event count for diagnostics
 */
uint32_t ButtonHandler_GetDroppedEventCount(void) {
  return dropped_event_count;
}

void poll_buttons(void) {
  uint32_t currentTick = HAL_GetTick();

  GPIO_PinState pbUp = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
  GPIO_PinState pbDown = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4);
  GPIO_PinState pbRight = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5);
  GPIO_PinState pbLeft = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8);
  GPIO_PinState pbSelect = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10);

  // --- UP (PA0) --- UI BUTTON: Use longer debounce
  if (pbUp == GPIO_PIN_RESET) { // Active low
    if (currentTick - last_press_up >
        UI_DEBOUNCE_MS) { // *** FIX: UI_DEBOUNCE_MS ***
      myprintf("UP Pressed\n\r");
      selected_song -= 1;
      if (selected_song < 0)
        selected_song = 0;
      scrolled = true;
      myprintf("selected_song is now %d\n", selected_song);
      last_press_up = currentTick;
    }
  }

  // --- DOWN (PA4) --- UI BUTTON: Use longer debounce
  if (pbDown == GPIO_PIN_RESET) {
    if (currentTick - last_press_down >
        UI_DEBOUNCE_MS) { // *** FIX: UI_DEBOUNCE_MS ***
      myprintf("DOWN Pressed\n\r");
      selected_song += 1;
      scrolled = true;
      myprintf("selected_song is now %d\n", selected_song);
      last_press_down = currentTick;
    }
  }

  // --- RIGHT (PB5) --- UI BUTTON: Use longer debounce
  if (pbRight == GPIO_PIN_RESET) {
    if (currentTick - last_press_right >
        UI_DEBOUNCE_MS) { // *** FIX: UI_DEBOUNCE_MS ***
      myprintf("RIGHT Pressed\n\r");
      rightPressed = true;
      last_press_right = currentTick;
    }
  }

  // --- LEFT (PB8) --- UI BUTTON: Use longer debounce
  if (pbLeft == GPIO_PIN_RESET) {
    if (currentTick - last_press_left >
        UI_DEBOUNCE_MS) { // *** FIX: UI_DEBOUNCE_MS ***
      myprintf("LEFT Pressed\n\r");
      last_press_left = currentTick;
    }
  }

  // --- SELECT (PB10) --- UI BUTTON: Use longer debounce
  if (pbSelect == GPIO_PIN_RESET) {
    if (currentTick - last_press_select >
        UI_DEBOUNCE_MS) { // *** FIX: UI_DEBOUNCE_MS ***
      myprintf("SELECT Pressed\n\r");
      pbSelPressed = true;
      selectedNewSong = true;
      last_press_select = currentTick;
    }
  }
}

void clear_press_flags(void) {
  pbSelPressed = false;
  rightPressed = false;
  scrolled = false;
  selectedNewSong = false;
}

bool get_pbsel_pressed(void) { return pbSelPressed; }

void set_pbsel_pressed(bool value) { pbSelPressed = value; }

int get_choose_song(void) { return selected_song; }

bool get_selected_song(void) { return selectedNewSong; }
bool get_scrolled(void) { return scrolled; }

bool get_press_right(void) { return rightPressed; }
