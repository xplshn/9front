/**
Xiaolin Wu's line algorithm https://en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm
*/

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include "dat.h"

// swaps two numbers
void swap(int* a , int* b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

// draws a pixel on screen of given brightness 0 <= brightness <= 1.
void drawPixel(Image *dst, int x, int y, float brightness, Image *src)
{
    int c = 255 * brightness;
	Image *c1 = allocimage(display, Rect(0,0,1,1), GREY8, 1, setalpha(DOpaque, c));
	draw(dst, Rect(x, y, x+1, y+1), src, c1, ZP);
	freeimage(c1);
}

void drawAALine(Image *dst, int x0, int y0, int x1, int y1, Image *src)
{
    int steep = fabs(y1 - y0) > fabs(x1 - x0);

    // swap the co-ordinates if slope > 1
    if (steep)
    {
        swap(&x0, &y0);
        swap(&x1, &y1);
    }
    if (x0 > x1)
    {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }

    // compute the slope
    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient;
    if (dx == 0.0)
	  gradient = 1.0;
	else
	  gradient = dy / dx;

    int xpxl1 = x0;
    int xpxl2 = x1;
    float intery = y0;
	float fpart, rfpart;

    if (steep)
    {
        for (int x = xpxl1; x <= xpxl2; x++) {
		  y0 = floor(intery);
		  fpart = intery - y0;
		  rfpart = 1 - fpart;
		  drawPixel(dst	,  y0    , x, rfpart, src);
		  drawPixel(dst	,  y0 + 1, x, fpart , src);
		  intery += gradient;
        }
    }
    else
    {
        for (int x = xpxl1; x <= xpxl2; x++) {
		  y0 = floor(intery);
		  fpart = intery - y0;
		  rfpart = 1 - fpart;
		  drawPixel(dst	, x	, y0    , rfpart, src);
		  drawPixel(dst	, x	, y0 + 1, fpart , src);
		  intery += gradient;
        }
    }
}
