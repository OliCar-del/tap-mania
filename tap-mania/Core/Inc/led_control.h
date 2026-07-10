#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "button_handler.h"
#include "main.h"
#include "sequence.h"
#include <stdbool.h>

typedef struct {
    uint32_t led_on_tick[BUTTON_COUNT];
    uint32_t led_off_tick[BUTTON_COUNT];
    bool led_scheduled[BUTTON_COUNT];
    bool led_is_on[BUTTON_COUNT];
} LEDControl;

extern LEDControl led_control;
extern uint32_t beat_led_on_tick;
extern uint32_t beat_led_off_tick;

// Function declarations
void SetButtonLED(ArcadeButtonID button_id, bool state);
void InitLEDControl(void);
void ProcessLEDSchedule(void);
uint32_t CalculateBeatTimestamp(uint32_t beat_number, uint32_t current_beat_num, uint32_t current_beat_timestamp_ms);
void ProcessBeatLED(void);
void ScheduleNextBeatLEDs(uint32_t next_beat_num, uint32_t next_beat_judgment_time, BeatData* next_beat_data, BeatData* current_beat_data);
void ScheduleCurrentBeatLEDsOff(uint32_t current_beat_num, uint32_t current_timestamp_ms, BeatData* current_beat_data);

#endif /* LED_CONTROL_H */
