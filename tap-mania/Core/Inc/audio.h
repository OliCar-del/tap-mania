#include "stm32l4xx_hal.h"
#include "fatfs.h"


void playWav(FIL file, char* filePath, uint16_t wavBuf[512*2], SAI_HandleTypeDef hsai_BlockA1);
void stopAudio(SAI_HandleTypeDef hsai_BlockA1);
