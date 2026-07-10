/**
 ******************************************************************************
 * @file           : sd_card_utils.c
 * @brief          : SD Card Utilities Implementation
 ******************************************************************************
 */

#include "sd_card_utils.h"
#include "ARGB.h"
#include <stdio.h>
#include <string.h>

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart2;
char sd_path[4];
/**
 * @brief Read pin state for SD card interface
 */
uint8_t hal_port_read(const uint32_t *port, const uint32_t pin) {
  // Cast the opaque pointer back to GPIO_TypeDef* for HAL
  GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
  GPIO_PinState s = HAL_GPIO_ReadPin(gpio, pin);
  return (s == GPIO_PIN_RESET) ? 1 : 0;
}

/**
 * @brief Send 80 clock cycles to SD card
 */
void sd_send_80_clocks(void) {
  uint8_t ff[10] = {[0 ... 9] = 0xFF};
  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET); // CS high
  HAL_SPI_Transmit(&hspi1, ff, sizeof ff, HAL_MAX_DELAY); // >=80 clocks
}

/**
 * @brief Copy TCHAR string safely
 */
void tchar_copy(TCHAR *dst, const TCHAR *src, size_t maxlen) {
  size_t i = 0;
  for (; i + 1 < maxlen && src[i]; ++i)
    dst[i] = src[i];
  dst[i] = 0; // Null terminate
}

/**
 * @brief List files in current directory
 */
FileList list_current_dir(void) {
  FileList fileList = {0};
  FRESULT fres;
  DIR dir;
  FILINFO fno;

  fres = f_opendir(&dir, ""); // Root directory
  if (fres != FR_OK) {
    char msg[64];
    snprintf(msg, sizeof(msg), "f_opendir error (%d)\r\n", fres);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 1000);
    return fileList; // empty list
  }

  for (;;) {
    fres = f_readdir(&dir, &fno);
    if (fres != FR_OK || fno.fname[0] == 0)
      break; // error or end

    const char *name = fno.fname;

    // skip "." and ".."
    if (name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2])))
      continue;

    // only files (skip directories)
    if (fno.fattrib & AM_DIR)
      continue;

    if (fileList.count < MAX_FILES) {
      strncpy(fileList.names[fileList.count], name, MAX_NAME_LEN - 1);
      fileList.names[fileList.count][MAX_NAME_LEN - 1] = '\0';
      fileList.count++;
    }
    //   } else {
    //       char msg[64];
    //       snprintf(msg, sizeof(msg), "list_current_dir: truncated at %d
    //       entries\r\n", MAX_FILES); HAL_UART_Transmit(&huart2, (uint8_t*)msg,
    //       strlen(msg), 1000); break;
    //   }
  }

  f_closedir(&dir);
  return fileList;
}

/**
 * @brief Read next data line from file
 */
int read_next_data_line(FIL *fil, char *dst, UINT dst_len, int *saw_end) {
  TCHAR tmp[MAX_NAME_LEN];
  *saw_end = 0;

  for (;;) {
    if (f_gets((TCHAR *)tmp, MAX_NAME_LEN, fil) == 0) {
      // EOF or read error
      return 0;
    }

    char c0 = ((char *)tmp)[0];

    if (c0 == ';') {
      *saw_end = 1;
      return 0;
    }

    if (c0 == ',') {
      continue;
    }

    // This is a data line; copy into dst (truncate safely)
    // Strip trailing newline if present
    size_t n = 0;
    while (n + 1 < dst_len && ((char *)tmp)[n] != '\0' &&
           ((char *)tmp)[n] != '\r' && ((char *)tmp)[n] != '\n') {
      dst[n] = ((char *)tmp)[n];
      n++;
    }
    dst[n] = '\0';
    return 1;
  }
}

/**
 * @brief Convert character to bit value
 */
int to_bit(char c) { return (c == '1') ? 1 : 0; }

/**
 * @brief Set LED matrix values from buffer
 */
void set_LED_matrix(char *readBuf, uint16_t ledIndex) {
  int r, g, b, y;
  char msg[128];
  int msg_len;

  r = readBuf[0] - '0';
  g = readBuf[1] - '0';
  b = readBuf[2] - '0';
  y = readBuf[3] - '0';

  msg_len = snprintf(msg, sizeof(msg), "r = %d\r\n", r);
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, msg_len, 1000);
  msg_len = snprintf(msg, sizeof(msg), "g =%d\r\n", g);
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, msg_len, 1000);
  msg_len = snprintf(msg, sizeof(msg), "b = %d\r\n", b);
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, msg_len, 1000);
  msg_len = snprintf(msg, sizeof(msg), "y = %d\r\n", y);
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, msg_len, 1000);

  if (r == 2) {
    ARGB_SetRGB(ledIndex, CONSTANT, CONSTANT, CONSTANT);
  } else {
    ARGB_SetRGB(ledIndex, r * CONSTANT, 0, 0);
  }

  if (g == 2) {
    ARGB_SetRGB(ledIndex + GREEN_COLUMN, CONSTANT, CONSTANT, CONSTANT);
  } else {
    ARGB_SetRGB(ledIndex + GREEN_COLUMN, 0, g * CONSTANT, 0);
  }

  if (b == 2) {
    ARGB_SetRGB(ledIndex + BLUE_COLUMN, CONSTANT, CONSTANT, CONSTANT);
  } else {
    ARGB_SetRGB(ledIndex + BLUE_COLUMN, 0, 0, b * CONSTANT);
  }

  if (y == 2) {
    ARGB_SetRGB(ledIndex + YELLOW_COLUMN, CONSTANT, CONSTANT, CONSTANT);
  } else {
    ARGB_SetRGB(ledIndex + YELLOW_COLUMN, y * CONSTANT, y * CONSTANT, 0);
  }
}

