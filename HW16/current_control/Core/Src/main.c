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
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  STATE_IDLE,
  STATE_ITEST
} ControlState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_MIN 350
#define ADC_MAX 4050

#define INA219_ADDR            0x40
#define INA219_REG_CONFIG      0x00
#define INA219_REG_CURRENT     0x04
#define INA219_REG_CALIBRATION 0x05

#define LOG_SIZE 400

// control gains
#define KP_CURRENT      -0.7f
#define KI_CURRENT      0.5f
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile int desired_current = 300;
volatile float error_integral = 0;
volatile int log_i = 0;

// enum type
volatile ControlState control_state = STATE_IDLE;

// storage logs
volatile int log_index[LOG_SIZE];
volatile int log_desired[LOG_SIZE];
volatile int log_actual[LOG_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
uint32_t readADC(void);
void init_ina219(void);
void writeINA219(int reg, int value);
signed short readINA219(unsigned char reg);
float read_ina219(void);
void motor_off(void);
void motor_forward(int pwm);
void motor_reverse(int pwm);
void setPWM(int8_t duty_cycle);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch) {
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
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

BSP_LED_Init(LED_GREEN);
BSP_LED_Init(LED_BLUE);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C2_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim2);
  init_ina219();

  // set both channels high = motor off
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2400);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 2400);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  /* USER CODE END 2 */

    /* Initialize leds */
  // BSP_LED_Init(LED_GREEN);
  // BSP_LED_Init(LED_BLUE);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* USER CODE BEGIN WHILE */
  // uint8_t rx_byte = 0;
  
  // while (1)
  // {
  //   // check if a character has been received via UART (Non-blocking check)
  //   if (HAL_UART_Receive(&huart2, &rx_byte, 1, 10) == HAL_OK) 
  //   {
  //     if (rx_byte == 'a' && state == 0) 
  //     {
  //       // reset logging index and restore initial test condition
  //       printf("Starting experiment...\r\n"); // debug
  //       log_i = 0; 
  //       desired_current = 300; 
  //       integral = 0;
        
  //       // fire off the PI execution window in the 1kHz ISR
  //       state = 1;
        
  //       // wait block until the 400 sample cycle finishes and resets state to 0
  //       printf("Starting sample cycle...\r\n");
  //       while(state == 1) 
  //       {
  //         BSP_LED_Toggle(LED_BLUE);
  //         HAL_Delay(10);
  //       }
        
  //       printf("printing data...\r\n");
  //       // dump gathered logs out of RAM via Serial using integer formats
  //       for (int i = 0; i < LOG_SIZE; i++) 
  //       {
  //         printf("%d %d %d\r\n", log_index[i], log_desired[i], log_actual[i]);
  //       }
  //     }
  //   }

  //   // Standard heartbeat indicator
  //   BSP_LED_Toggle(LED_GREEN);
  //   HAL_Delay(200);

  //   // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 100);
  //   // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 2400);
  //   // HAL_Delay(500);

  //   // // spin forward at 50% for 500ms
  //   // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2400);
  //   // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 1200);
  //   // HAL_Delay(500);

  //   // // motor off (both high)
  //   // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2400);
  //   // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 2400);
  //   // HAL_Delay(200);

  //   // // spin backward at 50% for 500ms
  //   // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1200);
  //   // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 2400);
  //   // HAL_Delay(500);

  //   // // motor off (both high)
  //   // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2400);
  //   // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 2400);
  //   // HAL_Delay(200);
    
  uint8_t rx_byte = 0;
  
  while (1)
  {
    // check for incoming UART characters
    if (HAL_UART_Receive(&huart2, &rx_byte, 1, 10) == HAL_OK) 
    {
      // trigger execution only if system is IDLE
      if (rx_byte == 'a' && control_state == STATE_IDLE) 
      {
        log_i = 0; 
        desired_current = 300; 
        error_integral = 0;
        
        printf("Starting test...\r\n");
        
        // fire off the PI loop in the ISR
        control_state = STATE_ITEST;
      }
    }

    // Gated Data Dump: Triggers exactly once when the ISR switches control_state back to IDLE
    if (control_state == STATE_IDLE && log_i == LOG_SIZE)
    {
        printf("Experiment complete. Printing data arrays:\r\n");
        HAL_Delay(10); // Give the UART register room to clear
        
        for (int i = 0; i < LOG_SIZE; i++) 
        {
          printf("%d %d %d\r\n", log_index[i], log_desired[i], log_actual[i]);
          
          // CRITICAL: Prevent serial buffers from drowning
          HAL_Delay(2); 
        }
        
        printf("--- End of Transmission ---\r\n");
        log_i = 0; // Reset index so it doesn't print again continuously
    }

    // Standard background heartbeat indicator
    BSP_LED_Toggle(LED_GREEN);
    HAL_Delay(200);
    
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

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_SEQ_FIXED;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_1CYCLE_5;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x10805D88;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 2400;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 48000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

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
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
uint32_t readADC() {
    uint32_t raw;
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
      raw = HAL_ADC_GetValue(&hadc1);
    } else {
      raw = 0;
    }
    HAL_ADC_Stop(&hadc1);
    return raw;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == &htim2)
  {
    static int counter = 0;

    // state check
    if (control_state == STATE_ITEST)
    {
      counter++;

      // // safety check
      // uint32_t adc = readADC();
      // if(adc < ADC_MIN || adc > ADC_MAX)
      // {
      //   setPWM(0); // shut off motor
      //   control_state = STATE_IDLE;
      //   counter = 0;
      //   error_integral = 0;
      //   return;
      // }

      // read sensor
      // signed short current = readINA219(INA219_REG_CURRENT);
      signed short current = 120; // Fake 120mA sensor reading

      // PI control loop math
      float error = (float)desired_current - (float)current;
      error_integral += error;

      // anti-windup clamp (scaled for floating point current values)
      if(error_integral > 200.0f)  error_integral = 200.0f;
      if(error_integral < -200.0f) error_integral = -200.0f;

      // calculate percentage output (-100 to 100)
      float u = (KP_CURRENT * error) + (KI_CURRENT * error_integral);

      // pass directly to driver helper
      setPWM((int8_t)u);

      // data logging
      if(log_i < LOG_SIZE)
      {
        log_index[log_i]   = log_i;
        log_desired[log_i] = desired_current;
        log_actual[log_i]  = current;
        log_i++;
      }

      // alternating square-wave setpoint adjustments every 100ms
      if(counter % 100 == 0)
        desired_current = -desired_current;

      // test window finished (400 samples = 400ms)
      if(counter >= 400)
      {
        setPWM(0); // turn off motor
        control_state = STATE_IDLE; // signal main loop that data is ready
        counter = 0;
        error_integral = 0;
      }
    }
  }
}

