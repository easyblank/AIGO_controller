/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdbool.h"
#include "string.h"
#include "math.h"
#include "mpu6050.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
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

/* USER CODE BEGIN PV */
uint8_t rx_data[] = {0};
uint32_t last = 0;
char data[1024] = {0};
uint8_t len = 0;
MPU6050_t MPU6050;
int16_t imu[6] = {0}; //AcX, AcY, AcZ, GyX, GyY, GyZ


/***********lidar************/
//Response data
bool scan_start = false;
uint8_t rx3_start[7] = {0};
uint8_t rx3_data[5] = {0};
uint8_t Q = 0;
bool S = false;
uint16_t angle;
uint16_t d;
uint16_t distance[360] = {0};
bool lidar_ok = false;

//Protocol
uint8_t scan_command[2] = {0xA5,0x20};
uint8_t stop_command[2] = {0xA5,0x25};
uint8_t soft_reboot[2] = {0xA5,0x40};
uint8_t scan_express[2] = {0xA5,0x82};
uint8_t scan_force[2] = {0xA5,0x21};
uint8_t device_info[2] = {0xA5,0x50};
uint8_t health_status[2] = {0xA5,0x52};
uint8_t sample_rate[2] = {0xA5,0x59};
uint8_t scan_response[7] = {0xa5, 0x5a, 0x5, 0x0, 0x0, 0x40, 0x81};

//Encoder Count per Duration(0.1 second)
uint16_t CntL = 0;
uint16_t CntR = 0;

//Desired Encoder Rate for PID control (Received From Raspberry Pi) : Left, Right
uint32_t RecL = 0;
uint32_t RecR = 0;

//Values for PID control
uint32_t desired_speed_L = 0;
uint32_t desired_speed_R = 0;
uint32_t encoder_speed_L = 0;
uint32_t encoder_speed_R = 0;
uint32_t MOTOR_PWM[4] = {0};
uint32_t Kp = 1;
uint32_t encoder_cnt[4] = {0};
int32_t error_speed[4] = {0};
int32_t PID_speed[4] = {0};
uint32_t old_PID_speed[4] = {0, 0, 0, 0};


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Initialize_Encoder_Count();
void Receive_Encoder_Count();
void Receive_Lidar();
void Receive_Imu();
void Receive_Serial();
void Transmit_Data();
void Set_Motor_PID();
uint32_t Calculate_Value(uint32_t);
void Set_Motor_PWM();
bool array_element_of_index_equal(uint8_t a[], uint8_t b[], uint8_t size) {
   uint8_t i;
   for(i=0; i<size; i++){
      if( a[i] != b[i] )
         return false;
   }
   return true;
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_DMA_Init();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM5_Init();
  MX_TIM6_Init();
  MX_USART6_UART_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  //while (MPU6050_Init(&hi2c1) ==1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

  HAL_Delay(1000);
  Initialize_Encoder_Count();
  HAL_UART_Receive_DMA(&huart6, rx_data, 10);
  HAL_UART_Transmit_DMA(&huart3, &scan_command, 2);
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);

  //Initialize for motor PWM

  //12,13 : LF | 14,15 : RF | 8,9 : RB | 10,11 : LB
  //write pin SET at lower pin to go forward
  //initialize all wheels directions forward
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, SET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, RESET);

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, SET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, RESET);

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, RESET);

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, RESET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  //Receive_Lidar();
	  //Receive_Imu();
	  //HAL_Delay(10);
	  if(HAL_GetTick()-last > 100L){
		  last = HAL_GetTick();
		  Receive_Encoder_Count();
		  Transmit_Data();
		  Receive_Serial();
		  Set_Motor_PID();
		  Set_Motor_PWM();
		  Initialize_Encoder_Count();
	  }
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
		 //TIM1->CCR1 = 0;
		 //TIM1->CCR2 = 0;
		 //TIM1->CCR3 = 0;
		 //TIM1->CCR4 = 0;
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
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void Initialize_Encoder_Count(){
	TIM2 -> CNT = 0;
	TIM3 -> CNT = 0;
	TIM4 -> CNT = 0;
	TIM5 -> CNT = 0;
}
void Receive_Encoder_Count(){
	//TIM2 : LF, TIM3 : RF, TIM4 : LB, TIM5 : RB
	  //CntR = (TIM3 -> CNT >> 3) + (TIM4 -> CNT >> 3);
	  //CntL = (TIM2 -> CNT >> 3) + (TIM5 -> CNT >> 3);
		CntR = TIM4 -> CNT >> 2;
		CntL = TIM5 -> CNT >> 2;
	  //sprintf(data, "e%u,%u\n\r", CntL, CntR);
}
void Receive_Imu(){
	MPU6050_Read_All(&hi2c1, &MPU6050);
	imu[1] = MPU6050.Accel_X_RAW;
	imu[2]= MPU6050.Accel_Y_RAW;
	imu[3] = MPU6050.Accel_Z_RAW;
	//int16_t Tmp = MPU6050.Temperature;
	imu[4] = MPU6050.Gyro_X_RAW;
	imu[5] = MPU6050.Gyro_Y_RAW;
	imu[6] = MPU6050.Gyro_Z_RAW;

}
void Receive_Lidar(){
	if(scan_start){
		HAL_UART_Receive_DMA(&huart3, rx3_data, 5);
		Q = rx3_data[0]>>2;
		 S = (rx3_data[0] & 0x01) ? 1 : 0;
		 angle = (rx3_data[2]<<7 | rx3_data[1]>>1)/64;
		 d = (rx3_data[4]<<8 | rx3_data[3])/4; //distance(mm);
		 if(d >= 12000){
			distance[angle] = 12000;
		 }
		 else{
			distance[angle] = d;
		 }
	  }
	else{
	HAL_UART_Transmit_DMA(&huart3, scan_command, 2);
	HAL_UART_Receive(&huart3, rx3_start, 7, 100);
		 if (array_element_of_index_equal(rx3_start, scan_response, 7)){
			scan_start = true;
		 }
	  }
}
void Transmit_Data(){
	//IMU
	/**
	sprintf(data,"i%d,%d,%d,%d,%d,%d\n\r", imu[1], imu[2], imu[3], imu[4], imu[5], imu[6]);
	HAL_UART_Transmit_DMA(&huart6, (uint8_t*)data, strlen(data));
	**/
	/**
	//LiDAR
	sprintf(data, "l");
	HAL_UART_Transmit_DMA(&huart6, (uint8_t*)data, strlen(data));
	uint16_t i = 0;
	while(i<359){
		sprintf(data, "%d,", distance[i]);
		i++;
		HAL_UART_Transmit_DMA(&huart6, (uint8_t*)data, strlen(data));
	}
	sprintf(data, "%d\n\r", distance[359]);
	HAL_UART_Transmit_DMA(&huart6, (uint8_t*)data, strlen(data));
	**/
	//Encoder
	sprintf(data, "e%u,%u\n\r", CntL, CntR);
	HAL_UART_Transmit_DMA(&huart6, (uint8_t*)data, strlen(data));
}


