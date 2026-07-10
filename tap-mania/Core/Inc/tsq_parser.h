/**
 ******************************************************************************
 * @file           : tsq_parser.h
 * @brief          : TSQ (TapMania Sequence) file format parser
 ******************************************************************************
 * @attention
 *
 * This module handles parsing of .tsq sequence files for the tpmania game.
 * It supports both full-beat (4 lines per bar) and half-beat (8 lines per bar)
 * sequence formats as defined in the specification.
 *
 * Key features:
 * - Automatic beat type detection (full-beat vs half-beat)
 * - BPM adjustment for half-beat sequences
 * - Robust error handling and validation
 * - Memory-efficient streaming parser
 *
 ******************************************************************************
 */

#ifndef TSQ_PARSER_H
#define TSQ_PARSER_H

#include "fatfs.h"
#include "main.h"
#include "sequence.h"
#include <stdbool.h>
#include <stdint.h>

/* Parser configuration constants */
#define TSQ_MAX_LINE_LENGTH 128
#define TSQ_MAX_HEADER_LINES 20
#define TSQ_HEADER_SEPARATOR "---"

/* Maximum beats to read based on sequence type */
#define TSQ_MAX_FULL_BEATS 180
#define TSQ_MAX_HALF_BEATS 360

/* TSQ file format markers */
#define TSQ_BAR_MARKER ','
#define TSQ_END_MARKER ';'

/* Beat type detection constants */
#define TSQ_FULL_BEAT_BAR_SIZE 4
#define TSQ_HALF_BEAT_BAR_SIZE 8

/**
 * @brief TSQ parser result codes
 */
typedef enum {
    TSQ_PARSE_OK = 0,
    TSQ_PARSE_ERROR_FILE_READ,
    TSQ_PARSE_ERROR_INVALID_HEADER,
    TSQ_PARSE_ERROR_MISSING_FIELD,
    TSQ_PARSE_ERROR_INVALID_BPM,
    TSQ_PARSE_ERROR_BEAT_DETECTION_FAILED,
    TSQ_PARSE_ERROR_INVALID_SEQUENCE_DATA,
    TSQ_PARSE_ERROR_NO_DATA,
    TSQ_PARSE_ERROR_BUFFER_OVERFLOW
} TSQParseResult;

/**
 * @brief TSQ metadata structure
 * Contains all metadata extracted from TSQ file header
 */
typedef struct {
    char name[65]; // Song name (up to 64 chars + null)
    char artist[65]; // Artist name (up to 64 chars + null)
    char difficulty[10]; // Difficulty string (N/A, EASY, MEDIUM, etc)
    uint16_t file_bpm; // BPM value from file
    uint16_t playback_bpm; // Actual playback BPM (may be doubled for half-beat)
    uint16_t offset_ms; // Offset in milliseconds before first beat
    bool is_half_beat; // true if half-beat sequence, false if full-beat
    uint32_t beats_loaded; // Number of beats successfully loaded
} TSQMetadata;

/**
 * @brief Parse TSQ file header and extract metadata
 *
 * Reads and validates the TSQ file header, extracting all metadata fields.
 * The file pointer will be positioned immediately after the header separator
 * upon successful return.
 *
 * @param fil Pointer to open FIL structure (must be at start of file)
 * @param metadata Output structure to receive parsed metadata
 * @return TSQ_PARSE_OK on success, error code otherwise
 */
TSQParseResult TSQ_ParseHeader(FIL* fil, TSQMetadata* metadata);

/**
 * @brief Detect whether sequence uses full beats or half beats
 *
 * Analyzes the first bar of sequence data to determine if the file uses
 * 4-line bars (full-beat) or 8-line bars (half-beat). This function will
 * restore the file position after detection.
 *
 * @param fil Pointer to open FIL structure (positioned after header)
 * @param is_half_beat Output: true if half-beat sequence detected
 * @return TSQ_PARSE_OK on success, error code otherwise
 */
TSQParseResult TSQ_DetectBeatType(FIL* fil, bool* is_half_beat);

/**
 * @brief Parse a single sequence data line into button actions
 *
 * Converts a 4-character sequence line (RGBY format) into an array of
 * button actions suitable for the sequence module.
 *
 * @param line Input string containing 4 characters (0, 1, or 2)
 * @param actions Output array of 4 button actions
 * @return TSQ_PARSE_OK on success, error code otherwise
 */
TSQParseResult TSQ_ParseSequenceLine(const char* line, uint8_t* actions);

/**
 * @brief Load sequence data from TSQ file into buffer
 *
 * Reads sequence data lines from the TSQ file, parsing each line and
 * storing the button actions in the provided buffer. Automatically handles
 * bar markers and end-of-sequence markers.
 *
 * @param fil Pointer to open FIL structure (positioned at sequence start)
 * @param sequence_buffer Output 2D array for sequence data [beat][button]
 * @param max_beats Maximum number of beats to read
 * @param beats_read Output: actual number of beats successfully loaded
 * @return TSQ_PARSE_OK on success, error code otherwise
 */
TSQParseResult TSQ_LoadSequenceData(FIL* fil, uint8_t sequence_buffer[][BUTTON_COUNT],
    uint32_t max_beats, uint32_t* beats_read);

/**
 * @brief Complete TSQ file loading and game configuration
 *
 * This is the main entry point for loading a TSQ file. It orchestrates
 * the complete loading process including:
 * - Header parsing
 * - Beat type detection
 * - BPM calculation for playback
 * - Sequence data loading
 * - Game system initialization
 * - LED scheduling setup
 *
 * Upon successful return, the game will be fully configured and ready
 * to start playback.
 *
 * @param fil Pointer to open FIL structure (must be at start of file)
 * @param metadata Output: metadata structure to receive file information
 * @return TSQ_PARSE_OK on success, error code otherwise
 */
TSQParseResult TSQ_LoadAndConfigure(FIL* fil, TSQMetadata* metadata);

/**
 * @brief Get human-readable error message for parse result
 *
 * Converts a TSQParseResult code into a descriptive error message
 * suitable for display or logging.
 *
 * @param result Parse result code
 * @return Pointer to constant error message string
 */
const char* TSQ_GetErrorMessage(TSQParseResult result);

#endif /* TSQ_PARSER_H */