#include <string.h>
#include "scrolltext.h"

#include "font.h"
#include "screendef.h"

volatile bool text_moving;

/**
 * Print text at coordinates
 * @param text
 * @param x
 * @param y
 */
void printtext(DotMatrix_Cfg *disp, const char *text, int x, int y)
{
	int totalX = 0;

	for (int textX = 0; textX < (int)strlen(text); textX++) {
		uint8_t ch = (uint8_t)text[textX];
		if (ch < FONT_MIN) ch = '?';
		if (ch > FONT_MAX) ch = '?';
		ch -= ' '; // normalize for font table

		if (ch == 0) { // space
			totalX += 4;
			continue;
		}

		// one letter
		uint8_t blanks = 0;

		// skip empty space on right
		for (int charX = FONT_WIDTH-1; charX >= 0; charX--) {
			uint8_t col = font[ch][charX];
			if (col == 0x00) {
				blanks++;
			} else {
				break;
			}
		}

		for (int charX = 0; charX < FONT_WIDTH - blanks; charX++) {
			uint8_t col = font[ch][charX];
			for (int charY = 0; charY < 8; charY++) {
				dmtx_set(disp, x + totalX, y + 8 - charY, (col >> charY) & 1);
			}
			totalX++;
		}

		totalX+= 2; // gap
	}
}

/**
 * Scroll text across the screen
 * @param text
 * @param step delay per step (ms)
 */
void scrolltext(DotMatrix_Cfg *disp, const char *text, ms_time_t step)
{
	(void)step;

	text_moving = true;
	for (int i = 0; i < (int)strlen(text)*(FONT_WIDTH+1) + SCREEN_W-1; i++) {
		if (i > 0) delay_ms(step);

		dmtx_clear(disp);
		printtext(disp, text, (SCREEN_W-1)-i, SCREEN_H/2-4);
		dmtx_show(disp);
	}
	text_moving = false;
}
