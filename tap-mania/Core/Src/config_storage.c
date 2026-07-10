/**
 ******************************************************************************
 * @file           : config_storage.c
 * @brief          : Non-volatile configuration storage implementation
 ******************************************************************************
 */

#include "config_storage.h"
#include "main.h" // Includes the main STM32 HAL drivers
#include "scoring.h"
#include <stdio.h> // Defines sprintf
#include <string.h>

/* External UART handle for debug output */
extern UART_HandleTypeDef huart2;

/* Static variables for status tracking */
static char status_string[128] = "Not initialized";

/**
 * @brief Detect actual flash size from device information register
 *
 * The STM32L4 stores the flash size in a special register at 0x1FFF75E0.
 * This register contains the flash size in KB (e.g., 0x0100 = 256KB).
 */
uint32_t Config_GetFlashSize(void) {
  // Read flash size register (16-bit value in KB)
  uint16_t flash_size_kb = *((uint16_t *)FLASHSIZE_BASE);
  return (uint32_t)flash_size_kb;
}

/**
 * @brief Validate that configuration address is within valid flash range
 *
 * This is a critical safety check that prevents hardfaults from attempting
 * to read from addresses outside the actual flash memory.
 */
bool Config_ValidateAddress(void) {
  char uart_buf[128];
  int uart_buf_len;

  // Get actual flash size
  uint32_t flash_size_kb = Config_GetFlashSize();
  uint32_t flash_size_bytes = flash_size_kb * 1024;
  uint32_t flash_end_address = FLASH_BASE_ADDRESS + flash_size_bytes - 1;

  uart_buf_len =
      sprintf(uart_buf, "CONFIG: Flash size detected: %lu KB (%lu bytes)\r\n",
              flash_size_kb, flash_size_bytes);
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

  uart_buf_len = sprintf(uart_buf, "CONFIG: Flash range: 0x%08X to 0x%08X\r\n",
                         FLASH_BASE_ADDRESS, flash_end_address);
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

  uart_buf_len =
      sprintf(uart_buf, "CONFIG: Config address: 0x%08X (page %d)\r\n",
              CONFIG_FLASH_ADDRESS, CONFIG_FLASH_PAGE);
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

  // Check if config address is within valid range
  if (CONFIG_FLASH_ADDRESS < FLASH_BASE_ADDRESS) {
    uart_buf_len = sprintf(
        uart_buf, "CONFIG: ERROR - Config address below flash start!\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Address below flash range",
            sizeof(status_string) - 1);
    return false;
  }

  if (CONFIG_FLASH_ADDRESS > flash_end_address) {
    uart_buf_len = sprintf(
        uart_buf,
        "CONFIG: ERROR - Config address 0x%08X is beyond flash end 0x%08X!\r\n",
        CONFIG_FLASH_ADDRESS, flash_end_address);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

    // Calculate what the correct address should be
    uint32_t last_page = (flash_size_bytes / FLASH_PAGE_SIZE) - 1;
    uint32_t correct_address =
        FLASH_BASE_ADDRESS + (last_page * FLASH_PAGE_SIZE);

    uart_buf_len = sprintf(uart_buf,
                           "CONFIG: This device has %lu pages. Use page %lu at "
                           "address 0x%08lX\r\n",
                           last_page + 1, last_page, correct_address);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

    uart_buf_len = sprintf(uart_buf,
                           "CONFIG: Update config_storage.h:\r\n"
                           "  #define CONFIG_FLASH_PAGE %lu\r\n"
                           "  #define CONFIG_FLASH_ADDRESS 0x%08lX\r\n",
                           last_page, correct_address);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

    strncpy(status_string, "Address beyond flash range",
            sizeof(status_string) - 1);
    return false;
  }

  // Check page alignment
  uint32_t page_offset = CONFIG_FLASH_ADDRESS - FLASH_BASE_ADDRESS;
  if ((page_offset % FLASH_PAGE_SIZE) != 0) {
    uart_buf_len =
        sprintf(uart_buf,
                "CONFIG: WARNING - Address not page-aligned (offset: %lu)\r\n",
                page_offset % FLASH_PAGE_SIZE);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
  }

  uart_buf_len = sprintf(uart_buf, "CONFIG: Address validation passed\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

  return true;
}

/**
 * @brief Calculate CRC32 checksum
 *
 * Simple CRC32 implementation for data validation.
 * Uses polynomial 0xEDB88320 (reversed).
 */
uint32_t Config_CalculateChecksum(const ConfigData *config) {
  uint32_t crc = 0xFFFFFFFF;
  const uint8_t *data = (const uint8_t *)config;

  // Calculate CRC over everything except the checksum field itself
  size_t length = offsetof(ConfigData, checksum);

  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
  }

  return ~crc;
}

