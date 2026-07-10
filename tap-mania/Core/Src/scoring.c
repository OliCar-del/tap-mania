#include "scoring.h"

// Score point values as defined in the specification
#define PERFECT_SCORE_POINTS 50
#define GOOD_SCORE_POINTS 20
#define OK_SCORE_POINTS 10
#define POOR_SCORE_POINTS 0
#define MISS_SCORE_POINTS -10

// --- Private Variables ---
static volatile int32_t total_score = 0;
static volatile uint32_t combo_counter = 0;
static volatile uint32_t max_combo = 0;

// NEW: Static instance of the scoring windows struct
static ScoringWindows current_scoring_windows;

/**
 * @brief Initializes scoring variables and loads default window settings.
 */
void Scoring_Init(void) {
  Scoring_Reset();
  // Load default settings on initialization.
  Scoring_SetDefaultWindows();
}

/**
 * @brief Resets the scoring system for a new game session.
 */
void Scoring_Reset(void) {
  total_score = 0;
  combo_counter = 0;
  max_combo = 0;
}

/**
 * @brief Sets the scoring windows to their default values as per the
 * specification.
 */
void Scoring_SetDefaultWindows(void) {
  current_scoring_windows.perfect_radius_ms = 100; // Total window: 200ms
  current_scoring_windows.good_radius_ms = 150;    // Total window: 300ms
  current_scoring_windows.ok_radius_ms = 200;      // Total window: 400ms
  current_scoring_windows.poor_radius_ms = 250;    // Total window: 500ms
}

/**
 * @brief Updates the total score and combo counter based on a press rating.
 * @param rating The ScoreRating of the button press.
 */
void Scoring_Update(ScoreRating rating) {
  int32_t base_score = Scoring_GetPoints(rating);
  int32_t combo_bonus = 0;

  // Update Combo Counter based on rating
  if (rating == SCORE_RATING_PERFECT || rating == SCORE_RATING_GOOD) {
    // Each time the user is able to complete a subsequent "Perfect!" or "Good!"
    // action, the combo counter is increased by 1 and added to the total score.
    combo_bonus = combo_counter;
    combo_counter++;

    if (combo_counter > max_combo) {
      max_combo = combo_counter;
    }
  } else {
    // An "OK", "Poor" or "Miss" action resets the counter to zero.
    combo_counter = 0;
  }

  total_score += base_score + combo_bonus;

  // The score cannot go below zero.
  if (total_score < 0) {
    total_score = 0;
  }
}

/**
 * @brief  Calculates the score rating based on the time difference from the
 * beat.
 * @param  delta_ms: The time difference in milliseconds (can be negative).
 * @retval The corresponding ScoreRating.
 */
ScoreRating Scoring_CalculateRating(int32_t delta_ms) {
  uint32_t abs_delta = (delta_ms < 0) ? -delta_ms : delta_ms;

  // Use values from the struct instead of hardcoded defines
  if (abs_delta <= current_scoring_windows.perfect_radius_ms)
    return SCORE_RATING_PERFECT;
  if (abs_delta <= current_scoring_windows.good_radius_ms)
    return SCORE_RATING_GOOD;
  if (abs_delta <= current_scoring_windows.ok_radius_ms)
    return SCORE_RATING_OK;
  if (abs_delta <= current_scoring_windows.poor_radius_ms)
    return SCORE_RATING_POOR;

  return SCORE_RATING_MISS;
}

/**
 * @brief Sets new scoring window values with validation.
 * @param new_windows Pointer to the new window settings.
 * @retval true if the new windows were valid and applied, false otherwise.
 */
bool Scoring_SetWindows(const ScoringWindows *new_windows) {
  if (!new_windows) {
    return false;
  }

  // Validate that windows maintain the correct order and are within spec
  // limits.
  if (new_windows->perfect_radius_ms > 0 &&
      new_windows->perfect_radius_ms < new_windows->good_radius_ms &&
      new_windows->good_radius_ms < new_windows->ok_radius_ms &&
      new_windows->ok_radius_ms < new_windows->poor_radius_ms &&
      new_windows->poor_radius_ms <= 250) { // Max radius for 500ms total window

    current_scoring_windows = *new_windows;
    // NOTE: In a full implementation, you would save these new settings
    // to non-volatile memory (e.g., Flash) here, as required by the spec.
    return true;
  }

  return false;
}

/**
 * @brief Gets a read-only pointer to the current scoring window settings.
 * @retval A constant pointer to the current ScoringWindows struct.
 */
const ScoringWindows *Scoring_GetWindows(void) {
  return &current_scoring_windows;
}

/*
 * @brief uses GetWindows to determine scoring radius and returns as int
 */
int32_t Scoring_GetOkRadius(void) {
  const ScoringWindows *windows = Scoring_GetWindows();
  uint32_t ok_radius_ms = windows->ok_radius_ms;
  return ok_radius_ms;
}

/**
 * @brief Gets the current total score.
 * @retval The total score.
 */
int32_t Scoring_GetTotal(void) { return total_score; }

/**
 * @brief Gets the current combo count.
 * @retval The combo count.
 */
uint32_t Scoring_GetCombo(void) { return combo_counter; }

/**
 * @brief Gets the maximum combo achieved.
 * @retval The maximum combo count.
 */
uint32_t Scoring_GetMaxCombo(void) { return max_combo; }

/**
 * @brief  Gets the point value for a given score rating.
 * @param  rating: The score rating.
 * @retval The integer score value.
 */
int32_t Scoring_GetPoints(ScoreRating rating) {
  switch (rating) {
  case SCORE_RATING_PERFECT:
    return PERFECT_SCORE_POINTS;
  case SCORE_RATING_GOOD:
    return GOOD_SCORE_POINTS;
  case SCORE_RATING_OK:
    return OK_SCORE_POINTS;
  case SCORE_RATING_POOR:
    return POOR_SCORE_POINTS;
  case SCORE_RATING_MISS:
    return MISS_SCORE_POINTS;
  default:
    return 0;
  }
}

/**
 * @brief  Gets the display text for a given score rating.
 * @param  rating: The score rating.
 * @retval A constant string with the rating's name.
 */
const char *Scoring_GetText(ScoreRating rating) {
  switch (rating) {
  case SCORE_RATING_PERFECT:
    return "Perfect!";
  case SCORE_RATING_GOOD:
    return "Good!";
  case SCORE_RATING_OK:
    return "OK";
  case SCORE_RATING_POOR:
    return "Poor";
  case SCORE_RATING_MISS:
    return "Miss";
  default:
    return "?";
  }
}
