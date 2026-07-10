/**
 ******************************************************************************
 * @file           : uart_debug.c
 * @brief          : UART Debug and Communication Implementation
 ******************************************************************************
 */

#include "uart_debug.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
extern uint8_t uart_tx_buffer[UART_TX_BUFFER_SIZE];
extern volatile uint32_t uart_rx_head;
extern uint32_t uart_rx_tail;
extern volatile uint8_t uart_tx_busy;

/**
 * @brief Custom printf for UART output
 */
void myprintf(const char *fmt, ...) {
  static char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  int len = strlen(buffer);
  HAL_UART_Transmit(&huart2, (uint8_t *)buffer, len, -1);
}

/**
 * @brief Safe UART string output
 */
void safe_uart_puts(const char *str) {
  int len = strlen(str);
  UART_Transmit((uint8_t *)str, len);
}

/**
 * @brief Safe UART unsigned long output
 */
void safe_uart_put_ulu(uint32_t num) {
  char buf[11]; // Max 10 digits for a 32-bit unsigned + null
  char *p = buf + sizeof(buf) - 1;
  *p = '\0';

  if (num == 0) {
    *--p = '0';
  } else {
    while (num > 0) {
      *--p = (num % 10) + '0';
      num /= 10;
    }
  }
  safe_uart_puts(p);
}

/**
 * @brief Safe UART signed long output
 */
void safe_uart_put_sli(int32_t num) {
  if (num < 0) {
    safe_uart_puts("-");
    num = -num;
  }
  safe_uart_put_ulu(num);
}

/**
 * @brief Legacy safe UART number output
 */
void safe_uart_putnum(uint32_t num) {
  char buf[12];
  int i = 11;
  buf[i--] = '\0';

  if (num == 0) {
    buf[i--] = '0';
  } else {
    while (num > 0 && i >= 0) {
      buf[i--] = '0' + (num % 10);
      num /= 10;
    }
  }
  safe_uart_puts(&buf[i + 1]);
}

/**
 * @brief Check what caused the system reset
 */
void Check_Reset_Flags(void) {
  safe_uart_puts("Reset Flags: ");

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
    safe_uart_puts("IWDG ");
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))
    safe_uart_puts("WWDG ");
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST))
    safe_uart_puts("LPWR ");
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))
    safe_uart_puts("PIN ");
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST))
    safe_uart_puts("BOR ");
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))
    safe_uart_puts("SOFT ");

  safe_uart_puts("\r\n");
  __HAL_RCC_CLEAR_RESET_FLAGS();
}

/**
 * @brief Fill stack with pattern for debugging
 */
void Safe_Stack_Fill(void) {
  safe_uart_puts("DEBUG: Starting stack fill\r\n");

  extern uint32_t _estack;
  extern uint32_t _Min_Stack_Size;
  uint32_t *stack_start = &_estack - (_Min_Stack_Size / 4);
  uint32_t *current_sp = (uint32_t *)__get_MSP();

  uint32_t fill_count = 0;
  for (uint32_t *p = stack_start; p < current_sp - 64; p++) {
    *p = 0xDEADBEEF;
    fill_count++;
    if ((fill_count % 16) == 0) {
      for (volatile int i = 0; i < 1000; i++)
        ;
    }
    if ((fill_count % 64) == 0) {
      safe_uart_puts(".");
    }
  }
  safe_uart_puts(" Stack pattern filled\r\n");
}

/**
 * @brief Start DMA reception in circular mode
 */
void UART_StartDMAReception(void) {
  HAL_UART_Receive_DMA(&huart2, uart_rx_buffer, UART_RX_BUFFER_SIZE);
  uart_rx_head = UART_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
}

/**
 * @brief Check if new data available in RX buffer
 */
uint32_t UART_GetRxCount(void) {
  uint32_t current_head =
      UART_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);

  if (current_head >= uart_rx_tail) {
    return current_head - uart_rx_tail;
  } else {
    return UART_RX_BUFFER_SIZE - uart_rx_tail + current_head;
  }
}

/**
 * @brief Read bytes from circular buffer
 */
uint32_t UART_ReadBytes(uint8_t *dest, uint32_t max_len) {
  uint32_t available = UART_GetRxCount();
  uint32_t to_read = (available < max_len) ? available : max_len;

  for (uint32_t i = 0; i < to_read; i++) {
    dest[i] = uart_rx_buffer[uart_rx_tail];
    uart_rx_tail = (uart_rx_tail + 1) % UART_RX_BUFFER_SIZE;
  }

  return to_read;
}

/**
 * @brief Transmit data using DMA
 */
HAL_StatusTypeDef UART_TransmitDMA(uint8_t *data, uint16_t len) {
  uint32_t timeout = HAL_GetTick() + 1000;
  while (uart_tx_busy && HAL_GetTick() < timeout) {
    // Wait for previous transmission
  }

  if (uart_tx_busy) {
    return HAL_BUSY;
  }

  if (len > UART_TX_BUFFER_SIZE) {
    len = UART_TX_BUFFER_SIZE;
  }
  memcpy(uart_tx_buffer, data, len);

  uart_tx_busy = 1;
  return HAL_UART_Transmit_DMA(&huart2, uart_tx_buffer, len);
}

/**
 * @brief Transmit data using blocking UART
 */
HAL_StatusTypeDef UART_Transmit(uint8_t *data, uint16_t len) {
  uint32_t timeout = 100 + (len * 10000) / 115200;
  return HAL_UART_Transmit(&huart2, data, len, timeout);
}
