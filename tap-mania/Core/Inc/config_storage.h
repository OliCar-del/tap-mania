/**
 ******************************************************************************
 * @file           : config_storage.h
 * @brief          : Non-volatile configuration storage using Flash memory
 ******************************************************************************
 * @attention
 *
 * This module manages storage of configuration data (scoring windows) in
 * the device's Flash memory for persistence across power cycles.
 *
 * Flash usage: Last page of Flash (calculated dynamically based on device)
 * Typical addresses:
 *   - 256KB device (STM32L432KC): Page 127, 0x0803F800
 *   - 512KB device: Page 255, 0x0807F800
 *
 ******************************************************************************
 */
#ifndef CONFIG_STORAGE_H_
#define CONFIG_STORAGE_H_

#include "main.h"
#include "scoring.h"
#include <stdbool.h>
#include <stdint.h> // Defines uint32_t, int32_t, etc.

/* Configuration structure stored in Flash */
typedef struct {
    uint32_t magic; // Magic number for validation (0x544D4E50 = "TPMN")
    uint32_t version; // Config version (currently 1)
    ScoringWindows windows; // Scoring window configuration
    uint32_t checksum; // CRC32 checksum for validation
} ConfigData;

/* Flash memory configuration - updated for STM32L432KC with 256KB flash */
#define CONFIG_FLASH_PAGE 127 // Last page of 256KB Flash (0-127)
#define CONFIG_FLASH_ADDRESS 0x0803F800 // Start of last page for 256KB device
#define CONFIG_MAGIC 0x544D4E50 // "TPMN" in hex
#define CONFIG_VERSION 1

/* Flash size detection */
// #define FLASHSIZE_BASE          0x1FFF75E0          // Flash size register address for STM32L4
#define FLASH_BASE_ADDRESS 0x08000000 // Start of Flash memory
// #define FLASH_PAGE_SIZE         2048                // 2KB page size for STM32L4

/* Default scoring windows (from specification) */
#define DEFAULT_PERFECT_RADIUS_MS 100
#define DEFAULT_GOOD_RADIUS_MS 150
#define DEFAULT_OK_RADIUS_MS 200
#define DEFAULT_POOR_RADIUS_MS 250

/* Function prototypes */

/**
 * @brief Initialize configuration storage system
 *
 * Performs flash size detection and validates configuration address.
 * Checks if valid configuration exists in Flash.
 * If valid config found, loads it into scoring system.
 * If not, uses default values.
 *
 * @return true if valid config was loaded from Flash, false if using defaults
 */
bool Config_Init(void);

/**
 * @brief Load configuration from Flash into scoring system
 *
 * Reads configuration from Flash, validates it, and applies to scoring module.
 *
 * @return true if successful, false if Flash data invalid
 */
bool Config_Load(void);

/**
 * @brief Save current scoring windows to Flash
 *
 * Reads current windows from scoring module and writes to Flash memory.
 * Performs erase-program cycle on the config page.
 *
 * @return true if successful, false if Flash operation failed
 */
bool Config_Save(void);

/**
 * @brief Reset configuration to defaults
 *
 * Restores default scoring windows and optionally saves to Flash.
 *
 * @param save_to_flash If true, writes defaults to Flash immediately
 * @return true if successful
 */
bool Config_ResetToDefaults(bool save_to_flash);

/**
 * @brief Validate configuration data
 *
 * Checks magic number, version, window ordering, ranges, and checksum.
 * Also detects uninitialized flash (all 0xFF).
 *
 * @param config Pointer to configuration structure to validate
 * @return true if valid, false otherwise
 */
bool Config_Validate(const ConfigData* config);

/**
 * @brief Get pointer to current configuration in Flash
 *
 * @return Pointer to Flash-resident config structure (read-only)
 */
const ConfigData* Config_GetFlashData(void);

/**
 * @brief Calculate CRC32 checksum for configuration
 *
 * @param config Pointer to configuration structure
 * @return CRC32 checksum value
 */
uint32_t Config_CalculateChecksum(const ConfigData* config);

/**
 * @brief Check if Flash contains valid configuration
 *
 * @return true if valid config exists in Flash
 */
bool Config_IsValid(void);

/**
 * @brief Erase configuration from Flash
 *
 * WARNING: This erases the entire config page. Use with caution.
 *
 * @return true if successful
 */
bool Config_Erase(void);

/**
 * @brief Detect actual flash size from device information register
 *
 * Reads the FLASHSIZE register to determine the actual flash size
 * of the device in use.
 *
 * @return Flash size in kilobytes (e.g., 256 for 256KB device)
 */
uint32_t Config_GetFlashSize(void);

/**
 * @brief Validate that configuration address is within valid flash range
 *
 * Checks that CONFIG_FLASH_ADDRESS falls within the actual flash memory
 * of the device. This prevents hardfaults from accessing invalid addresses.
 *
 * @return true if address is valid, false if out of range
 */
bool Config_ValidateAddress(void);

/**
 * @brief Get detailed validation status string
 *
 * Provides human-readable description of configuration status.
 * Useful for debugging and user feedback.
 *
 * @return Pointer to status string
 */
const char* Config_GetStatusString(void);

#endif /* CONFIG_STORAGE_H */
