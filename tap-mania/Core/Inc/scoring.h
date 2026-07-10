#ifndef INC_SCORING_H_
#define INC_SCORING_H_

#include <stdbool.h>
#include <stdint.h>

// Enum for score ratings
typedef enum {
    SCORE_RATING_MISS,
    SCORE_RATING_POOR,
    SCORE_RATING_OK,
    SCORE_RATING_GOOD,
    SCORE_RATING_PERFECT
} ScoreRating;

/**
 * @brief Struct to hold configurable scoring window radii in milliseconds.
 *
 * This allows the scoring windows to be adjusted at runtime, for example,
 * [cite_start]by a PC application, and stored in non-volatile memory[cite: 62].
 * [cite_start]The values represent the +/- radius around the exact beat time[cite: 64].
 */
typedef struct {
    uint16_t perfect_radius_ms;
    uint16_t good_radius_ms;
    uint16_t ok_radius_ms;
    uint16_t poor_radius_ms;
} ScoringWindows;

// --- Public Function Prototypes ---
void Scoring_Init(void);
void Scoring_Reset(void);
void Scoring_Update(ScoreRating rating);
int32_t Scoring_GetTotal(void);
uint32_t Scoring_GetCombo(void);
uint32_t Scoring_GetMaxCombo(void);
ScoreRating Scoring_CalculateRating(int32_t delta_ms);
int32_t Scoring_GetPoints(ScoreRating rating);
const char* Scoring_GetText(ScoreRating rating);

/**
 * @brief NEW: Functions for managing scoring windows.
 */
void Scoring_SetDefaultWindows(void);
bool Scoring_SetWindows(const ScoringWindows* new_windows);
const ScoringWindows* Scoring_GetWindows(void);
int32_t Scoring_GetOkRadius(void);

#endif /* INC_SCORING_H_ */
