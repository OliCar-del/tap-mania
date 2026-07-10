/**
 ******************************************************************************
 * @file           : led_display.c
 * @brief          : LED Matrix Display Manager Implementation
 ******************************************************************************
 */

#include "led_display.h"
#include "ARGB.h"
#include "sequence.h"
#include <string.h>

/* Private Variables */
static DisplayWindow display_window;

/* External UART for debug (if needed) */
extern UART_HandleTypeDef huart2;

/* Button Color Definitions */
static const RGBColor COLOR_RED = {255, 0, 0};
static const RGBColor COLOR_GREEN = {0, 255, 0};
static const RGBColor COLOR_BLUE = {0, 0, 255};
static const RGBColor COLOR_YELLOW = {255, 255, 0};
static const RGBColor COLOR_WHITE = {255, 255, 255};
static const RGBColor COLOR_BLACK = {0, 0, 0};

/**
 * @brief Column offset lookup table
 */
static const uint16_t COLUMN_OFFSETS[BUTTON_COUNT] = {
    RED_COLUMN_OFFSET,   // BUTTON_ID_RED
    GREEN_COLUMN_OFFSET, // BUTTON_ID_GREEN
    BLUE_COLUMN_OFFSET,  // BUTTON_ID_BLUE
    YELLOW_COLUMN_OFFSET // BUTTON_ID_YELLOW
};

/**
 * @brief Initialize the LED display system
 */
void LEDDisplay_Init(void) {
  // Initialize ARGB LED driver
  ARGB_Init();
  ARGB_SetBrightness(50); // Set to 50% brightness (adjustable)

  // Clear the window buffer
  memset(&display_window, 0, sizeof(DisplayWindow));
  display_window.initialized = true;
  display_window.current_beat = 0;
  display_window.beats_visible = 0;

  // Clear the physical LED matrix
  LEDDisplay_Clear();
}

/**
 * @brief Load initial beat window from sequence
 * FIXED: Load beats in REVERSE order so nearest beat is at bottom
 * Row 0 (top) = furthest future beat (will arrive in DISPLAY_ROWS beat periods)
 * Row 15 (bottom) = nearest future beat (will arrive in 1 beat period)
 */
void LEDDisplay_LoadInitialWindow(void) {
  if (!display_window.initialized) {
    return;
  }

  // Get the current beat number from sequence
  uint32_t start_beat = Sequence_GetCurrentBeatNumber();

  // Load DISPLAY_ROWS beats into the window IN REVERSE ORDER
  // This makes beats scroll from top (far future) to bottom (near future)
  // toward judgment line
  for (uint8_t row = 0; row < DISPLAY_ROWS; row++) {
    // FIXED: Reverse the beat order
    // Row 0 gets beat (start_beat + DISPLAY_ROWS - 1)
    // Row 15 gets beat (start_beat + 0)
    uint32_t beat_num = start_beat + (DISPLAY_ROWS - 1 - row);
    BeatData *beat = Sequence_GetBeatData(beat_num);

    if (beat != NULL) {
      // Copy beat actions into window
      for (uint8_t col = 0; col < BUTTON_COUNT; col++) {
        display_window.window[row][col] = beat->actions[col];
      }
      display_window.beats_visible = row + 1;
    } else {
      // No more beats - fill with NONE
      for (uint8_t col = 0; col < BUTTON_COUNT; col++) {
        display_window.window[row][col] = SEQ_ACTION_NONE;
      }
    }
  }

  display_window.current_beat = start_beat;

  // Render the initial window
  LEDDisplay_Render();
}

/**
 * @brief Update display on beat interrupt
 * FIXED: Load beat at current_beat + DISPLAY_ROWS (not - 1)
 */
void LEDDisplay_UpdateOnBeat(uint32_t current_beat) {
  if (!display_window.initialized) {
    return;
  }

  // Shift window down by one row (scroll effect)
  // Move rows from bottom to top to avoid overwriting data
  for (int8_t row = DISPLAY_ROWS - 1; row > 0; row--) {
    for (uint8_t col = 0; col < BUTTON_COUNT; col++) {
      display_window.window[row][col] = display_window.window[row - 1][col];
    }
  }

  // Load the next beat into the top row (row 0)
  // FIXED: Should be current_beat + DISPLAY_ROWS, not DISPLAY_ROWS - 1
  // When beat N arrives, beat N is at judgment line (row 16)
  // Beat N+DISPLAY_ROWS should be loaded at row 0 (top of display)
  uint32_t next_beat_num =
      current_beat + DISPLAY_ROWS; // Beat that should appear at top
  BeatData *next_beat = Sequence_GetBeatData(next_beat_num);

  if (next_beat != NULL) {
    // Copy next beat actions to top row
    for (uint8_t col = 0; col < BUTTON_COUNT; col++) {
      display_window.window[0][col] = next_beat->actions[col];
    }
  } else {
    // Sequence complete - insert blank row at top
    for (uint8_t col = 0; col < BUTTON_COUNT; col++) {
      display_window.window[0][col] = SEQ_ACTION_NONE;
    }
  }

  display_window.current_beat = current_beat;

  // Render the updated window
  LEDDisplay_Render();
}

