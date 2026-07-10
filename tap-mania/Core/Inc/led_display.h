/**
 ******************************************************************************
 * @file           : led_display.h
 * @brief          : LED Matrix Display Manager for TPMania
 ******************************************************************************
 * @attention
 *
 * This module handles rendering the scrolling beat window to the 16x4
 * WS2812B LED matrix. It maintains a sliding window of beats and updates
 * the display on each beat interrupt.
 *
 * Key features:
 * - Scrolling beat visualization
 * - Support for PRESS and HOLD actions
 * - Synchronized with beat timing
 * - Color-coded columns for each button
 *
 ******************************************************************************
 */

#ifndef LED_DISPLAY_H
#define LED_DISPLAY_H

#include "main.h"
#include "sequence.h"
#include <stdbool.h>
#include <stdint.h>

/* Display Configuration */
#define DISPLAY_ROWS 16 // Total rows on the LED matrix
#define DISPLAY_COLUMNS 4 // One column per button (RGBY)
#define JUDGMENT_LINE_ROW 16 // Row where notes are judged (arcade button)

/* LED Column Offsets (based on physical LED matrix layout) */
#define RED_COLUMN_OFFSET 0 // Red button column starts at LED 0
#define GREEN_COLUMN_OFFSET 16 // Green button column starts at LED 16
#define BLUE_COLUMN_OFFSET 32 // Blue button column starts at LED 32
#define YELLOW_COLUMN_OFFSET 48 // Yellow button column starts at LED 48

/* LED Rendering Constants */
#define ROW_STRIDE 1 // LED index increment per row
#define LED_BRIGHTNESS_MAX 255 // Maximum brightness value

/* Color Definitions (RGB values) */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGBColor;

/* Beat Window Structure
 * Maintains a scrolling window of upcoming beats to display on the matrix
 */
typedef struct {
    SequenceAction window[DISPLAY_ROWS][BUTTON_COUNT]; // Scrolling window buffer
    uint32_t current_beat; // Current beat number (1-indexed)
    uint32_t beats_visible; // Number of beats currently visible
    bool initialized; // Initialization flag
} DisplayWindow;

/**
 * @brief Initialize the LED display system
 *
 * Initializes the display window, clears the LED matrix, and prepares
 * the system for rendering. Must be called before any other display functions.
 * Also initializes the ARGB LED driver.
 */
void LEDDisplay_Init(void);

/**
 * @brief Load initial beat window from sequence
 *
 * Fills the display window with the first DISPLAY_ROWS beats from the
 * current sequence. This should be called after a sequence is loaded
 * but before playback starts.
 */
void LEDDisplay_LoadInitialWindow(void);

/**
 * @brief Update display on beat interrupt
 *
 * Called from the beat timer interrupt. Shifts the window down by one row,
 * loads the next beat from the sequence at the top, and renders the updated
 * window to the LED matrix.
 *
 * @param current_beat The current beat number (1-indexed)
 */
void LEDDisplay_UpdateOnBeat(uint32_t current_beat);

/**
 * @brief Render the entire window to the LED matrix
 *
 * Converts the current window buffer to LED pixel values and sends them
 * to the WS2812B matrix via the ARGB driver. This function waits for
 * DMA to be ready before updating.
 */
void LEDDisplay_Render(void);

/**
 * @brief Clear the entire LED matrix
 *
 * Turns off all LEDs and clears the window buffer. Useful for
 * game over or between songs.
 */
void LEDDisplay_Clear(void);

/**
 * @brief Set a single LED's color
 *
 * Helper function to set an individual LED by matrix position.
 *
 * @param row Row index (0-15)
 * @param column Column index (0-3, corresponding to RGBY)
 * @param color RGB color to set
 */
void LEDDisplay_SetPixel(uint8_t row, uint8_t column, RGBColor color);

/**
 * @brief Get the color for a button based on sequence action
 *
 * Returns the appropriate color for displaying a button's action:
 * - NONE: Black (LED off)
 * - PRESS: Button's native color
 * - HOLD: White (indicates sustained hold)
 *
 * @param button_id Button identifier (BUTTON_ID_RED, etc.)
 * @param action Sequence action for this beat
 * @return RGB color to display
 */
RGBColor LEDDisplay_GetColorForAction(ArcadeButtonID button_id, SequenceAction action);

/**
 * @brief Convert matrix position to LED index
 *
 * Converts a row/column position to the linear LED index for the
 * WS2812B chain. Handles the column offset mapping.
 *
 * @param row Row index (0-15)
 * @param column Column index (0-3)
 * @return Linear LED index for ARGB library
 */
uint16_t LEDDisplay_GetLEDIndex(uint8_t row, uint8_t column);

/**
 * @brief Get current display window state
 *
 * Returns a pointer to the current display window for debugging
 * or external monitoring.
 *
 * @return Pointer to DisplayWindow structure
 */
DisplayWindow* LEDDisplay_GetWindow(void);

/**
 * @brief Scroll window effect for game ending
 *
 * Scrolls blank lines through the window to clear the display
 * smoothly at the end of a song. Call this repeatedly after
 * the sequence is complete.
 *
 * @return true if scroll complete (all rows cleared), false otherwise
 */
bool LEDDisplay_ScrollOutComplete(void);

#endif /* LED_DISPLAY_H */
