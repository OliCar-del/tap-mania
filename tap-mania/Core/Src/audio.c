#include "audio.h"
#include "fatfs.h"
#include "uart_debug.h"

UINT bytesRead;
FRESULT res;

// Sorry for all the magic numbers,

void playWav(FIL file, char *filePath, uint16_t wavBuf[512 * 2],
             SAI_HandleTypeDef hsai_BlockA1) {
  res = f_open(&file, filePath, FA_READ);
  if (res != FR_OK) {
    myprintf("Failed to read file %s\n", filePath);
    return;
  }

  f_lseek(&file, 44); // Skip WAV header
  f_read(&file, &wavBuf, 512 * 2, &bytesRead);
  // Start DMA for the full buffer (1024 bytes)
  myprintf("Beginning transmission of data for file\n");
  // No matter what I do here, this complains about the cast.
  HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint16_t *)wavBuf, 512);
}

void stopAudio(SAI_HandleTypeDef hsai_BlockA1) {
  HAL_SAI_DMAStop(&hsai_BlockA1);
}
