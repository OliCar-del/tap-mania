# tap-mania

## Overview

**tap-mania** is a miniaturized arcade rhythm and dance game designed to be played with your fingers. Inspired by classic rhythm games like *Dance Dance Revolution*, *StepMania*, and *Guitar Hero*, tpmania brings the arcade experience to a desktop form factor.

Players follow rhythm cues displayed on a 16x4 LED matrix and must press the corresponding colored arcade buttons in time with the music. The game tracks your accuracy, calculates your score, and builds combo multipliers based on your timing.

## Features

* **Miniature Arcade Interface:** Four distinct arcade-style buttons (Red, Green, Blue, Yellow) with internal LED illumination, paired with a 16x4 RGB LED matrix to display incoming beats.
* **Standalone Gameplay:** Fully functional without a PC. Load your favorite `.wav` tracks and `.tsq` sequence files onto a microSD card, plug it in, and select your song using the built-in OLED display.
* **Audio Output:** Integrated 3.5mm audio jack with volume control, capable of driving standard 32-ohm headphones or speakers.
* **Dynamic Scoring & Combos:** Earn points based on timing accuracy (Perfect!, Good!, OK, Poor, Miss) and build up a combo counter for successive accurate hits.
* **Customizable Difficulty:** Connect the device to the tpmania PC GUI to customize your timing windows (stored directly on the device's non-volatile memory).
* **Portable:** Powered by a rechargeable 3.7V LiPo battery with USB charging capabilities.

## Hardware Specifications

To build or understand the hardware architecture of tpmania, the following core components are utilized:

* **Microcontroller / USB Interface:** Uses a primary MCU connected via a custom USB bridge (utilizing the `polyglot-turtle-xiao` firmware).
* **LED Matrix:** 16x4 WS2812B RGB LED matrix.
* **Buttons:** 4x illuminated arcade pushbuttons + dedicated control/navigation buttons (or optional rotary encoder/joystick).
* **Display:** OLED module for track selection and game statistics.
* **Storage:** MicroSD card module for reading audio (`.wav`) and beatmaps (`.tsq`).
* **Audio:** 16-bit / 44.1kHz (minimum) DAC for audio generation.
* **Power:** 3.7V LiPo battery with an integrated USB charging circuit.

## PC Companion Application

The tpmania ecosystem includes a PC-based graphical user interface (GUI) that communicates with the device over USB.

**Using the GUI, you can:**

* **Manage Tracks:** Browse, add, and remove `.tsq` sequences and `.wav` audio files directly to/from the microSD card.
* **Tweak Game Settings:** Adjust the exact millisecond timing windows for scoring (e.g., changing a "Perfect!" window from the default 200ms). These settings are saved to the device's internal memory.
* **View Track Metadata:** Read song names, artists, difficulty levels, BPM, and track length.

## How to Play

1. **Select a Track:** Use the onboard navigation buttons and OLED screen to pick a song from the SD card.
2. **Watch the Matrix:** The 16x4 LED matrix will show colored indicators falling towards the bottom of the screen.
3. **Tap to the Beat:** When the colored indicators reach the bottom of the matrix, the corresponding arcade buttons will light up. Press the buttons to the beat!
4. **Hold Notes:** If the LEDs show white indicators, hold the corresponding button down for the duration of the note.

**Scoring System:**

* **Perfect!**: +50 points (Default: ±100ms)
* **Good!**: +20 points (Default: ±150ms)
* **OK**: +10 points (Default: ±200ms)
* **Poor**: 0 points (Default: ±250ms)
* **Miss**: -10 points (Missed entirely, or pressed outside windows)

*Note: Your score cannot drop below zero. Missing notes will reset your active combo counter.*

## Custom Sequences (.tsq Format)

tpmania uses a custom, lightweight text format for its beatmaps (`.tsq`).

### File Structure

A `.tsq` file consists of a header and a sequence body.

```text
Name: Song Title
Artist: Artist Name
BPM: 120
Difficulty: HARD
Offset: 500
---

```

* **BPM** must be between 90 and 220.
* **Difficulty** and **Offset** (delay in ms before the first beat) are optional.

### Sequence Data

Following the `---` divider, the beatmap uses a simple grid layout representing the 4 buttons (Red, Green, Blue, Yellow).

* `0` = No press
* `1` = Tap
* `2` = Hold (Hold starts on the first `2` and releases on the final `2` in a column)

Sequences are grouped into "bars" of 4 rows (or 8 rows for half-beats), separated by commas.

```text
1000
0100
0010
0001
,
2000
2000
0000
0000
;

```

*The file must end with a semicolon (`;`). Any audio file associated with the beatmap must share the exact same filename (e.g., `song.tsq` and `song.wav`) and be placed on the SD card.*

> ## Deep Dive Documentation
If you are looking to understand the internal workings of tpmania, modify the codebase, or review the hardware design, please refer to our detailed documentation:

* 🧠 [Software Architecture & Execution Flow](ARCHITECTURE.md) - Details on the internal state machine, FatFs implementation, scoring logic, and hardware abstraction.
* ⚡ [Hardware & PCB Design](HARDWARE.md) - Details on the Altium schematics, power delivery (LiPo/USB), and signal routing.