/**
 * @brief Check if flash memory is uninitialized (all 0xFF)
 *
 * When flash is erased, all bits are set to 1, resulting in 0xFF values.
 * This helper function detects this condition to provide better error messages.
 */
static bool IsFlashUninitialized(const ConfigData *config) {
  const uint8_t *data = (const uint8_t *)config;
  size_t length = sizeof(ConfigData);

  // Check if all bytes are 0xFF
  for (size_t i = 0; i < length; i++) {
    if (data[i] != 0xFF) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Validate configuration structure
 *
 * Enhanced validation with improved error detection and reporting.
 * Distinguishes between uninitialized flash, corrupted data, and valid data.
 */
bool Config_Validate(const ConfigData *config) {
  char uart_buf[128];
  int uart_buf_len;

  if (!config) {
    strncpy(status_string, "Null pointer", sizeof(status_string) - 1);
    return false;
  }

  // Check if flash is uninitialized (all 0xFF)
  if (IsFlashUninitialized(config)) {
    uart_buf_len = sprintf(uart_buf, "CONFIG: Flash is uninitialized (all "
                                     "0xFF) - first run or after erase\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Flash uninitialized", sizeof(status_string) - 1);
    return false;
  }

  // Check magic number
  if (config->magic != CONFIG_MAGIC) {
    uart_buf_len = sprintf(
        uart_buf, "CONFIG: Invalid magic number: 0x%08X (expected 0x%08X)\r\n",
        config->magic, CONFIG_MAGIC);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Invalid magic number", sizeof(status_string) - 1);
    return false;
  }

  // Check version
  if (config->version != CONFIG_VERSION) {
    uart_buf_len =
        sprintf(uart_buf, "CONFIG: Version mismatch: %lu (expected %d)\r\n",
                config->version, CONFIG_VERSION);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Version mismatch", sizeof(status_string) - 1);
    return false;
  }

  // Validate window values (1-500ms, inclusive)
  if (config->windows.perfect_radius_ms < 1 ||
      config->windows.perfect_radius_ms > 500) {
    uart_buf_len =
        sprintf(uart_buf, "CONFIG: Perfect window out of range: %u\r\n",
                config->windows.perfect_radius_ms);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Perfect window out of range",
            sizeof(status_string) - 1);
    return false;
  }
  if (config->windows.good_radius_ms < 1 ||
      config->windows.good_radius_ms > 500) {
    uart_buf_len = sprintf(uart_buf, "CONFIG: Good window out of range: %u\r\n",
                           config->windows.good_radius_ms);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Good window out of range",
            sizeof(status_string) - 1);
    return false;
  }
  if (config->windows.ok_radius_ms < 1 || config->windows.ok_radius_ms > 500) {
    uart_buf_len = sprintf(uart_buf, "CONFIG: OK window out of range: %u\r\n",
                           config->windows.ok_radius_ms);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "OK window out of range", sizeof(status_string) - 1);
    return false;
  }
  if (config->windows.poor_radius_ms < 1 ||
      config->windows.poor_radius_ms > 500) {
    uart_buf_len = sprintf(uart_buf, "CONFIG: Poor window out of range: %u\r\n",
                           config->windows.poor_radius_ms);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Poor window out of range",
            sizeof(status_string) - 1);
    return false;
  }

  // Validate window ordering (Perfect < Good < OK < Poor)
  if (config->windows.perfect_radius_ms >= config->windows.good_radius_ms) {
    uart_buf_len = sprintf(
        uart_buf, "CONFIG: Window ordering error: Perfect(%u) >= Good(%u)\r\n",
        config->windows.perfect_radius_ms, config->windows.good_radius_ms);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Window ordering error (P>=G)",
            sizeof(status_string) - 1);
    return false;
  }
  if (config->windows.good_radius_ms >= config->windows.ok_radius_ms) {
    uart_buf_len = sprintf(
        uart_buf, "CONFIG: Window ordering error: Good(%u) >= OK(%u)\r\n",
        config->windows.good_radius_ms, config->windows.ok_radius_ms);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Window ordering error (G>=O)",
            sizeof(status_string) - 1);
    return false;
  }
  if (config->windows.ok_radius_ms >= config->windows.poor_radius_ms) {
    uart_buf_len = sprintf(
        uart_buf, "CONFIG: Window ordering error: OK(%u) >= Poor(%u)\r\n",
        config->windows.ok_radius_ms, config->windows.poor_radius_ms);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Window ordering error (O>=Po)",
            sizeof(status_string) - 1);
    return false;
  }

  // Validate checksum
  uint32_t calculated_crc = Config_CalculateChecksum(config);
  if (config->checksum != calculated_crc) {
    uart_buf_len = sprintf(
        uart_buf, "CONFIG: Checksum mismatch: 0x%08lX (expected 0x%08lX)\r\n",
        config->checksum, calculated_crc);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    strncpy(status_string, "Checksum mismatch - data corrupted",
            sizeof(status_string) - 1);
    return false;
  }

  strncpy(status_string, "Valid configuration", sizeof(status_string) - 1);
  return true;
}

