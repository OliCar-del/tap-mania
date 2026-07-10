/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body (REFACTORED with modular structure)
 ******************************************************************************
 * @author         : Oliver Carnew
 * @date           : 26-Sep-2025 (Refactored: 2025)
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ARGB.h"
#include "SH1106.h"
#include "button_debounce.h"
#include "button_handler.h"
#include "config_input.h"
#include "config_storage.h"
#include "ff.h"
#include "led_control.h"
#include "led_display.h"
#include "libs.h"
#include "scoring.h"
#include "sequence.h"
#include "tsq_parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
// New modular includes
#include "beat_event.h"
#include "command_handler.h"
#include "game_state.h"
#include "led_control.h"
#include "sd_card_detect.h"
#include "sd_card_utils.h"
#include "test_sequences.h"
#include "uart_debug.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LED_ON_TIME_MS 100
#define INIT_DELAY_SHORT_MS 50
#define INIT_DELAY_LONG_MS 200
#define INIT_DELAY_VERY_LONG_MS 8000
#define TIM7_OVERFLOW_US 655360
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SAI_HandleTypeDef hsai_BlockA1;
DMA_HandleTypeDef hdma_sai1_a;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;
DMA_HandleTypeDef hdma_tim2_ch2_ch4;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */
// Global variables shared across modules
volatile bool beat_event_flag = false;
volatile uint32_t last_beat_timestamp = 0;
volatile uint32_t beat_count = 0;
volatile uint16_t current_bpm = 30;
uint32_t sequence_length = 0;
volatile uint32_t tim7_overflow_count = 0;

// DMA UART Buffers
uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
uint8_t uart_tx_buffer[UART_TX_BUFFER_SIZE];
volatile uint32_t uart_rx_head = 0;
uint32_t uart_rx_tail = 0;
volatile uint8_t uart_tx_busy = 0;

// LED control
LEDControl led_control = {0};
uint32_t beat_led_on_tick = 0;
uint32_t beat_led_off_tick = 0;

// Legacy note event variables (kept for compatibility)
NoteEvent notes[MAX_NOTES];
int notes_head = 0, notes_tail = 0;
int32_t total_score = 0;
uint32_t odd_count = 0;
int selected_song = 0;
bool scrolled = false;
bool rightPressed = false;
bool selectedNewSong = false;