/**
 * @brief Render window to LED matrix
 */
void render_window(char window[NUM_ROWS][MAX_NAME_LEN], int rowsFilled,
                   uint16_t baseLedIndex) {
  for (int r = 0; r < rowsFilled; ++r) {
    uint16_t idx = (uint16_t)(baseLedIndex + r * ROW_STRIDE);
    set_LED_matrix(window[r], idx);
  }

  while (ARGB_Ready() == ARGB_BUSY) { /* spin until ready */
  }
  ARGB_Show();
}

/**
 * @brief Flush LED matrix
 */
void flush_LED_matrix(FIL *fil) {
  TCHAR tmp[MAX_NAME_LEN];
  char name[128] = {0}, artist[128] = {0};
  int bpm = 0;

  while (f_gets((TCHAR *)tmp, 30, fil)) {
    if (strncmp((char *)tmp, "---", 3) == 0)
      break; // end of header
    if (sscanf((char *)tmp, "Name: %[^\n]", name) == 1)
      continue;
    if (sscanf((char *)tmp, "Artist: %[^\n]", artist) == 1)
      continue;
    if (sscanf((char *)tmp, "BPM: %d", &bpm) == 1)
      continue;
  }
}

/**
 * @brief Enqueue note event (legacy function)
 */
void enqueue_note(uint8_t lane, uint32_t t_hit) {
  extern NoteEvent notes[MAX_NOTES];
  extern int notes_tail;

  notes[notes_tail] = (NoteEvent){.t_hit = t_hit, .lane = lane, .used = 0};
  notes_tail = (notes_tail + 1) % MAX_NOTES;
}

void sd_get_name_artist(const char *filename, char **results) {
  FIL file;
  FRESULT fp;

  fp = f_open(&file, filename, FA_READ);
  if (fp != 0) {
    // printf("The helper function has FAILED\n\r");
    return;
  }
  char tmp_name[64];
  char tmp_artist[64];

  f_gets(tmp_name, sizeof(tmp_name), &file);
  f_gets(tmp_artist, sizeof(tmp_artist), &file);

  f_close(&file);
  int name_offset = 6;
  int artist_offset = 8;

  char *name = tmp_name + name_offset;
  char *artist = tmp_artist + artist_offset;

  results[0] = malloc(sizeof(char) * strlen(name) + 1);
  results[1] = malloc(sizeof(char) * strlen(artist) + 1);

  strcpy(results[0], name);
  strcpy(results[1], artist);
  // printf("The helper function has now finished copying data from the sd to
  // memory\n\r");
}

char ***sd_list_tsq_files(int *tsqCount) {
  DIR dir;
  FILINFO fno;
  f_opendir(&dir, sd_path);

  int maxSize = 2;
  *tsqCount = 0;
  char ***results = malloc(sizeof(char **) * 3);
  results[0] = malloc(sizeof(char *) * maxSize);
  results[1] = malloc(sizeof(char *) * maxSize);
  results[2] = malloc(sizeof(char *) * maxSize);
  // printf("Mallocs Complete\n\r");
  while (1) {
    // printf("At the start of the while loop, moving onto reading the next
    // file\n\r");
    f_readdir(&dir, &fno);
    if (fno.fname[0] == 0)
      break;

    const char *name = fno.fname;
    const char *ext = strrchr(name, '.');

    if (ext && strcasecmp(ext, ".tsq") == 0) {
      if (*tsqCount >= maxSize) {
        maxSize *= 2;
        results[0] = realloc(results[0], sizeof(char *) * maxSize);
        results[1] = realloc(results[1], sizeof(char *) * maxSize);
        results[2] = realloc(results[2], sizeof(char *) * maxSize);
      }
      // printf("Inside the biggest loop with the current name being %s\n",
      // name);
      char **buffer = malloc(sizeof(char *) * 2);
      sd_get_name_artist(name, buffer);

      results[0][*tsqCount] = malloc(strlen(buffer[0]) + 1);
      strcpy(results[0][*tsqCount], buffer[0]);

      results[1][*tsqCount] = malloc(strlen(buffer[1]) + 1);
      strcpy(results[1][*tsqCount], buffer[1]);

      results[2][*tsqCount] = malloc(strlen(name) + 1);
      strcpy(results[2][*tsqCount], name);
      // printf("strcpy's are complete, incrementing tsqCount\n\r");
      (*tsqCount)++;
    }
  }

  f_closedir(&dir);
  return results;
}