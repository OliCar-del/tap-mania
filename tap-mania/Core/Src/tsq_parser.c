/**
 ******************************************************************************
 * @file           : tsq_parser.c
 * @brief          : TSQ file format parser implementation
 ******************************************************************************
 */

#include "tsq_parser.h"
#include "SH1106.h"
#include "button_handler.h"
#include "led_control.h"
#include "main.h"
#include "scoring.h"
#include "uart_debug.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* External variables from main.c */
extern UART_HandleTypeDef huart2;
extern volatile uint16_t current_bpm;
extern uint32_t sequence_length;
extern HAL_StatusTypeDef UART_Transmit(uint8_t *data, uint16_t len);
bool hardcode_bpm = false;

/* External timing constants from main.h */
#ifndef OK_WINDOW_BEFORE_MS
#define OK_WINDOW_BEFORE_MS 200
#endif

#ifndef BEAT_LED_BEFORE_MS
#define BEAT_LED_BEFORE_MS 50
#endif

/* External function prototypes */
extern void Update_BPM_Timer(uint16_t bpm);

/* Private helper functions */

/**
 * @brief Trim whitespace from end of string
 */
static void trim_trailing_whitespace(char *str) {
  int len = strlen(str);
  while (len > 0 && (str[len - 1] == '\r' || str[len - 1] == '\n' ||
                     str[len - 1] == ' ' || str[len - 1] == '\t')) {
    str[--len] = '\0';
  }
}

/**
 * @brief Check if line is empty or whitespace only
 */
