/**
 ******************************************************************************
 * @file           : uart_debug.h
 * @brief          : UART Debug and Communication Utilities
 ******************************************************************************
 */

#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include "main.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

// UART Buffer sizes
#define UART_RX_BUFFER_SIZE 256
#define UART_TX_BUFFER_SIZE 1024

/**
 * @brief Custom printf for UART output
 * @param fmt Format string
 * @param ... Variable arguments
 */
void myprintf(const char* fmt, ...);

/**
 * @brief Safe UART string output (non-DMA)
 * @param str String to transmit
 */
void safe_uart_puts(const char* str);

/**
 * @brief Safe UART unsigned long output
 * @param num Number to transmit
 */
void safe_uart_put_ulu(uint32_t num);

/**
 * @brief Safe UART signed long output
 * @param num Number to transmit
 */
void safe_uart_put_sli(int32_t num);

/**
 * @brief Legacy safe UART number output
 * @param num Number to transmit
 */
void safe_uart_putnum(uint32_t num);

/**
 * @brief Check and display reset flags
 */
void Check_Reset_Flags(void);

/**
 * @brief Fill stack with pattern for debugging
 */
void Safe_Stack_Fill(void);

/**
 * @brief Start UART DMA reception
 */
void UART_StartDMAReception(void);

/**
 * @brief Get count of available RX bytes
 * @return Number of bytes available
 */
uint32_t UART_GetRxCount(void);

/**
 * @brief Read bytes from UART RX buffer
 * @param dest Destination buffer
 * @param max_len Maximum bytes to read
 * @return Number of bytes actually read
 */
uint32_t UART_ReadBytes(uint8_t* dest, uint32_t max_len);

/**
 * @brief Transmit data using DMA
 * @param data Data buffer
 * @param len Data length
 * @return HAL status
 */
HAL_StatusTypeDef UART_TransmitDMA(uint8_t* data, uint16_t len);

/**
 * @brief Transmit data using blocking UART
 * @param data Data buffer
 * @param len Data length
 * @return HAL status
 */
HAL_StatusTypeDef UART_Transmit(uint8_t* data, uint16_t len);

#endif /* UART_DEBUG_H */
