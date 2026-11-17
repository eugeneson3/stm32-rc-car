/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Tunable distances, in cm. Side sensors point diagonally outward. */
#define FRONT_BLOCK_CM 60U
#define FRONT_CLEAR_CM 85U
#define SIDE_BLOCK_CM 30U
#define SIDE_CLEAR_CM 40U
#define DIRECTION_MARGIN_CM 10U
#define SENSOR_SLOT_MS 60U
#define SENSOR_STALE_MS 300U
#define ECHO_TIMEOUT_US 25000U
#define BUTTON_DEBOUNCE_MS 30U

typedef struct {
    GPIO_TypeDef *trig_port;
    uint16_t trig_pin, echo_pin;
    uint16_t cm;
    uint8_t valid;
    uint32_t measured_ms;
} sensor_t;
static sensor_t sensors[3] = {
    {GPIOA, GPIO_PIN_8, GPIO_PIN_10, 0, 0, 0},
    {GPIOB, GPIO_PIN_4, GPIO_PIN_5, 0, 0, 0},
    {GPIOA, GPIO_PIN_10, GPIO_PIN_3, 0, 0, 0}
};
/* All ECHO pins belong to GPIOB. Inspect these in Live Expressions. */
static volatile uint8_t running;
static volatile int8_t motion; /* 0 stop, 1 forward, -1 left, 2 right */
static uint8_t sensor_index, phase;
static uint32_t phase_cycles, slot_ms;
static uint8_t button_raw = 1, button_stable = 1;
static uint32_t button_changed_ms;
static int8_t preferred_turn = -1;

static uint32_t elapsed_us(uint32_t start)
{
    return (uint32_t)(DWT->CYCCNT - start) / (SystemCoreClock / 1000000U);
}

static void drive(int8_t next)
{
    if (motion == next) return;
    /* Remove enable before changing bridge direction. */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);
    if (next) {
        uint8_t left_forward = next != -1;
        uint8_t right_forward = next != 2;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, left_forward ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, left_forward ? GPIO_PIN_RESET : GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, right_forward ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, right_forward ? GPIO_PIN_RESET : GPIO_PIN_SET);
        /* ARR=999: CCR=1000 gives true 100 percent duty. */
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 1000);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 1000);
    }
    motion = next;
}

static void navigate(uint32_t now)
{
    if (!running) { drive(0); return; }
    for (unsigned i=0; i<3; ++i) {
        if (!sensors[i].valid || (uint32_t)(now-sensors[i].measured_ms)>SENSOR_STALE_MS) {
            drive(0); return;
        }
    }
    uint16_t left=sensors[0].cm, front=sensors[1].cm, right=sensors[2].cm;
    uint8_t turning = motion == -1 || motion == 2;
    if (front >= (turning ? FRONT_CLEAR_CM : FRONT_BLOCK_CM) &&
        left >= (turning ? SIDE_CLEAR_CM : SIDE_BLOCK_CM) &&
        right >= (turning ? SIDE_CLEAR_CM : SIDE_BLOCK_CM)) {
        drive(1); return;
    }
    /* Keep the previous direction on a near tie to avoid oscillation. */
    if (left > right + DIRECTION_MARGIN_CM) preferred_turn=-1;
    else if (right > left + DIRECTION_MARGIN_CM) preferred_turn=2;
    drive(preferred_turn);
}

static void button_update(uint32_t now)
{
    uint8_t raw = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0) == GPIO_PIN_SET;
    if (raw != button_raw) { button_raw=raw; button_changed_ms=now; }
    if (raw != button_stable && (uint32_t)(now-button_changed_ms)>=BUTTON_DEBOUNCE_MS) {
        button_stable=raw;
        if (!raw) {
            running=!running;
            drive(0);
            /* Require a fresh complete scan before starting. */
            for (unsigned i=0; i<3; ++i) sensors[i].valid=0;
        }
    }
}

static void finish_measurement(uint8_t valid, uint32_t width_us, uint32_t now)
{
    sensor_t *s=&sensors[sensor_index];
    s->cm=(uint16_t)((width_us+29U)/58U);
    s->valid=valid && width_us>=116U && width_us<=23200U;
    s->measured_ms=now;
    sensor_index=(sensor_index+1U)%3U;
    phase=0;
    navigate(now);
}

static void sensors_update(uint32_t now)
{
    sensor_t *s=&sensors[sensor_index];
    switch (phase) {
    case 0:
        if ((uint32_t)(now-slot_ms)<SENSOR_SLOT_MS) return;
        slot_ms=now;
        if (HAL_GPIO_ReadPin(GPIOB,s->echo_pin)==GPIO_PIN_SET) {
            finish_measurement(0,0,now); return;
        }
        HAL_GPIO_WritePin(s->trig_port,s->trig_pin,GPIO_PIN_SET);
        phase_cycles=DWT->CYCCNT; phase=1;
        break;
    case 1:
        if (elapsed_us(phase_cycles)>=12U) {
            HAL_GPIO_WritePin(s->trig_port,s->trig_pin,GPIO_PIN_RESET);
            phase_cycles=DWT->CYCCNT; phase=2;
        }
        break;
    case 2:
        if (elapsed_us(phase_cycles)>=ECHO_TIMEOUT_US) finish_measurement(0,0,now);
        else if (HAL_GPIO_ReadPin(GPIOB,s->echo_pin)==GPIO_PIN_SET) {
            phase_cycles=DWT->CYCCNT; phase=3;
        }
        break;
    case 3:
        if (elapsed_us(phase_cycles)>=ECHO_TIMEOUT_US) finish_measurement(0,0,now);
        else if (HAL_GPIO_ReadPin(GPIOB,s->echo_pin)==GPIO_PIN_RESET)
            finish_measurement(1,elapsed_us(phase_cycles),now);
        break;
    default: phase=0; break;
    }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_TIM3_Init();
	/* USER CODE BEGIN 2 */

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT=0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    if (HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_1)!=HAL_OK ||
        HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_3)!=HAL_OK) Error_Handler();
    motion=99;
    drive(0);
    slot_ms=HAL_GetTick()-SENSOR_SLOT_MS;

/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
    while (1) {
        uint32_t now=HAL_GetTick();
        button_update(now);
        sensors_update(now);
        navigate(now);
        /* USER CODE END WHILE */
        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 100;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void)
{

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 100-1;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 1000-1;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */
	HAL_TIM_MspPostInit(&htim3);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

	/*Configure GPIO pins : PC0 PC1 PC2 */
	GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : PA5 PA6 PA7 */
	GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : PB6 */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8|GPIO_PIN_10,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_4,GPIO_PIN_RESET);
    GPIO_InitStruct.Pin=GPIO_PIN_8|GPIO_PIN_10;
    GPIO_InitStruct.Mode=GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull=GPIO_NOPULL;
    GPIO_InitStruct.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA,&GPIO_InitStruct);
    GPIO_InitStruct.Pin=GPIO_PIN_4;
    HAL_GPIO_Init(GPIOB,&GPIO_InitStruct);
    GPIO_InitStruct.Pin=GPIO_PIN_10|GPIO_PIN_5|GPIO_PIN_3;
    GPIO_InitStruct.Mode=GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull=GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOB,&GPIO_InitStruct);
    GPIO_InitStruct.Pin=GPIO_PIN_0;
    GPIO_InitStruct.Pull=GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC,&GPIO_InitStruct);

/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM10 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	/* USER CODE BEGIN Callback 0 */

	/* USER CODE END Callback 0 */
	if (htim->Instance == TIM10)
	{
		HAL_IncTick();
	}
	/* USER CODE BEGIN Callback 1 */

	/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
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
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
