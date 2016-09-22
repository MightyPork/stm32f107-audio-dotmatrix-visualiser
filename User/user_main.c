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
#include "fft_windows.h"
#include "screendef.h"
#include "scrolltext.h"

// 512 = show 0-5 kHz
// 256 = show 0-10 kHz
// smaller range is OK since we have only a limited reception of higher frequencies anyway
#define SAMPLE_COUNT 512
#define BIN_COUNT (SAMPLE_COUNT/2)
#define CFFT_INST arm_cfft_sR_f32_len256

#define DEFAULT_BRIGHTNESS 3

// Pins
#define BTN_CENTER 0
#define BTN_LEFT 1
#define BTN_RIGHT 2
#define BTN_UP 3
#define BTN_DOWN 4

// Y axis scaling factors
#define WAVEFORM_SCALE 0.02
#define FFT_PRELOG_SCALE 1.0
#define FFT_FINAL_SCALE 3.0
#define FFT_PREFFT_SCALE 0.4

#define VOL_STEP_TIME 50
#define VOL_STEP 0.05
#define VOL_STEP_LARGE 0.75
#define VOL_STEP_THRESH 0.01
#define VOL_STEP_SPEEDUP_TIME 750

#define SCROLL_STEP_MS 15

uint32_t audio_samples[SAMPLE_COUNT * 2]; // 2x size needed for complex FFT
float *audio_samples_f = (float *) audio_samples;

// counter for auto repeat
ms_time_t updn_press_timer = 0;
ms_time_t ltrt_press_timer = 0;
ms_time_t updn_hold_ts;

/** Dot matrix display instance */
DotMatrix_Cfg *disp;

/** Capture in progress flag */
volatile bool capture_pending = false;

/** scale & brightness config fields. Initial values. */
float y_scale = 1;
uint8_t brightness = DEFAULT_BRIGHTNESS;

/** var used to signalize to MAIN that a banner should scroll */
static bool request_banner_text = false;

/** active rendering mode (visualisation preset) */
enum {
	MODE_SPECTRUM,
	MODE_SPECTRUM2,
	MODE_LINEAR,
	MODE_WAVEFORM,
	MAX_MODE
} render_mode;

bool up_pressed = false;
bool down_pressed = false;
bool left_pressed = false;
bool right_pressed = false;

static void display_wave();

static void calculate_fft(bool logmode);

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
	if (text_moving || request_banner_text) {
		capture_pending = false;
		return;
	}

	switch (render_mode) {
		case MODE_WAVEFORM:
			display_wave();
			break;

		case MODE_LINEAR:
			calculate_fft(false);
			display_fft();
			break;

		case MODE_SPECTRUM:
			calculate_fft(true);
			display_fft();
			break;

		case MODE_SPECTRUM2:
			calculate_fft(true);
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
		audio_samples_f[i * 2] = audio_samples_f[i]; // * win_hamming_512[i]; // real
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

/** Calculate FFT */
static void calculate_fft(bool logmode)
{
	float *bins = audio_samples_f;

	samples_to_float();

	for (int i = 0; i < SAMPLE_COUNT; i++) {
		bins[i] *= y_scale * FFT_PREFFT_SCALE; //win_hamming_512[i];
	}

	spread_samples_for_fft();

	const arm_cfft_instance_f32 *S;
	S = &CFFT_INST;

	arm_cfft_f32(S, bins, 0, true); // bit reversed FFT
	arm_cmplx_mag_f32(bins, bins, BIN_COUNT); // get magnitude (extract real values)

	// Normalize & display

	for (int i = 0; i < BIN_COUNT; i++) { // +1 because bin 0 is always 0
		float bin = bins[i] * (1.0f / BIN_COUNT) * FFT_PRELOG_SCALE;
		if (logmode) {
			bin = log2f(bin);
		} else {
			//
		}
		bins[i] = bin * FFT_FINAL_SCALE;
	}
}

/** Render classic FFT */
static void display_fft()
{
	start_render();

	float *bins = audio_samples_f;

	for (int x = 0; x < SCREEN_W; x++) {
		float bin = floorf(bins[x]);
		if (bin<0) bin=0;
		for (int j = 0; j < 1 + bin; j++) {
			dmtx_set(disp, x, j, 1);
		}
	}

	dmtx_show(disp);
}

/** Render FFT "spindle" */
static void display_fft_spindle()
{
	start_render();

	float *bins = audio_samples_f;

	for (int x = 0; x < SCREEN_W; x++) {
		float bin = floorf(bins[x] * 0.5f);
		if (bin<0) bin=0;
		for (int j = 0; j < 1 + bin; j++) {
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
			if (press) {
				updn_hold_ts = ms_now();
				updn_press_timer = 0;
				y_scale += VOL_STEP;
			}
			break;

		case BTN_DOWN:
			down_pressed = press;
			if (press) {
				updn_hold_ts = ms_now();
				updn_press_timer = 0;
				if (y_scale > VOL_STEP + VOL_STEP_THRESH) {
					y_scale -= VOL_STEP;
				}
			}
			break;

		case BTN_LEFT:
			left_pressed = press;
			if (press) {
				ltrt_press_timer = 0;
				if (brightness > 0) brightness--;
			}
			break;

		case BTN_RIGHT:
			right_pressed = press;
			if (press) {
				ltrt_press_timer = 0;
				if (brightness < 15) brightness++;
			}
			break;

		case BTN_CENTER:
			if (!press) {
				// center button released
				// cycle through modes
				if (++render_mode == MAX_MODE) {
					render_mode = 0;
				}

				info("Switched to render mode %d", render_mode);
				request_banner_text = true;
			}

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

	request_banner_text = true;

	ms_time_t counter1 = 0;
	while (1) {
		if (ms_loop_elapsed(&counter1, 500)) {
			// Blink
			HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		}

		// hold-to-repeat
		// This is not the correct way to do it, but good enough
		if (ms_loop_elapsed(&updn_press_timer, VOL_STEP_TIME)) {
			if (up_pressed) {
				if (ms_now() - updn_hold_ts > VOL_STEP_SPEEDUP_TIME) {
					y_scale += VOL_STEP_LARGE;
				} else {
					y_scale += VOL_STEP;
				}
			}

			if (down_pressed) {
				if (ms_now() - updn_hold_ts > VOL_STEP_SPEEDUP_TIME) {
					if (y_scale > VOL_STEP_LARGE + VOL_STEP_THRESH) {
						y_scale -= VOL_STEP_LARGE;
					} else if (y_scale > VOL_STEP + VOL_STEP_THRESH) {
						y_scale -= VOL_STEP;
					}
				} else {
					if (y_scale > VOL_STEP + VOL_STEP_THRESH) {
						y_scale -= VOL_STEP;
					}
				}
			}

			if (up_pressed || down_pressed) {
				dbg("scale = %.2f", y_scale);
			}
		}

		if (ms_loop_elapsed(&ltrt_press_timer, 250)) {
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

		if (request_banner_text) {
			switch (render_mode) {
				case MODE_SPECTRUM:
					scrolltext(disp, "FFT LOG-Y", SCROLL_STEP_MS);
					break;

				case MODE_SPECTRUM2:
					scrolltext(disp, "FFT LOG-Y MIRROR", SCROLL_STEP_MS);
					break;

				case MODE_LINEAR:
					scrolltext(disp, "FFT LIN-Y", SCROLL_STEP_MS);
					break;

				case MODE_WAVEFORM:
					scrolltext(disp, "WAVEFORM", SCROLL_STEP_MS);
					break;
			}
			request_banner_text = false;
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