// INA219 helper functions
void writeINA219(int reg, int value) {
  uint8_t buf[3];
  buf[0] = reg;
  buf[1] = value >> 8;
  buf[2] = value & 0xff;
  HAL_I2C_Master_Transmit(&hi2c2, INA219_ADDR << 1, buf, 3, 10);
}

signed short readINA219(unsigned char reg) {
  HAL_I2C_Master_Transmit(&hi2c2, INA219_ADDR << 1, &reg, 1, 10);
  uint8_t buffer[2];
  HAL_I2C_Master_Receive(&hi2c2, INA219_ADDR << 1, buffer, 2, 10);
  return (signed short)((buffer[0] << 8) | buffer[1]);
}

void init_ina219() {
  unsigned short ina219_calValue = 1024;
  unsigned short ina219_config = 0b0011000010001111;
  writeINA219(INA219_REG_CALIBRATION, ina219_calValue);
  writeINA219(INA219_REG_CONFIG, ina219_config);
}

float read_ina219() {
  signed short value = readINA219(INA219_REG_CURRENT);
  return value / 3.0f;
}
void setPWM(int8_t duty_cycle);
// pwm function
void setPWM(int8_t duty_cycle)
{
    if (duty_cycle > 100)  duty_cycle = 100;
    if (duty_cycle < -100) duty_cycle = -100;

    // calculate pull-down depth based on 2400 timer period
    uint32_t pull_down_depth = (uint32_t)((abs(duty_cycle) * 2400) / 100);
    uint32_t active_compare  = 2400 - pull_down_depth;

    if (duty_cycle > 0)
    {
        // forward
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2400);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, active_compare);
    }
    else if (duty_cycle < 0)
    {
        // reverse
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, active_compare);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 2400);
    }
    else
    {
        // off (Brake) - both channels held high
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2400);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 2400);
    }
}
/* USER CODE END 4 */

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
