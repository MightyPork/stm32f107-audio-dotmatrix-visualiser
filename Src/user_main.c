//
// Created by MightyPork on 2.9.16.
//

#include <inttypes.h>
#include <stm32f1xx_hal_gpio.h>
#include <dotmatrix.h>
#include <arm_math.h>
#include <arm_const_structs.h>
#include "mxconstants.h"
#include "stm32f1xx_hal.h"
#include "utils.h"
#include "adc.h"
#include "tim.h"
#include "user_main.h"

#define SAMPLE_COUNT 256
#define BIN_COUNT (SAMPLE_COUNT/2)

#define SCREEN_W 32
#define SCREEN_H 16

static uint32_t audio_samples[SAMPLE_COUNT * 2]; // 2x size needed for complex FFT
static float *audio_samples_f = (float *) audio_samples;

static DotMatrix_Cfg *disp;

static volatile bool capture_pending = false;

static void display_wave();
static void display_fft();

void capture_start()
{
	if (capture_pending) return;
	capture_pending = true;

	//uart_print("- Starting ADC DMA\n");

	HAL_ADC_Start_DMA(&hadc1, audio_samples, SAMPLE_COUNT);
	HAL_TIM_Base_Start(&htim3);
}

/** This callback is called by HAL after the transfer is complete */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	display_wave();
	capture_pending = false;
}

/**
 * Convert audio samples to float.
 * NOTE: This trashes the original array of ints, they share the same memory location.
 */
void samples_to_float()
{
	// Convert to float
	for (int i = 0; i < SAMPLE_COUNT; i++) {
		audio_samples_f[i] = (float) audio_samples[i];
	}

	// Obtain mean value
	float mean;
	arm_mean_f32(audio_samples_f, SAMPLE_COUNT, &mean);

	// Subtract mean from all samples
	for (int i = 0; i < SAMPLE_COUNT; i++) {
		audio_samples_f[i] -= mean;
	}
}

/** Spread numbers in the samples array so that they are interleaved by zeros (imaginary part) */
void spread_samples_for_fft()
{
	for (int i = SAMPLE_COUNT - 1; i >= 0; i--) {
		audio_samples_f[i * 2 + 1] = 0;              // imaginary
		audio_samples_f[i * 2] = audio_samples_f[i]; // real
	}
}

/** Display waveform preview */
void display_wave()
{
	float wave_y_mult = 0.0078125f;
	float relative = 1;

	samples_to_float();

	int x_offset = 0;

	for (int i = 1; i < SAMPLE_COUNT; i++) {
		if (audio_samples_f[i] > 0 && audio_samples_f[i-1] < 0) {
			x_offset = i;
			break;
		}
	}

	// make sure we're not gonna run out of range
	if (x_offset >= SAMPLE_COUNT - SCREEN_W) {
		x_offset = 0;
	}

	dmtx_clear(disp);
	for (int i = 0; i < SCREEN_W; i++) {
		dmtx_set(disp, i, 7 + roundf(audio_samples_f[i + x_offset] * wave_y_mult * relative), 1);
	}
	dmtx_show(disp);
}

/** Calculate and display FFT */
static void display_fft()
{
	float *bins = audio_samples_f;

	samples_to_float();
	spread_samples_for_fft();

	const arm_cfft_instance_f32 *S;
	S = &arm_cfft_sR_f32_len128;

	arm_cfft_f32(S, bins, 0, true); // bit reversed FFT
	arm_cmplx_mag_f32(bins, bins, BIN_COUNT); // get magnitude (extract real values)

	// Normalize & display

	dmtx_clear(dmtx);

	float factor = (1.0f / BIN_COUNT) * 0.25f;
	for (int i = 0; i < BIN_COUNT; i++) { // +1 because bin 0 is always 0
		bins[i] *= factor;
	}

	// TODO implement offset using gamepad buttons
	for (int x = 0; x < SCREEN_W; x++) {
		for (int j = 0; j < 1 + floorf(bins[x]); j++) {
			dmtx_set(dmtx, x, j, 1);
		}
	}

	dmtx_show(dmtx);
}


void user_main()
{
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

	dmtx_intensity(disp, 7);

	dmtx_clear(disp);
	dmtx_show(disp);

	uint32_t counter1 = 0;
	uint32_t counter2 = 0;
	while (1) {
		if (counter1++ == 500) {
			counter2 = 0;
			// Blink
			HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		}

		if (counter2++ >= 5) {
			if (!capture_pending) {
				counter2 = 0;
				capture_start();
			}
		}

		HAL_Delay(1);
	}
}

//region Error handlers

void user_Error_Handler()
{
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
void user_assert_failed(uint8_t *file, uint32_t line)
{
	user_error_file_line("Assert failed", (const char *) file, line);
}

void user_error_file_line(const char *message, const char *file, uint32_t line)
{
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