static bool config_menu_enabled = true; // Set to true to show config option
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM6_Init(void);
static void MX_SAI1_Init(void);
static void MX_TIM7_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
void Update_BPM_Timer(uint16_t bpm);
uint32_t GetMicrosecondTimestamp(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Prepend config option to song list
 * This modifies tsqFiles to include a "CHANGE CONFIG->" entry at index 0
 */
void PrependConfigOption(char ****tsqFiles_ptr, int *tsqCount_ptr) {
  if (!config_menu_enabled)
    return;

  char ***tsqFiles = *tsqFiles_ptr;
  int tsqCount = *tsqCount_ptr;

  // Allocate new arrays with space for one extra entry
  char ***newTsqFiles = malloc(sizeof(char **) * 3);
  newTsqFiles[0] = malloc(sizeof(char *) * (tsqCount + 1)); // Names
  newTsqFiles[1] = malloc(sizeof(char *) * (tsqCount + 1)); // Artists
  newTsqFiles[2] = malloc(sizeof(char *) * (tsqCount + 1)); // Filenames

  // Add config option at index 0
  newTsqFiles[0][0] = malloc(strlen("CHANGE CONFIG->") + 1);
  strcpy(newTsqFiles[0][0], "CHANGE CONFIG->");

  newTsqFiles[1][0] = malloc(strlen("") + 1);
  strcpy(newTsqFiles[1][0], "");

  newTsqFiles[2][0] = malloc(strlen("__CONFIG__") + 1);
  strcpy(newTsqFiles[2][0], "__CONFIG__");

  // Copy existing songs starting at index 1
  for (int i = 0; i < tsqCount; i++) {
    newTsqFiles[0][i + 1] = malloc(strlen(tsqFiles[0][i]) + 1);
    strcpy(newTsqFiles[0][i + 1], tsqFiles[0][i]);

    newTsqFiles[1][i + 1] = malloc(strlen(tsqFiles[1][i]) + 1);
    strcpy(newTsqFiles[1][i + 1], tsqFiles[1][i]);

    newTsqFiles[2][i + 1] = malloc(strlen(tsqFiles[2][i]) + 1);
    strcpy(newTsqFiles[2][i + 1], tsqFiles[2][i]);
  }

  // Free old arrays (if needed - be careful with this based on your memory
  // management) In your case, you might want to keep the old arrays since
  // they're from SD card

  // Update pointers
  *tsqFiles_ptr = newTsqFiles;
  *tsqCount_ptr = tsqCount + 1;
}

/**
 * @brief Get microsecond timestamp using TIM7 with overflow handling
 */
uint32_t GetMicrosecondTimestamp(void) {
  uint32_t overflow_snapshot;
  uint16_t counter_snapshot;

  do {
    overflow_snapshot = tim7_overflow_count;
    counter_snapshot = TIM7->CNT;
    __DSB();
  } while (overflow_snapshot != tim7_overflow_count);

  return (overflow_snapshot * TIM7_OVERFLOW_US) + (counter_snapshot * 10);
}

/**
 * @brief  Calculates and updates the TIM6 period to match a given BPM.
 * @param  bpm: The desired beats per minute.
 * @retval None
 */
void Update_BPM_Timer(uint16_t bpm) {
  if (bpm > 0) {
    uint32_t timer_period = 600000 / bpm;
    __HAL_TIM_SET_AUTORELOAD(&htim6, timer_period - 1);
  }
}

/**
 * @brief  TIM6 Period Elapsed Callback (Metronome ISR).
 * @param  htim: TIM handle.
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM6) {
    last_beat_timestamp = GetMicrosecondTimestamp();
    beat_count++;
    beat_event_flag = true;
  } else if (htim->Instance == TIM7) {
    tim7_overflow_count++;
  }
}

/**
 * @brief UART TX Complete Callback
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2) {
    uart_tx_busy = 0;
  }
}

/**
 * @brief UART Error Callback
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2) {
    HAL_UART_Receive_DMA(&huart2, uart_rx_buffer, UART_RX_BUFFER_SIZE);
    uart_tx_busy = 0;
  }
}
bool fillWavBuffer = false;
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  // myprintf("TXCallBack");
  fillWavBuffer = true;
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */
  char uart_buf[256];
  int uart_buf_len;
  uint8_t selected_sequence = 0;

  FATFS FatFs;
  FIL fil;
  FRESULT fres;
  FileList fileList;
  TSQMetadata tsq_metadata;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  HAL_Delay(INIT_DELAY_LONG_MS);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_TIM6_Init();
  MX_SAI1_Init();
  MX_TIM7_Init();
  MX_FATFS_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  // ONE-TIME HARDWARE & MODULE INITIALIZATION
  uint8_t sh1106InitRet = SH1106_Init();
  myprintf("sh1106InitRet = %d\r\n", sh1106InitRet);
  UART_StartDMAReception();
  uart_buf_len = sprintf(uart_buf, "\r\n\r\n=== tpmania v1.0 ===\r\n");
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);
  HAL_Delay(100);
  uart_buf_len = sprintf(uart_buf, "\r\n=== DMA UART INITIALIZED ===\r\n");
  UART_Transmit((uint8_t *)uart_buf, uart_buf_len);

  volatile uint32_t *stack_pointer = (uint32_t *)__get_MSP();
  safe_uart_puts("DEBUG: 5. Stack pointer: ");
  safe_uart_putnum((uint32_t)stack_pointer);
  safe_uart_puts("\r\n");
  HAL_Delay(INIT_DELAY_SHORT_MS);
  Safe_Stack_Fill();
  safe_uart_puts("DEBUG: 6. Stack pattern complete\r\n");
  HAL_Delay(INIT_DELAY_SHORT_MS);
  __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
  __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
  safe_uart_puts("DEBUG: 7. Timer flags cleared\r\n");
  HAL_Delay(INIT_DELAY_SHORT_MS);
  safe_uart_puts("DEBUG: 8. System ready message\r\n");
  safe_uart_puts("\r\n=== SYSTEM READY ===\r\n");
  safe_uart_puts("BPM: ");
  safe_uart_put_ulu(current_bpm);
  safe_uart_puts("\r\n");
  HAL_Delay(INIT_DELAY_SHORT_MS);

  // Initialize custom modules
  Scoring_Init();
  Config_Init();
  ButtonHandler_Init();
  Sequence_Init();
  InitLEDControl();
  HAL_GPIO_WritePin(BEAT_LED_GPIO_Port, BEAT_LED_Pin, GPIO_PIN_SET);

  myprintf("\r\n=== Initialising ARGB ===");
  ARGB_Init();

  ARGB_SetBrightness(ARGB_BRIGHTNESS);
  myprintf("\r\n=== ARGB INITIALISED w/ Brightness = %d ===", ARGB_BRIGHTNESS);
  myprintf("\r\nARR=%lu PSC=%lu DMA_state=%d hdma_cc2=%p",
           (unsigned long)TIM2->ARR, (unsigned long)TIM2->PSC,
           (int)HAL_DMA_GetState(&hdma_tim2_ch2_ch4),
           (void *)htim2.hdma[TIM_DMA_ID_CC2]);
  ARGB_SetBrightness(1);

  myprintf("\r\n=== Initialising SD CARD ===");
  myprintf("\r\nretUSER = %d", retUSER);
  HAL_Delay(1000);

  myprintf("\r\n>>>> Sending 80 clocks ===");
  sd_send_80_clocks();
  extern DSTATUS USER_SPI_initialize(BYTE);
  DSTATUS s = USER_SPI_initialize(0);
  myprintf("\r\nUSER_SPI_initialize -> 0x%02X\r\n", s);

  ChangeSystemState(SYSTEM_STATE_INIT);

  // ETHAN PV
  int horizontalOffset = 0;
  int tsqCount = 0;
  char ***tsqFiles;
  // NOTE: the sd card isn't getting mounted before this function gets called.
  uint32_t lastScrollTime = 0;

