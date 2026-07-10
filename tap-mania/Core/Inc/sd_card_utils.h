/**
 ******************************************************************************
 * @file           : sd_card_utils.h
 * @brief          : SD Card Utilities for FAT filesystem operations
 ******************************************************************************
 */

#ifndef SD_CARD_UTILS_H
#define SD_CARD_UTILS_H

#include "fatfs.h"
#include "main.h"
#include <stdbool.h>
#include <stdint.h>

// File list structure for directory listing
typedef struct {
    int count; // how many entries filled
    char names[MAX_FILES][MAX_NAME_LEN]; // filenames (no directories)
} FileList;

/**
 * @brief Read pin state for SD card interface
 * @param port GPIO port
 * @param pin GPIO pin
 * @return 1 if pin is RESET, 0 if SET
 */
uint8_t hal_port_read(const uint32_t* port, const uint32_t pin);

/**
 * @brief Send 80 clock cycles to SD card for initialization
 */
void sd_send_80_clocks(void);

/**
 * @brief Copy TCHAR string safely
 * @param dst Destination buffer
 * @param src Source string
 * @param maxlen Maximum length including null terminator
 */
void tchar_copy(TCHAR* dst, const TCHAR* src, size_t maxlen);

/**
 * @brief List files in current directory
 * @return FileList structure containing file names
 */
FileList list_current_dir(void);

/**
 * @brief Read next data line from TSQ file
 * @param fil File pointer
 * @param dst Destination buffer
 * @param dst_len Buffer length
 * @param saw_end Output parameter set to 1 if end delimiter seen
 * @return 1 if line read successfully, 0 otherwise
 */
int read_next_data_line(FIL* fil, char* dst, UINT dst_len, int* saw_end);

/**
 * @brief Convert character to bit value
 * @param c Character ('0' or '1')
 * @return 0 or 1
 */
int to_bit(char c);

/**
 * @brief Set LED matrix values from buffer
 * @param readBuf Buffer containing LED data (4 characters)
 * @param ledIndex Starting LED index
 */
void set_LED_matrix(char* readBuf, uint16_t ledIndex);

/**
 * @brief Render window to LED matrix
 * @param window 2D array of window data
 * @param rowsFilled Number of rows filled
 * @param baseLedIndex Base LED index for rendering
 */
void render_window(char window[NUM_ROWS][MAX_NAME_LEN],
    int rowsFilled,
    uint16_t baseLedIndex);

/**
 * @brief Flush LED matrix (read header from file)
 * @param fil File pointer
 */
void flush_LED_matrix(FIL* fil);

/**
 * @brief Enqueue note event (legacy function)
 * @param lane Lane identifier
 * @param t_hit Hit timestamp
 */
void enqueue_note(uint8_t lane, uint32_t t_hit);
char*** sd_list_tsq_files(int* tsqCount);
void sd_get_name_artist(const char* filename, char** results);
#endif /* SD_CARD_UTILS_H */