static bool is_empty_line(const char *line) {
  if (!line || line[0] == '\0')
    return true;

  for (int i = 0; line[i] != '\0'; i++) {
    if (!isspace((unsigned char)line[i])) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Log parser message to UART
 */
static void tsq_log(const char *format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (len > 0 && len < (int)sizeof(buffer)) {
    UART_Transmit((uint8_t *)buffer, (uint16_t)len);
  }
}

/* Public function implementations */

/**
 * @brief Parse TSQ file header
 */
TSQParseResult TSQ_ParseHeader(FIL *fil, TSQMetadata *metadata) {
  char line_buf[TSQ_MAX_LINE_LENGTH];
  bool found_separator = false;
  int lines_read = 0;
  bool found_name = false;
  bool found_artist = false;
  bool found_bpm = false;

  if (!fil || !metadata) {
    return TSQ_PARSE_ERROR_INVALID_HEADER;
  }

  // Initialize metadata with defaults
  memset(metadata, 0, sizeof(TSQMetadata));
  strcpy(metadata->difficulty, "N/A");
  metadata->offset_ms = 0;

  tsq_log("TSQ: Parsing header...\r\n");

  while (lines_read < TSQ_MAX_HEADER_LINES && !found_separator) {
    // Read line from file
    if (f_gets((TCHAR *)line_buf, TSQ_MAX_LINE_LENGTH, fil) == NULL) {
      tsq_log("TSQ: ERROR - Unexpected EOF in header\r\n");
      return TSQ_PARSE_ERROR_FILE_READ;
    }

    lines_read++;
    trim_trailing_whitespace(line_buf);

    // Skip empty lines
    if (is_empty_line(line_buf)) {
      continue;
    }

    // Check for header separator
    if (strncmp(line_buf, TSQ_HEADER_SEPARATOR, 3) == 0) {
      found_separator = true;
      tsq_log("TSQ: Header separator found at line %d\r\n", lines_read);
      break;
    }

    // Parse header fields
    if (sscanf(line_buf, "Name: %64[^\r\n]", metadata->name) == 1) {
      found_name = true;
      tsq_log("TSQ: Name = '%s'\r\n", metadata->name);
      continue;
    }

    if (sscanf(line_buf, "Artist: %64[^\r\n]", metadata->artist) == 1) {
      found_artist = true;
      tsq_log("TSQ: Artist = '%s'\r\n", metadata->artist);
      continue;
    }

    if (sscanf(line_buf, "BPM: %hu", &metadata->file_bpm) == 1) {
      found_bpm = true;
      tsq_log("TSQ: BPM = %u\r\n", metadata->file_bpm);
      continue;
    }

    if (sscanf(line_buf, "Offset: %hu", &metadata->offset_ms) == 1) {
      tsq_log("TSQ: Offset = %u ms\r\n", metadata->offset_ms);
      continue;
    }

    if (sscanf(line_buf, "Difficulty: %9[^\r\n]", metadata->difficulty) == 1) {
      tsq_log("TSQ: Difficulty = '%s'\r\n", metadata->difficulty);
      continue;
    }
  }

  // Validate that we found the header separator
  if (!found_separator) {
    tsq_log("TSQ: ERROR - Header separator '%s' not found\r\n",
            TSQ_HEADER_SEPARATOR);
    return TSQ_PARSE_ERROR_INVALID_HEADER;
  }

  // Validate required fields
  if (!found_name || !found_artist || !found_bpm) {
    tsq_log(
        "TSQ: ERROR - Missing required fields (Name:%d Artist:%d BPM:%d)\r\n",
        found_name, found_artist, found_bpm);
    return TSQ_PARSE_ERROR_MISSING_FIELD;
  }

  // Validate BPM range
  if (metadata->file_bpm < 90 || metadata->file_bpm > 220) {
    tsq_log("TSQ: ERROR - BPM %u out of range (90-220)\r\n",
            metadata->file_bpm);
    return TSQ_PARSE_ERROR_INVALID_BPM;
  }

  tsq_log("TSQ: Header parsed successfully\r\n");
  return TSQ_PARSE_OK;
}

/**
 * @brief Detect beat type (full-beat vs half-beat)
 */
TSQParseResult TSQ_DetectBeatType(FIL *fil, bool *is_half_beat) {
  char line_buf[TSQ_MAX_LINE_LENGTH];
  int data_line_count = 0;
  FSIZE_t start_position;

  if (!fil || !is_half_beat) {
    return TSQ_PARSE_ERROR_BEAT_DETECTION_FAILED;
  }

  *is_half_beat = false;

  // Save current file position
  start_position = f_tell(fil);
  tsq_log("TSQ: Detecting beat type (position: %lu)...\r\n",
          (unsigned long)start_position);

  // Read lines until we find a bar marker
  for (int line_num = 0; line_num < 12; line_num++) {
    if (f_gets((TCHAR *)line_buf, TSQ_MAX_LINE_LENGTH, fil) == NULL) {
      tsq_log("TSQ: ERROR - EOF during beat detection\r\n");
      f_lseek(fil, start_position);
      return TSQ_PARSE_ERROR_BEAT_DETECTION_FAILED;
    }

    trim_trailing_whitespace(line_buf);

    // Check for bar marker
    if (line_buf[0] == TSQ_BAR_MARKER) {
      if (data_line_count == TSQ_FULL_BEAT_BAR_SIZE) {
        *is_half_beat = false;
        tsq_log("TSQ: Full-beat sequence detected (%d lines per bar)\r\n",
                TSQ_FULL_BEAT_BAR_SIZE);
        f_lseek(fil, start_position);
        return TSQ_PARSE_OK;
      } else if (data_line_count == TSQ_HALF_BEAT_BAR_SIZE) {
        *is_half_beat = true;
        tsq_log("TSQ: Half-beat sequence detected (%d lines per bar)\r\n",
                TSQ_HALF_BEAT_BAR_SIZE);
        f_lseek(fil, start_position);
        return TSQ_PARSE_OK;
      } else {
        tsq_log("TSQ: ERROR - Invalid bar size: %d lines\r\n", data_line_count);
        f_lseek(fil, start_position);
        return TSQ_PARSE_ERROR_BEAT_DETECTION_FAILED;
      }
    }

    // Skip empty lines and end markers
    if (is_empty_line(line_buf) || line_buf[0] == TSQ_END_MARKER) {
      continue;
    }

    // Count data lines (should be 4 characters: 0, 1, or 2)
    if (strlen(line_buf) >= 4) {
      bool valid_line = true;
      for (int i = 0; i < 4; i++) {
        if (line_buf[i] != '0' && line_buf[i] != '1' && line_buf[i] != '2') {
          valid_line = false;
          break;
        }
      }
      if (valid_line) {
        data_line_count++;
      }
    }
  }

  // Didn't find bar marker in expected range
  tsq_log("TSQ: ERROR - No valid bar marker found\r\n");
  f_lseek(fil, start_position);
  return TSQ_PARSE_ERROR_BEAT_DETECTION_FAILED;
}

/**
 * @brief Parse single sequence line
 */
TSQParseResult TSQ_ParseSequenceLine(const char *line, uint8_t *actions) {
  if (!line || !actions) {
    return TSQ_PARSE_ERROR_INVALID_SEQUENCE_DATA;
  }

  // Verify line has at least 4 characters
  if (strlen(line) < 4) {
    return TSQ_PARSE_ERROR_INVALID_SEQUENCE_DATA;
  }

  // Parse RGBY order (matches specification and button enum order)
  for (int i = 0; i < BUTTON_COUNT; i++) {
    char c = line[i];
    switch (c) {
    case '0':
      actions[i] = SEQ_ACTION_NONE;
      break;
    case '1':
      actions[i] = SEQ_ACTION_PRESS;
      break;
    case '2':
      actions[i] = SEQ_ACTION_HOLD;
      break;
    default:
      tsq_log("TSQ: ERROR - Invalid character '%c' at position %d\r\n", c, i);
      return TSQ_PARSE_ERROR_INVALID_SEQUENCE_DATA;
    }
  }

  return TSQ_PARSE_OK;
}

/**
 * @brief Load sequence data from file
 */
TSQParseResult TSQ_LoadSequenceData(FIL *fil,
                                    uint8_t sequence_buffer[][BUTTON_COUNT],
                                    uint32_t max_beats, uint32_t *beats_read) {
  char line_buf[TSQ_MAX_LINE_LENGTH];
  uint32_t beat_index = 0;
  bool end_found = false;
  TSQParseResult result;

  if (!fil || !sequence_buffer || !beats_read) {
    return TSQ_PARSE_ERROR_INVALID_SEQUENCE_DATA;
  }

  *beats_read = 0;

  tsq_log("TSQ: Loading sequence data (max %lu beats)...\r\n", max_beats);

  while (beat_index < max_beats && !end_found) {
    // Read line from file
    if (f_gets((TCHAR *)line_buf, TSQ_MAX_LINE_LENGTH, fil) == NULL) {
      // EOF reached
      tsq_log("TSQ: EOF reached at beat %lu\r\n", beat_index);
      break;
    }

    trim_trailing_whitespace(line_buf);

    // Check for end marker
    if (line_buf[0] == TSQ_END_MARKER) {
      tsq_log("TSQ: End marker found at beat %lu\r\n", beat_index);
      end_found = true;
      break;
    }

    // Skip bar markers
    if (line_buf[0] == TSQ_BAR_MARKER) {
      continue;
    }

    // Skip empty lines
    if (is_empty_line(line_buf)) {
      continue;
    }

    // Parse sequence line
    result = TSQ_ParseSequenceLine(line_buf, sequence_buffer[beat_index]);
    if (result != TSQ_PARSE_OK) {
      tsq_log("TSQ: ERROR - Failed to parse beat %lu\r\n", beat_index);
      return result;
    }

    beat_index++;

    // Progress indicator every 50 beats
    if (beat_index % 50 == 0) {
      tsq_log("TSQ: Progress: %lu beats loaded...\r\n", beat_index);
    }
  }

  *beats_read = beat_index;

  if (beat_index == 0) {
    tsq_log("TSQ: ERROR - No sequence data found\r\n");
    return TSQ_PARSE_ERROR_NO_DATA;
  }

  tsq_log("TSQ: Successfully loaded %lu beats\r\n", beat_index);
  return TSQ_PARSE_OK;
}

/**
 * @brief Complete TSQ loading and configuration
 */
TSQParseResult TSQ_LoadAndConfigure(FIL *fil, TSQMetadata *metadata) {
  TSQParseResult result;
  static uint8_t loaded_sequence[MAX_SEQUENCE_LENGTH][BUTTON_COUNT];
  uint32_t max_beats_to_read;
  uint16_t spacing = 2;
  uint16_t h = Font_7x10.FontHeight;

  if (!fil || !metadata) {
    return TSQ_PARSE_ERROR_INVALID_HEADER;
  }

  tsq_log("\r\n=== TSQ File Loading Started ===\r\n");

  // Step 1: Parse header
  result = TSQ_ParseHeader(fil, metadata);
  if (result != TSQ_PARSE_OK) {
    tsq_log("TSQ: FAILED at header parsing (error %d)\r\n", result);
    return result;
  }

  // Step 2: Detect beat type
  result = TSQ_DetectBeatType(fil, &metadata->is_half_beat);
  if (result != TSQ_PARSE_OK) {
    tsq_log("TSQ: FAILED at beat type detection (error %d)\r\n", result);
    return result;
  }

  // Step 3: Calculate playback parameters
  if (metadata->is_half_beat) {
    max_beats_to_read = TSQ_MAX_HALF_BEATS;
    metadata->playback_bpm = metadata->file_bpm * 2;
    tsq_log("TSQ: Half-beat mode - File BPM: %u, Playback BPM: %u\r\n",
            metadata->file_bpm, metadata->playback_bpm);
  } else {
    max_beats_to_read = TSQ_MAX_FULL_BEATS;
    metadata->playback_bpm = metadata->file_bpm;
    tsq_log("TSQ: Full-beat mode - BPM: %u\r\n", metadata->playback_bpm);
  }

  // Step 4: Load sequence data
  result = TSQ_LoadSequenceData(fil, loaded_sequence, max_beats_to_read,
                                &metadata->beats_loaded);
  if (result != TSQ_PARSE_OK) {
    tsq_log("TSQ: FAILED at sequence data loading (error %d)\r\n", result);
    return result;
  }

  if (metadata->beats_loaded == 0) {
    tsq_log("TSQ: FAILED - No beats loaded\r\n");
    return TSQ_PARSE_ERROR_NO_DATA;
  }

  // Step 5: Initialize game subsystems
  tsq_log("TSQ: Initializing game subsystems...\r\n");
  Sequence_Init();
  Scoring_Reset();
  ButtonHandler_Init();
  //    Sequence_SetHalfBeatMode(metadata->is_half_beat); //ruins it by
  //    resetting.

  // Step 6: Load sequence into game
  if (!Sequence_LoadFromArray(loaded_sequence, metadata->beats_loaded)) {
    tsq_log("TSQ: FAILED to load sequence into game engine\r\n");
    return TSQ_PARSE_ERROR_INVALID_SEQUENCE_DATA;
  }

  // Step 7: Set sequence metadata
  Sequence_SetMetadata(metadata->name, metadata->artist, metadata->playback_bpm,
                       metadata->offset_ms, metadata->difficulty);

  // Step 8: Update global variables
  if (!hardcode_bpm) {
    current_bpm = metadata->playback_bpm;
  }
  sequence_length = metadata->beats_loaded;

  // Step 9: Configure BPM timer
  Update_BPM_Timer(current_bpm);

  // Step 10: Schedule initial LED events
  if (sequence_length > 0) {
    uint32_t beat_period_ms = (60 * 1000) / current_bpm;
    uint32_t first_beat_timestamp = HAL_GetTick() + beat_period_ms;

    const ScoringWindows *windows_struct = Scoring_GetWindows();
    uint32_t ok_radius_ms = windows_struct->ok_radius_ms;

    BeatData *first_beat_data = Sequence_GetBeatData(1);
    if (first_beat_data) {
      for (int i = 0; i < BUTTON_COUNT; i++) {
        bool is_press = (first_beat_data->actions[i] == SEQ_ACTION_PRESS);
        bool is_hold_start = (first_beat_data->actions[i] == SEQ_ACTION_HOLD);

        if (is_press || is_hold_start) {
          led_control.led_on_tick[i] = first_beat_timestamp - ok_radius_ms;
          led_control.led_scheduled[i] = true;
        }
      }
    }

    // Schedule first beat LED
    beat_led_on_tick = first_beat_timestamp - BEAT_LED_BEFORE_MS;
  }

  // Display comprehensive summary
  tsq_log("\r\n=== TSQ File Loaded Successfully ===\r\n");
  tsq_log("Song:           %s\r\n", metadata->name);
  tsq_log("Artist:         %s\r\n", metadata->artist);
  tsq_log("Difficulty:     %s\r\n", metadata->difficulty);
  tsq_log("File BPM:       %u\r\n", metadata->file_bpm);
  tsq_log("Playback BPM:   %u\r\n", metadata->playback_bpm);
  tsq_log("Beats Loaded:   %lu\r\n", metadata->beats_loaded);
  tsq_log("Offset:         %u ms\r\n", metadata->offset_ms);
  tsq_log("Beat Type:      %s\r\n",
          metadata->is_half_beat ? "Half-beat" : "Full-beat");
  // Calculate and display total duration in MM:SS format
  // Total duration = offset + (beat_period * num_beats)
  // beat_period_ms = (60 * 1000) / playback_bpm
  uint32_t total_duration_ms =
      metadata->offset_ms + (uint32_t)((uint64_t)metadata->beats_loaded *
                                       60000 / metadata->playback_bpm);
  uint32_t total_seconds = total_duration_ms / 1000;
  uint32_t minutes = total_seconds / 60;
  uint32_t seconds = total_seconds % 60;

  tsq_log("Duration:       %02lu:%02lu\r\n", minutes, seconds);
  tsq_log("====================================\r\n\r\n");

  char songNameBuf[18];
  char artistNameBuf[18];
  char diffBuf[18];
  char bpmBuf[18];
  char durationBuf[18];

  snprintf(songNameBuf, sizeof songNameBuf, "Song: %s", metadata->name);
  snprintf(artistNameBuf, sizeof songNameBuf, "Artist: %s", metadata->artist);
  snprintf(diffBuf, sizeof songNameBuf, "Diff: %s", metadata->difficulty);
  snprintf(bpmBuf, sizeof songNameBuf, "BPM: %u", metadata->file_bpm);
  snprintf(durationBuf, sizeof songNameBuf, "Duration: %02lu:%02lu", minutes,
           seconds);
  SH1106_Fill(SH1106_COLOR_BLACK);
  // SH1106_UpdateScreen();

  SH1106_GotoXY(1, 0);
  SH1106_Puts(songNameBuf, &Font_7x10, 1);

  SH1106_GotoXY(1, h + spacing);
  SH1106_Puts(artistNameBuf, &Font_7x10, 1);

  SH1106_GotoXY(1, 2 * (h + spacing));
  SH1106_Puts(diffBuf, &Font_7x10, 1);

  SH1106_GotoXY(1, 3 * (h + spacing));
  SH1106_Puts(bpmBuf, &Font_7x10, 1);

  SH1106_GotoXY(1, 4 * (h + spacing));
  SH1106_Puts(durationBuf, &Font_7x10, 1);
  SH1106_UpdateScreen();
  //  myprintf("song name length = %d\r\n", strlen(metadata->name));
  //  myprintf("artist length = %d\r\n", strlen(metadata->artist));
  //  myprintf("difficulty length = %d\r\n", strlen(metadata->difficulty));
  //  myprintf("BPM length = %d\r\n", sizeof(metadata->file_bpm));
  //  myprintf("song length length = %d\r\n", sizeof((metadata->beats_loaded *
  //  60) / metadata->playback_bpm));

  return TSQ_PARSE_OK;
}

/**
 * @brief Get error message string
 */
const char *TSQ_GetErrorMessage(TSQParseResult result) {
  switch (result) {
  case TSQ_PARSE_OK:
    return "Success";
  case TSQ_PARSE_ERROR_FILE_READ:
    return "File read error";
  case TSQ_PARSE_ERROR_INVALID_HEADER:
    return "Invalid or missing header";
  case TSQ_PARSE_ERROR_MISSING_FIELD:
    return "Missing required header field";
  case TSQ_PARSE_ERROR_INVALID_BPM:
    return "BPM out of range (90-220)";
  case TSQ_PARSE_ERROR_BEAT_DETECTION_FAILED:
    return "Could not detect beat type";
  case TSQ_PARSE_ERROR_INVALID_SEQUENCE_DATA:
    return "Invalid sequence data format";
  case TSQ_PARSE_ERROR_NO_DATA:
    return "No sequence data found";
  case TSQ_PARSE_ERROR_BUFFER_OVERFLOW:
    return "Buffer overflow during parsing";
  default:
    return "Unknown error";
  }
}
