//
// Created by MightyPork on 2.9.16.
//

#include <string.h>
#include "stm32f1xx_hal_conf.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "utils.h"

void uart_print(const char *string)
{
	HAL_UART_Transmit(&huart1, (char*)string, (uint16_t) strlen(string), 10);

	// Stop ADC and the timer
	HAL_ADC_Stop(&hadc1);
	HAL_TIM_Base_Stop(&htim3);
}