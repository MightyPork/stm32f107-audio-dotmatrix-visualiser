#ifndef SCROLLTEXT_H
#define SCROLLTEXT_H

#include "dotmatrix.h"
#include "timebase.h"

volatile extern bool text_moving;

void printtext(DotMatrix_Cfg *disp, const char *text, int x, int y);

void scrolltext(DotMatrix_Cfg *disp, const char *text, ms_time_t step);

#endif // SCROLLTEXT_H
