#include "sequence.h"
#include <stdio.h>
#include <string.h>

// External UART handle for debug output
extern UART_HandleTypeDef huart2;

// Private variables
static BeatData sequence_buffer[MAX_SEQUENCE_LENGTH];
static SequenceMetadata sequence_metadata;
static uint32_t current_beat_index = 0;
static uint32_t sequence_length = 0;
static HoldState hold_state;
static bool is_half_beat_sequence = false;
/**
 * @brief Initialize sequence module
 */
void Sequence_Init(void) {
  memset(&sequence_buffer, 0, sizeof(sequence_buffer));
  memset(&sequence_metadata, 0, sizeof(sequence_metadata));
  memset(&hold_state, 0, sizeof(hold_state));
  current_beat_index = 0;
  sequence_length = 0;

  // Default metadata
  strcpy(sequence_metadata.name, "Test Sequence");
  strcpy(sequence_metadata.artist, "Unknown");
  sequence_metadata.bpm = 120;
  sequence_metadata.offset_ms = 0;
  strcpy(sequence_metadata.difficulty, "N/A");
  is_half_beat_sequence = false;
}

/**
 * @brief Reset sequence to beginning
 */
void Sequence_Reset(void) {
  current_beat_index = 0;
  memset(&hold_state, 0, sizeof(hold_state));
  is_half_beat_sequence = false; // <-- ADD THIS
}

/**
 * @brief Set the sequence mode (full-beat or half-beat)
 */
void Sequence_SetHalfBeatMode(bool is_half_beat) {
  is_half_beat_sequence = is_half_beat;
}

/**
 * @brief Check if the current sequence is in half-beat mode
 */
bool Sequence_IsHalfBeat(void) { return is_half_beat_sequence; }

/**
 * @brief Load sequence from a 2D array
 * @param sequence_data Array of beat data [beat][button] where button order is
 * RGBY
 * @param num_beats Number of beats in the sequence
 * @return true if loaded successfully
 */
bool Sequence_LoadFromArray(const uint8_t sequence_data[][BUTTON_COUNT],
                            uint32_t num_beats) {
  if (num_beats > MAX_SEQUENCE_LENGTH) {
    return false;
  }

  sequence_length = num_beats;
  current_beat_index = 0;
  memset(&hold_state, 0, sizeof(hold_state));

  // Copy sequence data
  for (uint32_t beat = 0; beat < num_beats; beat++) {
    sequence_buffer[beat].beat_number = beat + 1; // Beats are 1-indexed

    for (uint32_t button = 0; button < BUTTON_COUNT; button++) {
      sequence_buffer[beat].actions[button] =
          (SequenceAction)sequence_data[beat][button];
    }
  }

  sequence_metadata.total_beats = num_beats;
  return true;
}

/**
 * @brief Set sequence metadata
 */
void Sequence_SetMetadata(const char *name, const char *artist, uint16_t bpm,
                          uint16_t offset_ms, const char *difficulty) {
  if (name)
    strncpy(sequence_metadata.name, name, 64);
  if (artist)
    strncpy(sequence_metadata.artist, artist, 64);
  sequence_metadata.bpm = bpm;
  sequence_metadata.offset_ms = offset_ms;
  if (difficulty)
    strncpy(sequence_metadata.difficulty, difficulty, 9);
}

/**
 * @brief Get beat data for a specific beat number
 * @param beat_number Beat number (1-indexed)
 * @return Pointer to beat data or NULL if out of range
 */
BeatData *Sequence_GetBeatData(uint32_t beat_number) {
  if (beat_number == 0 || beat_number > sequence_length) {
    return NULL;
  }
  return &sequence_buffer[beat_number - 1]; // Convert to 0-indexed
}

/**
 * @brief Get current beat data
 */
BeatData *Sequence_GetCurrentBeat(void) {
  if (current_beat_index >= sequence_length) {
    return NULL;
  }
  return &sequence_buffer[current_beat_index];
}

/**
 * @brief Advance to next beat
 */
void Sequence_AdvanceBeat(void) {
  if (current_beat_index < sequence_length) {
    current_beat_index++;
    Sequence_UpdateHoldState(current_beat_index +
                             1); // Update with 1-indexed beat
  }
}

