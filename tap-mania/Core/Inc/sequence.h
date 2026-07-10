#ifndef INC_SEQUENCE_H_
#define INC_SEQUENCE_H_

#include "button_handler.h"
#include <stdbool.h>
#include <stdint.h>

// Sequence action types matching TSQ format
typedef enum {
    SEQ_ACTION_NONE = 0, // No press required
    SEQ_ACTION_PRESS = 1, // Press and release before next beat
    SEQ_ACTION_HOLD = 2 // Hold through consecutive 2s
} SequenceAction;

#define LEADIN_BEATS 16 // Number of blank beats to prepend

// Beat data for all 4 buttons
typedef struct {
    SequenceAction actions[BUTTON_COUNT]; // RGBY order
    uint32_t beat_number;
} BeatData;

// Sequence metadata
typedef struct {
    char name[65];
    char artist[65];
    uint16_t bpm;
    uint16_t offset_ms; // Delay before first beat
    char difficulty[10];
    uint32_t total_beats;
} SequenceMetadata;

// Hold tracking structure
typedef struct {
    bool is_holding[BUTTON_COUNT]; // Currently holding button
    uint32_t hold_start_beat[BUTTON_COUNT]; // When hold started
    uint32_t hold_end_beat[BUTTON_COUNT]; // When hold should end
    bool hold_scored_press[BUTTON_COUNT]; // Was press already scored
    bool hold_scored_release[BUTTON_COUNT]; // Was release already scored
} HoldState;

// Maximum sequence length (3 minutes at 220 BPM = ~660 beats)
#define MAX_SEQUENCE_LENGTH 700

// Public functions
void Sequence_Init(void);
bool Sequence_LoadFromArray(const uint8_t sequence_data[][BUTTON_COUNT], uint32_t num_beats);
void Sequence_SetMetadata(const char* name, const char* artist, uint16_t bpm, uint16_t offset_ms, const char* difficulty);
BeatData* Sequence_GetBeatData(uint32_t beat_number);
BeatData* Sequence_GetCurrentBeat(void);
void Sequence_AdvanceBeat(void);
bool Sequence_IsComplete(void);
void Sequence_Reset(void);
SequenceMetadata* Sequence_GetMetadata(void);
uint32_t Sequence_GetCurrentBeatNumber(void);

// Hold state management
HoldState* Sequence_GetHoldState(void);
void Sequence_UpdateHoldState(uint32_t current_beat);
bool Sequence_IsButtonInHold(ArcadeButtonID button_id);
bool Sequence_ShouldReleaseHold(ArcadeButtonID button_id, uint32_t beat_number);

// Debug functions
void Sequence_PrintBeat(uint32_t beat_number);
void Sequence_PrintCurrentState(void);
// ========== ADD TO sequence.h ==========

/* Lead-in configuration */

/**
 * @brief Prepend blank beats to create visual lead-in
 *
 * Call this AFTER loading a sequence with Sequence_LoadFromArray()
 * but BEFORE starting playback. This shifts the sequence forward by
 * LEADIN_BEATS and fills the first LEADIN_BEATS with SEQ_ACTION_NONE.
 *
 * @return true if successful, false if sequence too long
 */
bool Sequence_PrependLeadIn(void);

/**
 * @brief Get the real sequence beat number from game beat number
 *
 * Game beats 1-16 are lead-in (return 0)
 * Game beat 17+ returns original beat number (17 -> 1, 18 -> 2, etc.)
 *
 * @param game_beat Current beat number during gameplay
 * @return Real sequence beat number, or 0 if in lead-in
 */
uint32_t Sequence_GetRealBeatNumber(uint32_t game_beat);

/**
 * @brief Check if currently in lead-in period
 *
 * @param game_beat Current beat number during gameplay
 * @return true if game_beat <= 16, false otherwise
 */
bool Sequence_IsInLeadIn(uint32_t game_beat);

/**
 * @brief Set the sequence mode (full-beat or half-beat)
 * @param is_half_beat true if half-beat mode, false otherwise
 */
void Sequence_SetHalfBeatMode(bool is_half_beat);

/**
 * @brief Check if the current sequence is in half-beat mode
 * @return true if half-beat mode, false otherwise
 */
bool Sequence_IsHalfBeat(void);

#endif /* INC_SEQUENCE_H_ */
