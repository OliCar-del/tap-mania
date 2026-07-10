/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void Update_BPM_Timer(uint16_t bpm);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SD_CD_Pin GPIO_PIN_13
#define SD_CD_GPIO_Port GPIOC
#define PB_U_Pin GPIO_PIN_0
#define PB_U_GPIO_Port GPIOA
#define PB_D_Pin GPIO_PIN_4
#define PB_D_GPIO_Port GPIOA
#define RED_PB_Pin GPIO_PIN_0
#define RED_PB_GPIO_Port GPIOB
#define RED_PB_EXTI_IRQn EXTI0_IRQn
#define GREEN_PB_Pin GPIO_PIN_1
#define GREEN_PB_GPIO_Port GPIOB
#define GREEN_PB_EXTI_IRQn EXTI1_IRQn
#define BLUE_PB_Pin GPIO_PIN_2
#define BLUE_PB_GPIO_Port GPIOB
#define BLUE_PB_EXTI_IRQn EXTI2_IRQn
#define PB_SEL_Pin GPIO_PIN_10
#define PB_SEL_GPIO_Port GPIOB
#define PB_SEL_EXTI_IRQn EXTI15_10_IRQn
#define SPI1_CS_Pin GPIO_PIN_11
#define SPI1_CS_GPIO_Port GPIOB
#define GREEN_LED_Pin GPIO_PIN_8
#define GREEN_LED_GPIO_Port GPIOA
#define RED_LED_Pin GPIO_PIN_9
#define RED_LED_GPIO_Port GPIOA
#define BLUE_LED_Pin GPIO_PIN_10
#define BLUE_LED_GPIO_Port GPIOA
#define YELLOW_LED_Pin GPIO_PIN_15
#define YELLOW_LED_GPIO_Port GPIOA
#define YELLOW_PB_Pin GPIO_PIN_4
#define YELLOW_PB_GPIO_Port GPIOB
#define YELLOW_PB_EXTI_IRQn EXTI4_IRQn
#define PB_R_Pin GPIO_PIN_5
#define PB_R_GPIO_Port GPIOB
#define PB_L_Pin GPIO_PIN_8
#define PB_L_GPIO_Port GPIOB
#define BEAT_LED_Pin GPIO_PIN_9
#define BEAT_LED_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define SD_SPI_HANDLE hspi1
#define RED_COLUMN 0
#define GREEN_COLUMN 16
#define BLUE_COLUMN 32
#define YELLOW_COLUMN 48
#define ROW_INDEX 30
#define SECONDS_TO_MS 1000
#define CONSTANT 255
#define MAX_FILES 10
#define MAX_NAME_LEN _MAX_LFN + 1
#define NUM_ROWS 16
#define CLR_LINE "0000"
#define ROW_STRIDE 1
#define WIN_PERF 200
#define WIN_GOOD 300
#define WIN_OK 400
#define WIN_POOR 500 // 0 points inside this window; Miss if > 500
#define MAX_NOTES 64
#define LANES 2 // you have 2 buttons; adjust if more
typedef enum { ST_WAIT_TIME,
    ST_READ_LINE,
    ST_RENDER,
    ST_DONE } State;

typedef struct {
    uint32_t t_hit; // time (ms) when the note reaches the judgement line
    uint8_t lane; // 0..LANES-1
    uint8_t used; // 0 = pending, 1 = already judged (hit or missed)
} NoteEvent;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
