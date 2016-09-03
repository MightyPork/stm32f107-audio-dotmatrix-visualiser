#include <string.h>
#include <stdbool.h>
#include <utils.h>
#include "dotmatrix.h"
#include "malloc_safe.h"

DotMatrix_Cfg* dmtx_init(DotMatrix_Init *init)
{
	DotMatrix_Cfg *disp = calloc_s(1, sizeof(DotMatrix_Cfg));

	disp->drv.SPIx = init->SPIx;
	disp->drv.CS_GPIOx = init->CS_GPIOx;
	disp->drv.CS_PINx = init->CS_PINx;
	disp->drv.chain_len = init->cols * init->rows;
	disp->cols = init->cols;
	disp->rows = init->rows;

	disp->screen = calloc_s(init->cols * init->rows * 8, 1); // 8 bytes per driver

	max2719_cmd_all(&disp->drv, MAX2719_CMD_DECODE_MODE, 0x00); // no decode
	max2719_cmd_all(&disp->drv, MAX2719_CMD_SCAN_LIMIT, 0x07); // scan all 8
	max2719_cmd_all(&disp->drv, MAX2719_CMD_SHUTDOWN, 0x01); // not shutdown
	max2719_cmd_all(&disp->drv, MAX2719_CMD_DISPLAY_TEST, 0x00); // not test
	max2719_cmd_all(&disp->drv, MAX2719_CMD_INTENSITY, 0x07); // half intensity

	// clear
	for (uint8_t i = 0; i < 8; i++) {
		max2719_cmd_all(&disp->drv, MAX2719_CMD_DIGIT0+i, 0);
	}

	return disp;
}

void dmtx_show(DotMatrix_Cfg* disp)
{
	for (uint8_t i = 0; i < 8; i++) {
		// show each digit's array in turn
		max2719_cmd_all_data(&disp->drv, MAX2719_CMD_DIGIT0+i, disp->screen + (i * disp->drv.chain_len));
	}
}

void dmtx_clear(DotMatrix_Cfg* disp)
{
	memset(disp->screen, 0, disp->drv.chain_len*8);
}

void dmtx_intensity(DotMatrix_Cfg* disp, uint8_t intensity)
{
	max2719_cmd_all(&disp->drv, MAX2719_CMD_INTENSITY, intensity & 0x0F);
}

void dmtx_blank(DotMatrix_Cfg* disp, bool blank)
{
	max2719_cmd_all(&disp->drv, MAX2719_CMD_SHUTDOWN, (!blank) & 0x01);
}

/**
 * @brief Get a cell pointer
 * @param disp : driver inst
 * @param x : x coord
 * @param y : y coord
 * @param xd : pointer to store the offset in the cell
 * @return cell ptr
 */
static uint8_t* cell_ptr(DotMatrix_Cfg* disp, int32_t x, int32_t y, uint8_t *xd)
{
	if (x < 0 || y < 0) return NULL;
	if ((uint32_t)x >= disp->cols*8 || (uint32_t)y >= disp->rows*8) return NULL;

	uint32_t cell_x = (uint32_t)x >> 3;
	*xd = x & 7;

	// resolve cell
	uint32_t digit = y & 7;
	cell_x += ((uint32_t)y >> 3) * disp->cols;

	uint32_t cell_idx = (digit * disp->drv.chain_len) + cell_x;

	return &disp->screen[cell_idx];
}


bool dmtx_get(DotMatrix_Cfg* disp, int32_t x, int32_t y)
{
	uint8_t xd;
	uint8_t *cell = cell_ptr(disp, x, y, &xd);
	if (cell == NULL) return 0;

	return (*cell >> xd) & 1;
}


void dmtx_toggle(DotMatrix_Cfg* disp, int32_t x, int32_t y)
{
	uint8_t xd;
	uint8_t *cell = cell_ptr(disp, x, y, &xd);
	if (cell == NULL) return;

	*cell ^= 1 << xd;
}


void dmtx_set(DotMatrix_Cfg* disp, int32_t x, int32_t y, bool bit)
{
	uint8_t xd;
	uint8_t *cell = cell_ptr(disp, x, y, &xd);
	if (cell == NULL) return;

	if (bit) {
		*cell |= 1 << xd;
	} else {
		*cell &= ~(1 << xd);
	}
}

void dmtx_set_block(DotMatrix_Cfg* disp, int32_t startX, int32_t startY, uint32_t *data_rows, uint32_t width, uint16_t height)
{
	for (uint32_t y = 0; y < height; y++) {
		uint32_t row = data_rows[y];

		for (uint32_t x = 0; x < width; x++) {
			int xx = startX + (int)x;
			int yy = startY + (int)y;
			bool val = (row >> (width - x - 1)) & 1;

			dmtx_set(disp, xx, yy, val);
		}
	}
}
