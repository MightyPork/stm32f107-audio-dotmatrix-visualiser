//
// Created by MightyPork on 2.9.16.
//

#include <inttypes.h>
#include <stm32f1xx_hal_gpio.h>
#include <dotmatrix.h>
#include "mxconstants.h"
#include "stm32f1xx_hal.h"
#include "utils.h"
#include "adc.h"
#include "tim.h"
#include "user_main.h"

static uint32_t audio_samples[256];

static DotMatrix_Cfg *disp;

void start_DMA() {
	//uart_print("- Starting ADC DMA\n");

	HAL_ADC_Start_DMA(&hadc1, audio_samples, 32);
	HAL_TIM_Base_Start(&htim3);
}

/** This callback is called by HAL after the transfer is complete */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
//	uart_print("- DMA complete.\n");

//	char x[100];
//
//	for (int i = 0; i < 256; i++) {
//		sprintf(x, "%"PRIu32" ", audio_samples[i]);
//		uart_print(x);
//	}
//	uart_print("\n");

	dmtx_clear(disp);
	for (int i = 0; i < 32; i++) {
		dmtx_set(disp, i, ((audio_samples[i])>>6)-24, 1);
	}
	dmtx_show(disp);
}

void user_main() {
	uart_print("== USER CODE STARTING ==\n");

	// Leds OFF
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, 1);
	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 1);
	HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, 1);
	HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, 1);

	// Enable audio input
	HAL_GPIO_WritePin(AUDIO_NSTBY_GPIO_Port, AUDIO_NSTBY_Pin, 1);

	DotMatrix_Init disp_init;
	disp_init.cols = 4;
	disp_init.rows = 2;
	disp_init.CS_GPIOx = SPI1_CS_GPIO_Port;
	disp_init.CS_PINx = SPI1_CS_Pin;
	disp_init.SPIx = SPI1;
	disp = dmtx_init(&disp_init);

	dmtx_intensity(disp, 2);

	dmtx_clear(disp);
	dmtx_show(disp);

	while (1) {
		// Blink
		HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		HAL_Delay(50);

		//uart_print("Main loop\n");
		start_DMA();

		//dmtx_toggle(disp, 31, 15);
		//dmtx_show(disp);
	}
}

//region Error handlers

void user_Error_Handler() {
	uart_print("HAL error occurred.\n");
	while (1);
}

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void user_assert_failed(uint8_t *file, uint32_t line) {
	user_error_file_line("Assert failed", (const char *) file, line);
}

void user_error_file_line(const char *message, const char *file, uint32_t line) {
	uart_print(message);
	uart_print(" in file ");
	uart_print((char *) file);
	uart_print(" on line ");

	char x[10];
	sprintf(x, "%"PRIu32, line);
	uart_print(x);
	uart_print("\n");

	while (1);
}

// endregion