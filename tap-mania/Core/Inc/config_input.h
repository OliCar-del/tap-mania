/**
 ******************************************************************************
 * @file           : config_input.h
 * @brief          : Simple configuration input header
 ******************************************************************************
 */

#ifndef CONFIG_INPUT_H
#define CONFIG_INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "scoring.h"

/* Configuration input buffer size */
#define CONFIG_INPUT_MAX_LEN 128

/**
 * @brief Parse SETWIN command from input string
 * @param input Input string (e.g., "SETWIN:100,150,200,250")
 * @param windows Output structure to populate
 * @return true if valid and parsed successfully, false otherwise
 */
bool Config_ParseInput(const char* input, ScoringWindows* windows);

/**
 * @brief Wait for configuration input via UART
 * @return true if configuration was changed, false otherwise
 */
bool Config_WaitForInput(void);

/**
 * @brief Read one line from UART (blocking with character echo)
 * @param buffer Output buffer for the line
 * @param max_len Maximum buffer length
 * @return true if line was read successfully
 */
bool ReadLine(char* buffer, uint32_t max_len);

/**
 * @brief Send current window configuration via UART
 *
 * Sends current scoring windows in format: WINDOWS:P,G,O,Po\r\n
 * This is used by the GUI to read the device's current configuration.
 */
void Config_SendCurrentWindows(void);

#endif /* CONFIG_INPUT_H */