/**
 * @brief Check if sequence is complete
 */
bool Sequence_IsComplete(void) { return current_beat_index >= sequence_length; }

/**
 * @brief Get sequence metadata
 */
SequenceMetadata *Sequence_GetMetadata(void) { return &sequence_metadata; }

/**
 * @brief Get current beat number (1-indexed)
 */
uint32_t Sequence_GetCurrentBeatNumber(void) { return current_beat_index + 1; }

/**
 * @brief Get hold state structure
 */
HoldState *Sequence_GetHoldState(void) { return &hold_state; }

/**
 * @brief Update hold state based on current beat
 * This function properly tracks hold sequences across multiple beats
 */
void Sequence_UpdateHoldState(uint32_t current_beat) {
  if (current_beat == 0 || current_beat > sequence_length) {
    return;
  }

  uint32_t beat_idx = current_beat - 1; // Convert to 0-indexed

  // Debug output
  char uart_buf[128];
  int uart_buf_len;

  for (uint32_t button = 0; button < BUTTON_COUNT; button++) {
    SequenceAction current_action = sequence_buffer[beat_idx].actions[button];
    SequenceAction prev_action = SEQ_ACTION_NONE;

    // Get previous beat action if not the first beat
    if (beat_idx > 0) {
      prev_action = sequence_buffer[beat_idx - 1].actions[button];
    }

    // Determine hold state transitions
    if (current_action == SEQ_ACTION_HOLD) {
      if (prev_action != SEQ_ACTION_HOLD) {
        // This is the START of a new hold sequence
        if (hold_state.is_holding[button]) {
          // Error: already holding (shouldn't happen)
          uart_buf_len = sprintf(uart_buf,
                                 "[DEBUG] Button %s: Hold start error at beat "
                                 "%lu (already holding)\r\n",
                                 GetButtonName(button), current_beat);
          HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

          // Reset the hold state
          hold_state.is_holding[button] = false;
          hold_state.hold_scored_press[button] = false;
          hold_state.hold_scored_release[button] = false;
        }

        // Start new hold
        hold_state.is_holding[button] = true;
        hold_state.hold_start_beat[button] = current_beat;
        hold_state.hold_scored_press[button] = false;
        hold_state.hold_scored_release[button] = false;

        // Find the end of this hold sequence
        uint32_t end_beat = current_beat;
        for (uint32_t future = beat_idx + 1; future < sequence_length;
             future++) {
          if (sequence_buffer[future].actions[button] == SEQ_ACTION_HOLD) {
            end_beat = future + 1; // Convert to 1-indexed
          } else {
            break;
          }
        }
        hold_state.hold_end_beat[button] = end_beat;

        // Debug: log hold start
        uart_buf_len = sprintf(
            uart_buf,
            "[DEBUG] Button %s: Hold starts at beat %lu, ends at beat %lu\r\n",
            GetButtonName(button), current_beat, end_beat);
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
      }
      // else: Hold is continuing, no state change needed

    } else {
      // Current action is not HOLD
      if (prev_action == SEQ_ACTION_HOLD && hold_state.is_holding[button]) {
        // This is the END of a hold sequence (hold just ended)
        // Note: The actual release scoring happens when the button is
        // physically released We just track that the hold sequence has ended

        uart_buf_len =
            sprintf(uart_buf,
                    "[DEBUG] Button %s: Hold sequence ended after beat %lu\r\n",
                    GetButtonName(button), current_beat - 1);
        HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

        // Clear hold state if the button was already released
        if (hold_state.hold_scored_release[button]) {
          hold_state.is_holding[button] = false;
          hold_state.hold_start_beat[button] = 0;
          hold_state.hold_end_beat[button] = 0;
          hold_state.hold_scored_press[button] = false;
          hold_state.hold_scored_release[button] = false;
        }
      } else if (hold_state.is_holding[button]) {
        // Clear any stale hold state
        hold_state.is_holding[button] = false;
        hold_state.hold_start_beat[button] = 0;
        hold_state.hold_end_beat[button] = 0;
        hold_state.hold_scored_press[button] = false;
        hold_state.hold_scored_release[button] = false;
      }
    }
  }
}

/**
 * @brief Check if button is currently in a hold
 */