// Start of Audio Definitions.
#define WAV_BUF_SIZE 512
  uint16_t wavBuf[WAV_BUF_SIZE * 2];

  // Because we access the relevant .wav file in multiple places. I got lazy and
  // made it a global.
  FIL wavFil;
  UINT bytesRead;
  FRESULT res;
  uint16_t spacing = 2;
  uint16_t h = Font_7x10.FontHeight;

  void playWav(char *filePath) {
    res = f_open(&wavFil, filePath, FA_READ);
    if (res != FR_OK) {
      return;
      myprintf("Failed to read file %s\n", filePath);
    }

    f_lseek(&wavFil, 44); // Skip WAV header
    f_read(&wavFil, &wavBuf, WAV_BUF_SIZE * 2, &bytesRead);
    // Start DMA for the full buffer (1024 bytes)
    myprintf("Beginning transmission of data for file\n");
    // No matter what I do here, this complains about the cast.
    HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint16_t *)wavBuf, WAV_BUF_SIZE);
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    switch (GetCurrentSystemState()) {
    case SYSTEM_STATE_INIT: {
      static uint8_t mount_attempts = 0;
      myprintf("\r\n=== SD CARD CHECK ===\r\n");

      // Try to mount
      fres = f_mount(&FatFs, "", 1);

      if (fres == FR_DISK_ERR || fres == FR_NOT_READY) {
        // No card detected
        mount_attempts++;

        if (mount_attempts == 1) {
          // First attempt failed - show error
          myprintf("\r\n");
          myprintf("╔════════════════════════════════════════════════╗\r\n");
          myprintf("║          NO SD CARD DETECTED                   ║\r\n");
          myprintf("║                                                ║\r\n");
          myprintf("║  Please insert a microSD card                  ║\r\n");
          myprintf("║  Device will retry every 2 seconds...          ║\r\n");
          myprintf("╚════════════════════════════════════════════════╝\r\n");
          myprintf("\r\n");
          char SD_mount_err_buff1[18];
          char SD_mount_err_buff2[18];
          char SD_mount_err_buff3[18];
          char SD_mount_err_buff4[18];

          SH1106_Fill(SH1106_COLOR_BLACK);
          snprintf(SD_mount_err_buff1, sizeof SD_mount_err_buff1,
                   "NO SD CARD FOUND:");
          SH1106_GotoXY(1, 1 * (h + spacing));
          SH1106_Puts(SD_mount_err_buff1, &Font_7x10, 1);
          snprintf(SD_mount_err_buff2, sizeof SD_mount_err_buff2,
                   "TURN OFF DEVICE");
          SH1106_GotoXY(1, 2 * (h + spacing));
          SH1106_Puts(SD_mount_err_buff2, &Font_7x10, 1);
          snprintf(SD_mount_err_buff3, sizeof SD_mount_err_buff3,
                   "AND REINSERT SD");
          SH1106_GotoXY(1, 3 * (h + spacing));
          SH1106_Puts(SD_mount_err_buff3, &Font_7x10, 1);
          snprintf(SD_mount_err_buff4, sizeof SD_mount_err_buff4,
                   "TO CONTINUE");
          SH1106_GotoXY(1, 4 * (h + spacing));
          SH1106_Puts(SD_mount_err_buff4, &Font_7x10, 1);
          SH1106_UpdateScreen();

        } else {
          // Still waiting
          myprintf("Retry %d: Still no card detected...\r\n", mount_attempts);
        }

        // Blink LED while waiting
        HAL_GPIO_TogglePin(RED_LED_GPIO_Port, RED_LED_Pin);

        // Wait 2 seconds and retry
        HAL_Delay(2000);
        break; // Stay in INIT state
      }

      if (fres == FR_NO_FILESYSTEM) {
        myprintf("Card detected but not formatted as FAT32.\r\n");
        char SD_mount_err_buff1[18];
        char SD_mount_err_buff2[18];
        char SD_mount_err_buff3[18];
        char SD_mount_err_buff4[18];
        SH1106_Fill(SH1106_COLOR_BLACK);
        snprintf(SD_mount_err_buff1, sizeof SD_mount_err_buff1,
                 "SD CARD DETECTED:");
        SH1106_GotoXY(1, 1 * (h + spacing));
        SH1106_Puts(SD_mount_err_buff1, &Font_7x10, 1);
        snprintf(SD_mount_err_buff2, sizeof SD_mount_err_buff2,
                 "SD NOT FORMATTED");
        SH1106_GotoXY(1, 2 * (h + spacing));
        SH1106_Puts(SD_mount_err_buff3, &Font_7x10, 1);
        snprintf(SD_mount_err_buff3, sizeof SD_mount_err_buff3,
                 "AS FAT32, PLEASE");
        SH1106_GotoXY(1, 3 * (h + spacing));
        SH1106_Puts(SD_mount_err_buff3, &Font_7x10, 1);
        snprintf(SD_mount_err_buff4, sizeof SD_mount_err_buff4,
                 "REINSERT/REFORMAT");
        SH1106_GotoXY(1, 4 * (h + spacing));
        SH1106_Puts(SD_mount_err_buff4, &Font_7x10, 1);
        SH1106_UpdateScreen();
        Error_Handler();
      }

      if (fres != FR_OK) {
        myprintf("Mount error: %d\r\n", fres); // display
        char SD_mount_err_buff1[18];
        char SD_mount_err_buff2[18];
        char SD_mount_err_buff3[18];
        char SD_mount_err_buff4[18];
        SH1106_Fill(SH1106_COLOR_BLACK);
        snprintf(SD_mount_err_buff1, sizeof SD_mount_err_buff1,
                 "SD CARD MOUNT ERR");
        SH1106_GotoXY(1, 1 * (h + spacing));
        SH1106_Puts(SD_mount_err_buff1, &Font_7x10, 1);
        snprintf(SD_mount_err_buff2, sizeof SD_mount_err_buff2,
                 "SD MOUNT FAILED");
        SH1106_GotoXY(1, 2 * (h + spacing));
        SH1106_Puts(SD_mount_err_buff3, &Font_7x10, 1);
        snprintf(SD_mount_err_buff3, sizeof SD_mount_err_buff3,
                 "TRY TURNING OFF");
        SH1106_GotoXY(1, 3 * (h + spacing));
        SH1106_Puts(SD_mount_err_buff3, &Font_7x10, 1);
        snprintf(SD_mount_err_buff4, sizeof SD_mount_err_buff4,
                 "AND REINSERTING");
        SH1106_GotoXY(1, 4 * (h + spacing));
        SH1106_Puts(SD_mount_err_buff4, &Font_7x10, 1);
        SH1106_UpdateScreen();
        Error_Handler();
        //   Error_Handler();
      }

      // Success!
      myprintf("✓ SD card detected and mounted (attempt %d)\r\n",
               mount_attempts + 1); // display
      mount_attempts = 0;           // Reset for next time
      tsqFiles = sd_list_tsq_files(&tsqCount);
      myprintf(
          "###------------------   There are %d files on the SD CARD \n\r ",
          tsqCount);

      // Add config option to the menu
      PrependConfigOption(&tsqFiles, &tsqCount);
      myprintf("Menu initialized with %d total options (including config)\n\r",
               tsqCount);

      //			// Turn on green LED to indicate success
      //			HAL_GPIO_WritePin(RED_LED_GPIO_Port,
      //RED_LED_Pin, GPIO_PIN_RESET); 			HAL_GPIO_WritePin(GREEN_LED_GPIO_Port,
      //GREEN_LED_Pin, GPIO_PIN_SET);

      ChangeSystemState(SYSTEM_STATE_SEQUENCE_SELECT);
      break;
    }

    case SYSTEM_STATE_WAITING_CONFIG_INPUT: {
      // --- OLED Message Buffers (18 chars max per line) ---
      char oled_buf1[19];
      char oled_buf2[19];
      char oled_buf3[19];
      char oled_buf4[19];
      char oled_buf5[19];

      uint16_t spacing = 2;
      uint16_t h = Font_7x10.FontHeight; // 10

      // Get current windows
      const ScoringWindows *w = Scoring_GetWindows();

      // --- Display current config on OLED ---
      SH1106_Fill(SH1106_COLOR_BLACK);

      snprintf(oled_buf1, sizeof(oled_buf1), "Current Windows:");
      SH1106_GotoXY(1, 0 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
      SH1106_Puts(oled_buf1, &Font_7x10, 1);

      // Show P and G on one line
      snprintf(oled_buf2, sizeof(oled_buf2), "P:%u G:%u", w->perfect_radius_ms,
               w->good_radius_ms);
      SH1106_GotoXY(1, 1 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
      SH1106_Puts(oled_buf2, &Font_7x10, 1);

      // Show O and Po on next line
      snprintf(oled_buf3, sizeof(oled_buf3), "O:%u Po:%u", w->ok_radius_ms,
               w->poor_radius_ms);
      SH1106_GotoXY(1, 2 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
      SH1106_Puts(oled_buf3, &Font_7x10, 1);

      snprintf(oled_buf4, sizeof(oled_buf4), "Send SETWIN via");
      SH1106_GotoXY(1, 3 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
      SH1106_Puts(oled_buf4, &Font_7x10, 1);

      snprintf(oled_buf5, sizeof(oled_buf5), "UART or press X");
      SH1106_GotoXY(1, 4 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
      SH1106_Puts(oled_buf5, &Font_7x10, 1);

      SH1106_UpdateScreen();

      // --- Wait for UART input and get result ---
      myprintf("\r\nCurrent: P=±%u G=±%u O=±%u Po=±%u ms\r\n",
               w->perfect_radius_ms, w->good_radius_ms, w->ok_radius_ms,
               w->poor_radius_ms);
      myprintf(
          "Enter new config (SETWIN:P,G,O,Po) or press Enter to skip:\r\n> ");

      bool config_changed = Config_WaitForInput(); // Capture return value

      // --- Check if config was saved to flash ---
      bool saved_to_flash =
          config_changed; // Config_WaitForInput saves internally

      // Log to UART
      const ScoringWindows *w_after = Scoring_GetWindows();
      uart_buf_len =
          sprintf(uart_buf, "Current Windows: P=±%u G=±%u O=±%u Po=±%u\r\n",
                  w_after->perfect_radius_ms, w_after->good_radius_ms,
                  w_after->ok_radius_ms, w_after->poor_radius_ms);
      HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);

      // --- Display result on OLED ---
      SH1106_Fill(SH1106_COLOR_BLACK);
      memset(oled_buf1, 0, sizeof(oled_buf1));
      memset(oled_buf2, 0, sizeof(oled_buf2));
      memset(oled_buf3, 0, sizeof(oled_buf3));
      memset(oled_buf4, 0, sizeof(oled_buf4));
      memset(oled_buf5, 0, sizeof(oled_buf5));

      if (config_changed) {
        snprintf(oled_buf1, sizeof(oled_buf1), "Config Updated!");
        SH1106_GotoXY(1,
                      0 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
        SH1106_Puts(oled_buf1, &Font_7x10, 1);

        if (saved_to_flash) {
          snprintf(oled_buf2, sizeof(oled_buf2), "Saved to Flash.");
          SH1106_GotoXY(1,
                        1 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
          SH1106_Puts(oled_buf2, &Font_7x10, 1);
        } else {
          snprintf(oled_buf2, sizeof(oled_buf2), "Save FAILED.");
          SH1106_GotoXY(1,
                        1 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
          SH1106_Puts(oled_buf2, &Font_7x10, 1);

          snprintf(oled_buf3, sizeof(oled_buf3), "Using for now.");
          SH1106_GotoXY(1,
                        2 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
          SH1106_Puts(oled_buf3, &Font_7x10, 1);
        }

        // Display new values - split across two lines
        snprintf(oled_buf4, sizeof(oled_buf4), "P:%u G:%u",
                 w_after->perfect_radius_ms, w_after->good_radius_ms);
        SH1106_GotoXY(1,
                      3 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
        SH1106_Puts(oled_buf4, &Font_7x10, 1);

        snprintf(oled_buf5, sizeof(oled_buf5), "O:%u Po:%u",
                 w_after->ok_radius_ms, w_after->poor_radius_ms);
        SH1106_GotoXY(1,
                      4 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
        SH1106_Puts(oled_buf5, &Font_7x10, 1);

      } else {
        snprintf(oled_buf1, sizeof(oled_buf1), "No Changes Made");
        SH1106_GotoXY(1,
                      0 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
        SH1106_Puts(oled_buf1, &Font_7x10, 1);

        snprintf(oled_buf2, sizeof(oled_buf2), "Using current");
        SH1106_GotoXY(1,
                      1 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
        SH1106_Puts(oled_buf2, &Font_7x10, 1);

        snprintf(oled_buf3, sizeof(oled_buf3), "settings:");
        SH1106_GotoXY(1,
                      2 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
        SH1106_Puts(oled_buf3, &Font_7x10, 1);

        // Show current values
        snprintf(oled_buf4, sizeof(oled_buf4), "P:%u G:%u",
                 w_after->perfect_radius_ms, w_after->good_radius_ms);
        SH1106_GotoXY(1,
                      3 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
        SH1106_Puts(oled_buf4, &Font_7x10, 1);

        snprintf(oled_buf5, sizeof(oled_buf5), "O:%u Po:%u",
                 w_after->ok_radius_ms, w_after->poor_radius_ms);
        SH1106_GotoXY(1,
                      4 * (h + spacing) + TEXT_OFFSET_Y + LINE_TEXT_Y_OFFSET);
        SH1106_Puts(oled_buf5, &Font_7x10, 1);
      }

      SH1106_UpdateScreen();

      // --- Wait and return ---
      HAL_Delay(3000);
      ChangeSystemState(SYSTEM_STATE_SEQUENCE_SELECT);

      break;
    }

    case SYSTEM_STATE_SEQUENCE_SELECT: {
      // Poll buttons for navigation
      poll_buttons();
      selected_song = get_choose_song();
      scrolled = get_scrolled();
      rightPressed = get_press_right();
      selectedNewSong = get_selected_song();

      // Ensure selected_song stays within bounds
      if (selected_song < 0)
        selected_song = 0;
      if (selected_song >= tsqCount)
        selected_song = tsqCount - 1;

      char uart_check_buf[16];
      uint32_t bytes_available =
          UART_ReadBytes((uint8_t *)uart_check_buf, sizeof(uart_check_buf) - 1);
      if (bytes_available > 0) {
        uart_check_buf[bytes_available] = '\0';

        // Check if GETWIN command was received
        if (strstr(uart_check_buf, "GETWIN") != NULL) {
          myprintf("GETWIN command received - sending config\r\n");
          ChangeSystemState(SYSTEM_STATE_SEND_CONFIG);
          break;
        }
      }

      // Check if SELECT button was pressed (pbSelPressed)
      if (pbSelPressed) {
        // Check if config option was selected (index 0)
        if (config_menu_enabled && selected_song == 0) {
          myprintf("Config option selected - entering config mode\n\r");
          pbSelPressed = false; // Clear the flag
          ChangeSystemState(SYSTEM_STATE_WAITING_CONFIG_INPUT);
          break;
        }

        // Otherwise, try to open the selected song file
        // Adjust index by -1 since config option is at index 0
        int file_index =
            config_menu_enabled ? (selected_song - 1) : selected_song;

        if (file_index >= 0 &&
            file_index < (tsqCount - (config_menu_enabled ? 1 : 0))) {
          fres = f_open(&fil, tsqFiles[2][selected_song], FA_READ);

          if (fres == FR_OK) {
            TSQParseResult parse_result =
                TSQ_LoadAndConfigure(&fil, &tsq_metadata);
            ChangeSystemState(SYSTEM_STATE_READY);
            pbSelPressed = false; // Clear the flag
            break;
          } else {
            uart_buf_len = sprintf(
                uart_buf, "ERROR: Failed to open file (error %d)\r\n", fres);
            HAL_UART_Transmit(&huart2, (uint8_t *)uart_buf, uart_buf_len, 1000);
            pbSelPressed = false; // Clear the flag
          }
        }
      }

      // Check if RIGHT button was pressed (alternative selection method)
      if (rightPressed) {
        // Check if config option was selected (index 0)
        if (config_menu_enabled && selected_song == 0) {
          myprintf(
              "Config option selected via RIGHT - entering config mode\n\r");
          rightPressed = false; // Clear the flag
          ChangeSystemState(SYSTEM_STATE_WAITING_CONFIG_INPUT);
          break;
        }

        // Otherwise, try to open the selected song file
        fres = f_open(&fil, tsqFiles[2][selected_song], FA_READ);

        if (fres == FR_OK) {
          TSQParseResult parse_result =
              TSQ_LoadAndConfigure(&fil, &tsq_metadata);
          ChangeSystemState(SYSTEM_STATE_READY);
          rightPressed = false; // Clear the flag
          break;
        }
      }

      // Update the screen if someone pressed up/down or initially
      if (scrolled || !rightPressed) {
        scrolled = false;
        horizontalOffset = 0;
        write_to_oled(tsqFiles, tsqCount, selected_song, &horizontalOffset);
        SH1106_UpdateScreen();
      }

      break;
    }

    case SYSTEM_STATE_READY: {
      HAL_Delay(100);
      uart_buf_len = sprintf(
          uart_buf, "\r\n=== INITIALIZATION COMPLETE ===\r\nBPM: %u\r\n",
          (unsigned int)current_bpm);
      UART_Transmit((uint8_t *)uart_buf, uart_buf_len);

      const ScoringWindows *windows = Scoring_GetWindows();

      uart_buf_len =
          sprintf(uart_buf,
                  "\r\n=== GAME READY ===\r\n"
                  "Scoring Windows:\r\n"
                  "  Perfect: ±%u ms\r\n"
                  "  Good:    ±%u ms\r\n"
                  "  OK:      ±%u ms\r\n"
                  "  Poor:    ±%u ms\r\n"
                  "BPM: %u\r\n"
                  "Sequence: %d\r\n"
                  "\r\n"
                  "Starting in 3 seconds...\r\n",
                  windows->perfect_radius_ms, windows->good_radius_ms,
                  windows->ok_radius_ms, windows->poor_radius_ms, current_bpm,
                  selected_sequence);
      UART_Transmit((uint8_t *)uart_buf, uart_buf_len);

      HAL_Delay(3000);

      // Reset and prepare all game modules
      Scoring_Reset();
      ButtonHandler_Init();
      Sequence_Reset();
      Sequence_SetHalfBeatMode(
          tsq_metadata.is_half_beat); // necessary for halfbeat skipping
      InitLEDControl();

      char *songName = tsqFiles[2][selected_song];
      songName[strlen(songName) - 4] = '\0';
      strcat(songName, ".WAV");
      myprintf("Playing %s\n,", songName);
      playWav(songName); // This needs to be dynamicly chosen

      safe_uart_puts("DEBUG: 11a. Initializing LED matrix display\r\n");
      LEDDisplay_Init();
      safe_uart_puts("DEBUG: 11b. Loading initial beat window\r\n");
      LEDDisplay_LoadInitialWindow();
      safe_uart_puts("DEBUG: 11c. LED matrix ready\r\n");

      beat_count = 0;
      PrimeFirstBeatLEDs(sequence_length);

      // Start all game timers
      safe_uart_puts("DEBUG: 11. Starting TIM7 counter\r\n");
      if (HAL_TIM_Base_Start_IT(&htim7) != HAL_OK) {
        safe_uart_puts("ERROR: TIM7 start failed\r\n");
      } else {
        safe_uart_puts("TIM7 started successfully\r\n");
      }

      safe_uart_puts("DEBUG: 10. Starting TIM6 interrupt\r\n");
      if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
        safe_uart_puts("ERROR: TIM6 start failed\r\n");
      } else {
        safe_uart_puts("TIM6 started successfully\r\n");
      }

      safe_uart_puts("DEBUG: 11. Starting TIM2 counter\r\n");
      if (HAL_TIM_Base_Start(&htim2) != HAL_OK) {
        safe_uart_puts("ERROR: TIM2 start failed\r\n");
      } else {
        safe_uart_puts("TIM2 started successfully\r\n");
      }

      safe_uart_puts("DEBUG: 12. Entering main loop\r\n");
      ChangeSystemState(SYSTEM_STATE_PLAYING);
      break;
    }

    case SYSTEM_STATE_PLAYING: {
      pbSelPressed = false;
      // Process Beat Events
      ProcessBeatEvent(tsq_metadata);

      // Process Scheduled LED Events
      ProcessLEDSchedule();

      if (beat_count % 2 != 0) {
        odd_count += 1;
      }

      if (fillWavBuffer == true) {
        f_read(&wavFil, &wavBuf, WAV_BUF_SIZE * 2, &bytesRead);
        fillWavBuffer = false;
      }

      // myprintf("odd_count = %d\r\n", odd_count);
      // if (beat_count == 1) {
      //    ProcessBeatLED();
      // } else if (tsq_metadata.is_half_beat && odd_count == 2) {
      //    ProcessBeatLED();
      //    odd_count = 0;
      // } else if (!tsq_metadata.is_half_beat) {
      //    ProcessBeatLED();
      // }
      // if (tsq_metadata.is_half_beat && odd_count == )
      // Process beat LED on/off
      if (tsq_metadata.is_half_beat && beat_count % 2 != 0) {
        ProcessBeatLED();
      } else if (!tsq_metadata.is_half_beat) {
        ProcessBeatLED();
      }
      // ProcessBeatLED();

      // Process Button Press Events from Queue
      ButtonHandler_ProcessQueue();

      // Check for UART Commands
      ProcessGameCommands();

      HAL_Delay(1);
      break;
    }

    case SYSTEM_STATE_COMPLETE: {
      myprintf("Game Over. Press 'R' to retry song, or 'M' to return to main "
               "menu.\r\n");
      HAL_SAI_DMAStop(&hsai_BlockA1);
      if (pbSelPressed) {
        pbSelPressed = false;
        ChangeSystemState(SYSTEM_STATE_INIT);
      }

      // uint8_t rx_byte;
      // while (UART_ReadBytes(&rx_byte, 1) == 0) {
      //     HAL_Delay(50);
      // }

      // if (rx_byte == 'r' || rx_byte == 'R') {
      //     myprintf("Restarting song...\r\n");
      //     ChangeSystemState(SYSTEM_STATE_READY);
      // } else if (rx_byte == 'm' || rx_byte == 'M') {
      //     myprintf("Returning to menu...\r\n");
      //     ChangeSystemState(SYSTEM_STATE_SEQUENCE_SELECT);
      // }
      break;
    }

    default: {
      myprintf("FATAL: Unknown system state! Resetting...\r\n");
      ChangeSystemState(SYSTEM_STATE_INIT);
      break;
    }

    } // end switch
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  } // end while(1)
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00F12981;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief SAI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SAI1_Init(void) {

  /* USER CODE BEGIN SAI1_Init 0 */

  /* USER CODE END SAI1_Init 0 */

  /* USER CODE BEGIN SAI1_Init 1 */

  /* USER CODE END SAI1_Init 1 */
  hsai_BlockA1.Instance = SAI1_Block_A;
  hsai_BlockA1.Init.AudioMode = SAI_MODEMASTER_TX;
  hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA1.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
  hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_44K;
  hsai_BlockA1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  if (HAL_SAI_InitProtocol(&hsai_BlockA1, SAI_I2S_STANDARD,
                           SAI_PROTOCOL_DATASIZE_16BIT, 2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SAI1_Init 2 */

  /* USER CODE END SAI1_Init 2 */
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 89;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);
}

/**
 * @brief TIM6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM6_Init(void) {

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 7999;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 5000;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */
}

/**
 * @brief TIM7 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM7_Init(void) {

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 799;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 65535;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
  /* DMA2_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel1_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SPI1_CS_Pin | BEAT_LED_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA,
                    GREEN_LED_Pin | RED_LED_Pin | BLUE_LED_Pin | YELLOW_LED_Pin,
                    GPIO_PIN_RESET);

  /*Configure GPIO pin : SD_CD_Pin */
  GPIO_InitStruct.Pin = SD_CD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SD_CD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB_U_Pin PB_D_Pin */
  GPIO_InitStruct.Pin = PB_U_Pin | PB_D_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : RED_PB_Pin GREEN_PB_Pin BLUE_PB_Pin YELLOW_PB_Pin */
  GPIO_InitStruct.Pin = RED_PB_Pin | GREEN_PB_Pin | BLUE_PB_Pin | YELLOW_PB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB_SEL_Pin */
  GPIO_InitStruct.Pin = PB_SEL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(PB_SEL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI1_CS_Pin BEAT_LED_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_Pin | BEAT_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : GREEN_LED_Pin RED_LED_Pin BLUE_LED_Pin YELLOW_LED_Pin
   */
  GPIO_InitStruct.Pin =
      GREEN_LED_Pin | RED_LED_Pin | BLUE_LED_Pin | YELLOW_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB_R_Pin PB_L_Pin */
  GPIO_InitStruct.Pin = PB_R_Pin | PB_L_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
