//
// Created by MightyPork on 2.9.16.
//

#include <inttypes.h>
#include <arm_math.h>
#include <arm_const_structs.h>
#include <stm32f1xx_hal_gpio.h>
#include "dotmatrix.h"
#include "adc.h"
#include "tim.h"
#include "user_main.h"
#include "debounce.h"
#include "debug.h"

// 512 = show 0-5 kHz
// 256 = show 0-10 kHz
// smaller range is OK since we have only a limited reception of higher frequencies anyway
#define SAMPLE_COUNT 512
#define BIN_COUNT (SAMPLE_COUNT/2)
#define CFFT_INST arm_cfft_sR_f32_len256

#define SCREEN_W 32
#define SCREEN_H 16

// Pins
#define BTN_CENTER 0
#define BTN_LEFT 1
#define BTN_RIGHT 2
#define BTN_UP 3
#define BTN_DOWN 4

// Y axis scaling factors
#define WAVEFORM_SCALE 0.008f
#define FFT_SCALE 0.25f * 0.3f
#define FFT_SPINDLE_SCALE_MULT 0.5f

uint32_t audio_samples[SAMPLE_COUNT * 2]; // 2x size needed for complex FFT
float *audio_samples_f = (float *) audio_samples;

/** Dot matrix display instance */
DotMatrix_Cfg *disp;

/** Capture in progress flag */
volatile bool capture_pending = false;

/** scale & brightness config fields. Initial values. */
float y_scale = 3;
uint8_t brightness = 4;

/** active rendering mode (visualisation preset) */
enum {
	MODE_WAVEFORM = 0,
	MODE_SPECTRUM = 1,
	MODE_SPECTRUM2 = 2,
} render_mode;

#define MODE_MAX MODE_SPECTRUM2

bool up_pressed = false;
bool down_pressed = false;
bool left_pressed = false;
bool right_pressed = false;

static void display_wave();

static void calculate_fft();

static void display_fft();

static void display_fft_spindle();

static void start_render();

// region Audio capture & display

/** Start DMA to capture audio */
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
	switch (render_mode) {
		case MODE_WAVEFORM:
			display_wave();
			break;

		case MODE_SPECTRUM:
			calculate_fft();
			display_fft();
			break;

		case MODE_SPECTRUM2:
			calculate_fft();
			display_fft_spindle();
			break;
	}

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
	samples_to_float();

	int x_offset = 0;

	for (int i = 1; i < SAMPLE_COUNT; i++) {
		if (audio_samples_f[i] > 0 && audio_samples_f[i - 1] < 0) {
			x_offset = i;
			break;
		}
	}

	// make sure we're not gonna run out of range
	if (x_offset >= SAMPLE_COUNT - SCREEN_W) {
		x_offset = 0;
	}

	float totalmult = WAVEFORM_SCALE * y_scale;

	start_render();
	for (int i = 0; i < SCREEN_W; i++) {
		dmtx_set(disp, i, 7 + roundf(audio_samples_f[i + x_offset] * totalmult), 1);
	}

	dmtx_show(disp);
}

/** Calculate and display FFT */
static void calculate_fft()
{
	float *bins = audio_samples_f;

	samples_to_float();
	spread_samples_for_fft();

	const arm_cfft_instance_f32 *S;
	S = &CFFT_INST;

	arm_cfft_f32(S, bins, 0, true); // bit reversed FFT
	arm_cmplx_mag_f32(bins, bins, BIN_COUNT); // get magnitude (extract real values)

	// Normalize & display

	start_render();

	float factor = (1.0f / BIN_COUNT) * FFT_SCALE * y_scale;
	for (int i = 0; i < BIN_COUNT; i++) { // +1 because bin 0 is always 0
		bins[i] *= factor;
	}
}

/** Render classic FFT */
static void display_fft()
{
	float *bins = audio_samples_f;

	for (int x = 0; x < SCREEN_W; x++) {
		for (int j = 0; j < 1 + floorf(bins[x]); j++) {
			dmtx_set(disp, x, j, 1);
		}
	}

	dmtx_show(disp);
}

/** Render FFT "spindle" */
static void display_fft_spindle()
{
	float *bins = audio_samples_f;

	for (int x = 0; x < SCREEN_W; x++) {
		for (int j = 0; j < 1 + floorf(bins[x] * FFT_SPINDLE_SCALE_MULT); j++) {
			dmtx_set(disp, x, 7 + j, 1);
			dmtx_set(disp, x, 7 - j, 1);
		}
	}

	dmtx_show(disp);
}

// endregion

// region UI