/**
 * @brief Render the entire window to the LED matrix
 */
void LEDDisplay_Render(void) {
  // Wait for ARGB driver to be ready
  while (ARGB_Ready() == ARGB_BUSY) {
    // Spin until DMA is ready
  }

  // Render each row of the window
  for (uint8_t row = 0; row < DISPLAY_ROWS; row++) {
    for (uint8_t col = 0; col < BUTTON_COUNT; col++) {
      SequenceAction action = display_window.window[row][col];
      RGBColor color =
          LEDDisplay_GetColorForAction((ArcadeButtonID)col, action);
      LEDDisplay_SetPixel(row, col, color);
    }
  }

  // Send the frame to the LED matrix
  while (ARGB_Ready() == ARGB_BUSY) {
    // Wait for any pending transfer to complete
  }
  ARGB_Show();
}

/**
 * @brief Clear the entire LED matrix
 */
void LEDDisplay_Clear(void) {
  // Clear the window buffer
  memset(display_window.window, 0, sizeof(display_window.window));
  display_window.beats_visible = 0;

  // Clear all LEDs
  ARGB_Clear();

  // Wait for ARGB to be ready and show the cleared state
  while (ARGB_Ready() == ARGB_BUSY) {
    // Spin
  }
  ARGB_Show();
}

/**
 * @brief Set a single LED's color
 */
void LEDDisplay_SetPixel(uint8_t row, uint8_t column, RGBColor color) {
  if (row >= DISPLAY_ROWS || column >= BUTTON_COUNT) {
    return; // Out of bounds
  }

  uint16_t led_index = LEDDisplay_GetLEDIndex(row, column);
  ARGB_SetRGB(led_index, color.r, color.g, color.b);
}

/**
 * @brief Get the color for a button based on sequence action
 */
RGBColor LEDDisplay_GetColorForAction(ArcadeButtonID button_id,
                                      SequenceAction action) {
  if (action == SEQ_ACTION_NONE) {
    return COLOR_BLACK; // LED off
  }

  if (action == SEQ_ACTION_HOLD) {
    return COLOR_WHITE; // Hold actions show as white
  }

  // SEQ_ACTION_PRESS - show in button's native color
  switch (button_id) {
  case BUTTON_ID_RED:
    return COLOR_RED;
  case BUTTON_ID_GREEN:
    return COLOR_GREEN;
  case BUTTON_ID_BLUE:
    return COLOR_BLUE;
  case BUTTON_ID_YELLOW:
    return COLOR_YELLOW;
  default:
    return COLOR_BLACK;
  }
}

/**
 * @brief Convert matrix position to LED index
 */
uint16_t LEDDisplay_GetLEDIndex(uint8_t row, uint8_t column) {
  if (row >= DISPLAY_ROWS || column >= BUTTON_COUNT) {
    return 0; // Return safe default
  }

  // Calculate linear LED index:
  // Each column has a base offset, and we add row * ROW_STRIDE
  uint16_t led_index = COLUMN_OFFSETS[column] + (row * ROW_STRIDE);

  return led_index;
}

/**
 * @brief Get current display window state
 */
DisplayWindow *LEDDisplay_GetWindow(void) { return &display_window; }

/**
 * @brief Scroll window effect for game ending
 */
bool LEDDisplay_ScrollOutComplete(void) {
  // Check if all rows are empty (all SEQ_ACTION_NONE)
  for (uint8_t row = 0; row < DISPLAY_ROWS; row++) {
    for (uint8_t col = 0; col < BUTTON_COUNT; col++) {
      if (display_window.window[row][col] != SEQ_ACTION_NONE) {
        return false; // Still have visible beats
      }
    }
  }

  return true; // All rows cleared
}