bool Sequence_IsButtonInHold(ArcadeButtonID button_id) {
  if (button_id >= BUTTON_COUNT)
    return false;
  return hold_state.is_holding[button_id];
}

/**
 * @brief Check if button should be released on this beat
 */
bool Sequence_ShouldReleaseHold(ArcadeButtonID button_id,
                                uint32_t beat_number) {
  if (button_id >= BUTTON_COUNT)
    return false;
  return hold_state.is_holding[button_id] &&
         hold_state.hold_end_beat[button_id] == beat_number;
}

/**
 * @brief Print beat information for debugging
 */
void Sequence_PrintBeat(uint32_t beat_number) {
  char uart_buf[128];
  int uart_buf_len;

  BeatData *beat = Sequence_GetBeatData(beat_number);
  if (!beat)
    return;

  uart_buf_len =
      sprintf(uart_buf, "Beat %lu: R=%d G=%d B=%d Y=%d\r\n", beat_number,
              beat->actions[BUTTON_ID_RED], beat->actions[BUTTON_ID_GREEN],
              beat->actions[BUTTON_ID_BLUE], beat->actions[BUTTON_ID_YELLOW]);
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
}

/**
 * @brief Print current sequence state
 */
void Sequence_PrintCurrentState(void) {
  char uart_buf[256];
  int uart_buf_len;

  uart_buf_len = sprintf(uart_buf,
                         "\r\n=== SEQUENCE STATE ===\r\n"
                         "Name: %s\r\n"
                         "Artist: %s\r\n"
                         "BPM: %u\r\n"
                         "Current Beat: %lu/%lu\r\n"
                         "Holds: R=%d G=%d B=%d Y=%d\r\n"
                         "=====================\r\n",
                         sequence_metadata.name, sequence_metadata.artist,
                         sequence_metadata.bpm, current_beat_index + 1,
                         sequence_length, hold_state.is_holding[BUTTON_ID_RED],
                         hold_state.is_holding[BUTTON_ID_GREEN],
                         hold_state.is_holding[BUTTON_ID_BLUE],
                         hold_state.is_holding[BUTTON_ID_YELLOW]);
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
}

// ========== ADD TO sequence.c ==========

/**
 * @brief Prepend blank beats to a loaded sequence
 */
bool Sequence_PrependLeadIn(void) {
  // Check if sequence is loaded
  if (sequence_length == 0) {
    return false; // No sequence loaded
  }

  // Check if we have space for the prepended beats
  if (sequence_length + LEADIN_BEATS > MAX_SEQUENCE_LENGTH) {
    return false; // Not enough space
  }

  // Shift all existing beats forward by LEADIN_BEATS positions
  // Work backwards to avoid overwriting data
  for (int32_t i = (int32_t)sequence_length - 1; i >= 0; i--) {
    uint32_t new_index = i + LEADIN_BEATS;

    // Copy the beat data
    sequence_buffer[new_index] = sequence_buffer[i];

    // Update beat number (beat numbers are 1-indexed)
    sequence_buffer[new_index].beat_number = new_index + 1;
  }

  // Fill the first LEADIN_BEATS with blank beats
  for (uint32_t i = 0; i < LEADIN_BEATS; i++) {
    sequence_buffer[i].beat_number = i + 1;

    // Set all actions to NONE
    for (uint32_t button = 0; button < BUTTON_COUNT; button++) {
      sequence_buffer[i].actions[button] = SEQ_ACTION_NONE;
    }
  }

  // Update sequence length
  sequence_length += LEADIN_BEATS;

  // Update metadata
  sequence_metadata.total_beats = sequence_length;

  return true;
}

/**
 * @brief Get real beat number from game beat number
 */
uint32_t Sequence_GetRealBeatNumber(uint32_t game_beat) {
  if (game_beat <= LEADIN_BEATS) {
    return 0; // In lead-in period, no real sequence beat
  }
  return game_beat - LEADIN_BEATS;
}

/**
 * @brief Check if in lead-in period
 */
bool Sequence_IsInLeadIn(uint32_t game_beat) {
  return game_beat <= LEADIN_BEATS;
}

// GetButtonName function removed - now using the one from button_handler.c
// Declaration is in button_handler.h which is included via sequence.h