/** Clear screen & render "HUD" - button feedback lights */
void start_render()
{
	dmtx_clear(disp);

	if (up_pressed) dmtx_set(disp, SCREEN_W - 2, SCREEN_H - 1, 1);
	if (down_pressed) dmtx_set(disp, SCREEN_W - 2, SCREEN_H - 3, 1);
	if (left_pressed) dmtx_set(disp, SCREEN_W - 3, SCREEN_H - 2, 1);
	if (right_pressed) dmtx_set(disp, SCREEN_W - 1, SCREEN_H - 2, 1);
}

/** Callback when button press state changes */
static void gamepad_button_cb(uint32_t btn, bool press)
{
	dbg("Button press %d, state %d", btn, press);

	switch (btn) {
		case BTN_UP:
			up_pressed = press;
			break;

		case BTN_DOWN:
			down_pressed = press;
			break;

		case BTN_LEFT:
			left_pressed = press;
			break;

		case BTN_RIGHT:
			right_pressed = press;
			break;

		case BTN_CENTER:
			if (!press) {
				// center button released
				// cycle through modes
				if (render_mode++ == MODE_MAX) {
					render_mode = 0;
				}
			}

			info("Switched to render mode %d", render_mode);
			break;
	}
}

// endregion

/**
 * Increment timebase counter each ms.
 * This is called by HAL, weak override.
 */
void HAL_SYSTICK_Callback(void)
{
	timebase_ms_cb();
}

/** Init the application */
void user_init()
{
	// Enable audio input
	HAL_GPIO_WritePin(AUDIO_NSTBY_GPIO_Port, AUDIO_NSTBY_Pin, 1);

	// Init display
	DotMatrix_Init disp_init;
	disp_init.cols = 4;
	disp_init.rows = 2;
	disp_init.CS_GPIOx = SPI1_CS_GPIO_Port;
	disp_init.CS_PINx = SPI1_CS_Pin;
	disp_init.SPIx = SPI1;
	disp = dmtx_init(&disp_init);

	dmtx_intensity(disp, brightness);

	dmtx_clear(disp);
	dmtx_show(disp);

	timebase_init(5, 5);
	debounce_init(5);

	// Gamepad
	debo_init_t debo;
	debo.debo_time = 50;
	debo.invert = true;
	debo.callback = gamepad_button_cb;
	// Central button
	debo.cb_payload = BTN_CENTER;
	debo.GPIOx = BTN_CE_GPIO_Port;
	debo.pin = BTN_CE_Pin;
	debo_register_pin(&debo);
	// Left
	debo.cb_payload = BTN_LEFT;
	debo.GPIOx = BTN_L_GPIO_Port;
	debo.pin = BTN_L_Pin;
	debo_register_pin(&debo);
	// Right
	debo.cb_payload = BTN_RIGHT;
	debo.GPIOx = BTN_R_GPIO_Port;
	debo.pin = BTN_R_Pin;
	debo_register_pin(&debo);
	// Up
	debo.cb_payload = BTN_UP;
	debo.GPIOx = BTN_UP_GPIO_Port;
	debo.pin = BTN_UP_Pin;
	debo_register_pin(&debo);
	// Down
	debo.cb_payload = BTN_DOWN;
	debo.GPIOx = BTN_DN_GPIO_Port;
	debo.pin = BTN_DN_Pin;
	debo_register_pin(&debo);
}

/** Main function, called from MX-generated main.c */
void user_main()
{
	banner("== USER CODE STARTING ==");

	user_init();

	ms_time_t counter1 = 0;
	ms_time_t counter2 = 0;
	ms_time_t btn_scale_cnt = 0;
	ms_time_t btn_brt_cnt = 0;
	while (1) {
		if (ms_loop_elapsed(&counter1, 500)) {
			// Blink
			HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		}

		// hold-to-repeat for up/down buttons (sensitivity)
		// This is not the correct way to do it, but good enough
		if (ms_loop_elapsed(&btn_scale_cnt, 50)) {
			if (up_pressed) {
				y_scale += 0.1;
			}

			if (down_pressed) {
				if (y_scale > 0.1) {
					y_scale -= 0.1;
				}
			}

			if (up_pressed || down_pressed) {
				dbg("scale = %.1f", y_scale);
			}
		}

		// hold-to-repeat for left/right buttons (brightness)
		if (ms_loop_elapsed(&btn_brt_cnt, 200)) {
			if (left_pressed) {
				if (brightness > 0) {
					brightness--;
				}
			}

			if (right_pressed) {
				if (brightness < 15) {
					brightness++;
				}
			}

			if (left_pressed || right_pressed) {
				dmtx_intensity(disp, brightness);
			}
		}

		// capture a sample to update display
		if (!capture_pending) {
			capture_start();
		}
	}
}

//region Error handlers

/** Called from MX-generated HAL error handler */
void user_Error_Handler()
{
	error("HAL error occurred.\n");
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
	error("%s in file %s on line %d", message, file, line);
	while (1);
}

// endregion