/**
 * @brief Get pointer to Flash configuration data
 */
const ConfigData *Config_GetFlashData(void) {
  return (const ConfigData *)CONFIG_FLASH_ADDRESS;
}

/**
 * @brief Check if Flash contains valid configuration
 */
bool Config_IsValid(void) {
  // First validate the address is safe to read
  if (!Config_ValidateAddress()) {
    return false;
  }

  // Now safe to read from flash
  const ConfigData *flash_config = Config_GetFlashData();
  return Config_Validate(flash_config);
}

/**
 * @brief Get detailed validation status string
 */
const char *Config_GetStatusString(void) { return status_string; }

/**
 * @brief Initialize configuration system
 *
 * Enhanced with flash size detection and address validation.
 */
bool Config_Init(void) {
  char uart_buf[128];
  int uart_buf_len;

  uart_buf_len = sprintf(uart_buf, "\r\n=== CONFIG: Initializing ===\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

  // CRITICAL: Validate address before attempting to read flash
  if (!Config_ValidateAddress()) {
    uart_buf_len = sprintf(
        uart_buf,
        "CONFIG: CRITICAL ERROR - Invalid flash address configuration!\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

    uart_buf_len =
        sprintf(uart_buf, "CONFIG: Using default windows for safety\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

    Config_ResetToDefaults(false);
    return false;
  }

  // Now safe to check if valid config exists in Flash
  if (Config_IsValid()) {
    uart_buf_len = sprintf(uart_buf, "CONFIG: Valid config found in Flash\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

    // Load from Flash
    if (Config_Load()) {
      const ConfigData *config = Config_GetFlashData();
      uart_buf_len = sprintf(
          uart_buf, "CONFIG: Loaded windows: P=%u G=%u O=%u Po=%u\r\n",
          config->windows.perfect_radius_ms, config->windows.good_radius_ms,
          config->windows.ok_radius_ms, config->windows.poor_radius_ms);
      HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
      return true;
    }
  }

  // No valid config - report why
  uart_buf_len = sprintf(uart_buf, "CONFIG: No valid config (%s)\r\n",
                         Config_GetStatusString());
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

  uart_buf_len = sprintf(uart_buf, "CONFIG: Using defaults\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

  Config_ResetToDefaults(false);
  return false;
}

/**
 * @brief Load configuration from Flash
 */
bool Config_Load(void) {
  const ConfigData *flash_config = Config_GetFlashData();

  if (!Config_Validate(flash_config)) {
    return false;
  }

  // Apply to scoring module
  return Scoring_SetWindows(&flash_config->windows);
}

/**
 * @brief Erase Flash configuration page
 */
bool Config_Erase(void) {
  HAL_StatusTypeDef status;
  FLASH_EraseInitTypeDef erase_init;
  uint32_t page_error = 0;

  // Validate address before erasing
  if (!Config_ValidateAddress()) {
    return false;
  }

  // Unlock Flash
  status = HAL_FLASH_Unlock();
  if (status != HAL_OK) {
    return false;
  }

  // Configure erase
  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Page = CONFIG_FLASH_PAGE;
  erase_init.NbPages = 1;

  // Erase page
  status = HAL_FLASHEx_Erase(&erase_init, &page_error);

  // Lock Flash
  HAL_FLASH_Lock();

  return (status == HAL_OK);
}

/**
 * @brief Save configuration to Flash
 */
bool Config_Save(void) {
  char uart_buf[128];
  int uart_buf_len;
  HAL_StatusTypeDef status;

  uart_buf_len = sprintf(uart_buf, "CONFIG: Saving to Flash...\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

  // Validate address before saving
  if (!Config_ValidateAddress()) {
    uart_buf_len =
        sprintf(uart_buf, "CONFIG: ERROR - Cannot save, invalid address\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    return false;
  }

  // Create configuration structure
  ConfigData config;
  config.magic = CONFIG_MAGIC;
  config.version = CONFIG_VERSION;

  // Get current windows from scoring module
  const ScoringWindows *current_windows = Scoring_GetWindows();
  memcpy(&config.windows, current_windows, sizeof(ScoringWindows));

  // Calculate checksum
  config.checksum = Config_CalculateChecksum(&config);

  // Validate before writing
  if (!Config_Validate(&config)) {
    uart_buf_len =
        sprintf(uart_buf, "CONFIG: ERROR - Invalid config, not saving\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    return false;
  }

  // Erase Flash page
  if (!Config_Erase()) {
    uart_buf_len = sprintf(uart_buf, "CONFIG: ERROR - Flash erase failed\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    return false;
  }

  // Unlock Flash for programming
  status = HAL_FLASH_Unlock();
  if (status != HAL_OK) {
    uart_buf_len = sprintf(uart_buf, "CONFIG: ERROR - Flash unlock failed\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    return false;
  }

  // Program Flash (write as 64-bit words)
  uint64_t *src = (uint64_t *)&config;
  uint32_t dest = CONFIG_FLASH_ADDRESS;
  size_t num_words = (sizeof(ConfigData) + 7) / 8; // Round up to 64-bit words

  bool success = true;
  for (size_t i = 0; i < num_words; i++) {
    status =
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dest + (i * 8), src[i]);
    if (status != HAL_OK) {
      success = false;
      break;
    }
  }

  // Lock Flash
  HAL_FLASH_Lock();

  if (success) {
    uart_buf_len =
        sprintf(uart_buf, "CONFIG: Saved - P=%u G=%u O=%u Po=%u\r\n",
                config.windows.perfect_radius_ms, config.windows.good_radius_ms,
                config.windows.ok_radius_ms, config.windows.poor_radius_ms);
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

    // Verify by reading back
    if (Config_IsValid()) {
      uart_buf_len = sprintf(uart_buf, "CONFIG: Verification passed\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
      return true;
    } else {
      uart_buf_len =
          sprintf(uart_buf, "CONFIG: WARNING - Verification failed\r\n");
      HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
      return false;
    }
  } else {
    uart_buf_len = sprintf(uart_buf, "CONFIG: ERROR - Flash write failed\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
    return false;
  }
}

/**
 * @brief Reset to default configuration
 */
bool Config_ResetToDefaults(bool save_to_flash) {
  char uart_buf[128];
  int uart_buf_len;

  ScoringWindows defaults = {.perfect_radius_ms = DEFAULT_PERFECT_RADIUS_MS,
                             .good_radius_ms = DEFAULT_GOOD_RADIUS_MS,
                             .ok_radius_ms = DEFAULT_OK_RADIUS_MS,
                             .poor_radius_ms = DEFAULT_POOR_RADIUS_MS};

  uart_buf_len = sprintf(uart_buf, "CONFIG: Resetting to defaults\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

  // Apply to scoring module
  if (!Scoring_SetWindows(&defaults)) {
    return false;
  }

  // Optionally save to Flash
  if (save_to_flash) {
    return Config_Save();
  }

  return true;
}