void Receive_Serial(){
	//Receive two integer data (Desired Encoder Rate for two wheels) from serial (Raspberry Pi)
	//split string data, then convert to integer
	HAL_UART_Receive_DMA(&huart6, (uint8_t*)data, strlen(data));
	uint8_t i = 0;
	char *p = strtok(data, ",");
	char *array[2];
	while(p !=NULL){
		array[i++] = p;
		p = strtok(NULL, ",");
	}
	RecL = atoi(array[0]);
	RecR = atoi(array[1]);
}

//named PID, but the example code implemented only P control i think T.T
void Set_Motor_PID(){
	//0 : LF | 1 : RF | 2 : LB | 3 : RB
	uint8_t index = 0;
	if(RecL !=0)
		desired_speed_L = Calculate_Value(RecL);
	//Determine Desired motor PWM value

	if(RecR !=0)
		desired_speed_R = Calculate_Value(RecR);

	//Determine Current motor PWM value
	encoder_speed_L = Calculate_Value(CntL);
	encoder_speed_R = Calculate_Value(CntR);

	//Do P Control, NOT pid control

	error_speed[0] = desired_speed_L - encoder_speed_L;
	error_speed[1] = desired_speed_R - encoder_speed_R;
	error_speed[2] = desired_speed_L - encoder_speed_L;
	error_speed[3] = desired_speed_R - encoder_speed_R;

	PID_speed[0] = old_PID_speed[0] + Kp*error_speed[0];
	PID_speed[1] = old_PID_speed[1] + Kp*error_speed[1];
	PID_speed[2] = old_PID_speed[2] + Kp*error_speed[2];
	PID_speed[3] = old_PID_speed[3] + Kp*error_speed[3];

	old_PID_speed[0] = PID_speed[0];
	old_PID_speed[1] = PID_speed[1];
	old_PID_speed[2] = PID_speed[2];
	old_PID_speed[3] = PID_speed[3];

	//now, let's control motor PWM
}
void Set_Motor_PWM(){
	//mind the order LF, RF, LB, RB
	//Set motor rotation direction first
	//LF
	if (PID_speed[0] > 0 || PID_speed[0] == 0){
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, SET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, RESET);
	}
	else if(PID_speed[0] < 0){
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, SET);
		PID_speed[0] *= -1;
	}

	//RF
	if (PID_speed[1] > 0 || PID_speed[1] == 0){
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, SET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, RESET);
	}
	else if(PID_speed[1] < 0){
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, RESET);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, SET);
		PID_speed[1] *= -1;
	}

	//LB
	if (PID_speed[2] > 0 || PID_speed[2] == 0){
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, SET);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, RESET);
	}
	else if(PID_speed[2] < 0){
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, RESET);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, SET);
		PID_speed[2] *= -1;
	}

	//RB
	if (PID_speed[3] > 0 || PID_speed[3] == 0){
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, SET);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, RESET);
	}
	else if(PID_speed[3] < 0){
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, RESET);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, SET);
		PID_speed[3] *= -1;
	}
	//For Safety, PID_speed won't go beyond 9,000
	uint8_t i = 0;
	while (i < 4){
		if (PID_speed[i]> 3000){
			PID_speed[i] = 3000;
		}
		i++;
	}
	//Set PWM value
	 TIM1->CCR1 = PID_speed[0];
	 TIM1->CCR2 = PID_speed[1];
	 TIM1->CCR3 = PID_speed[2];
	 TIM1->CCR4 = PID_speed[3];
}
uint32_t Calculate_Value(uint32_t val){
	return 164.18 * exp(0.0112 * val);
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
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

#ifdef  USE_FULL_ASSERT
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
