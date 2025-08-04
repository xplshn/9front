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

#define max(a, b) a > b ? a : b

typedef struct Param	Param;
struct Param {
	Image	*dst;
	Image	*src;
	int     t;
	Point   sp;
};

// swaps two numbers
void swap(int* a , int* b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

// draws a pixel on screen of given brightness 0 <= brightness <= 1.
void drawPixel(int x, int y, float brightness, Param p)
{
	int c, t;
	Image *dst, *src;
	Point sp;
	float th;

	src = p.src;
	dst = p.dst;
	t   = p.t;
	sp  = p.sp;
	c   = 255 * brightness;
	Image *c1 = allocimage(display, Rect(0,0,t,t), GREY8, 1, setalpha(DOpaque, c));
	if (t > 1) {
	  th  = t/2.0;
	  sp  = subpt(sp, Pt(th, th));
	  draw(dst, Rect(x-(th), y-(th), x+ceil(th), y+ceil(th)), src, c1, sp);
	} else
	  draw(dst, Rect(x, y, x+t, y+t), src, c1, sp);
	freeimage(c1);
}

void _line(int x0, int y0, int x1, int y1, Param p)
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
		  drawPixel(y0    , x, rfpart, p);
		  drawPixel(y0 + 1, x, fpart , p);
		  intery += gradient;
        }
    }
    else
    {
        for (int x = xpxl1; x <= xpxl2; x++) {
		  y0 = floor(intery);
		  fpart = intery - y0;
		  rfpart = 1 - fpart;
		  drawPixel(x	, y0    , rfpart, p);
		  drawPixel(x	, y0 + 1, fpart , p);
		  intery += gradient;
        }
    }
}

void drawQuadCircle(int x0, int y0, int x, int y, float fpart, Param p)
{
	float rfpart = 1 - fpart;
	drawPixel( x0 + x , y0 + y    , fpart , p);
	drawPixel( x0 + x , y0 + y + 1, rfpart, p);
	drawPixel( x0 + y , y0 + x    , fpart , p);
	drawPixel( x0 + y + 1, y0 + x , rfpart, p);
}

void drawQuadEllipse(int x0, int y0, int x, int y, float fpart, Param p)
{
	float rfpart = 1 - fpart;
	drawPixel( x0 + x , y0 + y    , fpart , p);
	drawPixel( x0 + x , y0 + y + 1, rfpart, p);
}

void _circle(int x0, int y0, int a, Param p)
{
	int i, j;
	float intery, d;
	float fpart, rfpart;

	i = d = 0;
	j = a;
	while (i < j) {
	  intery = sqrt(max(a * a - i * i, 0));
	  fpart  = ceil(intery) - intery;
	  rfpart = 1 - fpart;
	  if (fpart < d)
		j--;
	  d = fpart;

	  drawQuadCircle(x0, y0,  i,  j, fpart , p);
	  drawQuadCircle(x0, y0,  i, -j, rfpart, p);
	  drawQuadCircle(x0, y0, -i,  j, fpart , p);
	  drawQuadCircle(x0, y0, -i, -j, rfpart, p);

	  i++;
	}
}

void _ellipse(int x0, int y0, int a, int b, Param p)
{
    if (a == b) {
      _circle(x0, y0, a, p);
      return;
    }

	int i, j;
	float intery;
	float fpart, rfpart;

	i = 0;
	j = b;
	while (i <= a) {
	  intery = b * sqrt(max(1 - (i * i * 1.0) / (a * a), 0));
	  fpart  = ceil(intery) - intery;
	  rfpart = 1 - fpart;
	  if (j - intery > 2) {
		_line(x0 + (i - 1), y0 + j, x0 + i, y0 + ceil(intery), p);
		_line(x0 + (i - 1), y0 - j, x0 + i, y0 - ceil(intery), p);
		_line(x0 - (i - 1), y0 + j, x0 - i, y0 + ceil(intery), p);
		_line(x0 - (i - 1), y0 - j, x0 - i, y0 - ceil(intery), p);
		/* printf("draw %d %d %d %d\n", i-1, j, i, (int)ceil(intery)); */
	  }
	  j = ceil(intery);
	  /* printf("%d %d %f %f\n", i, j, intery, fpart - d); */

	  drawQuadEllipse(x0, y0,  i,  j, fpart , p);
	  drawQuadEllipse(x0, y0,  i, -j, rfpart, p);
	  drawQuadEllipse(x0, y0, -i,  j, fpart , p);
	  drawQuadEllipse(x0, y0, -i, -j, rfpart, p);

	  i++;
	}
}

void _bezier(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, Param p)
{
    int i, j, x, y;
    float d1, d2, d3, d4, d5, d;
	float fpart, rfpart;

    float dx = x2 - x0;
    float step = (dx == 0.0) ? 1 : 1 / dx;

	i = x0;
	j = y0;
	fpart = 0;
	while (x < x2) {
	  fpart += step;
	  rfpart = 1 - fpart;
	  d1 = x0 * rfpart + x1 * fpart;
	  d2 = x1 * rfpart + x2 * fpart;
	  d  = d1 * rfpart + d2 * fpart;
	  x = (d);
	  if (x - i < 1)
		continue;

	  d1 = y0 * rfpart + y1 * fpart;
	  d2 = y1 * rfpart + y2 * fpart;
	  d  = d1 * rfpart + d2 * fpart;
	  y  = (d);
	  if (j - y > 1) {
		_line(i, j, x, y, p);
		/* printf("Line %d %d %d %f\n", i, j, x, d); */
	  } else {
		/* printf("%d %d %d %d %f\n", i, j, x, y, fpart); */
		drawPixel(x, y    , fpart, p);
		drawPixel(x, y - 1, rfpart, p);
	  }
	  i = x, j = y;
	}
}

void line(Image *dst, Point p0, Point p1, int end0, int end1, int radius,
		  Image *src, Point sp)
{
  Param p;
  p.src = src;
  p.dst = dst;
  p.sp  = sp;
  p.t   = radius;
  _line(p0.x, p0.y, p1.x, p1.y, p);
}

void ellipse(Image *dst, Point c, int a, int b, int thick, Image *src, Point sp)
{
  Param p;
  p.src = src;
  p.dst = dst;
  p.sp  = sp;
  p.t   = thick;
  _ellipse(c.x, c.y, a, b, p);
}

void circle(Image *dst, Point c, int a, int thick, Image *src, Point sp)
{
  Param p;
  p.src = src;
  p.dst = dst;
  p.sp  = sp;
  p.t   = thick;
  _circle(c.x, c.y, a, p);
}

int bezier(Image *dst, Point p0, Point p1, Point p2, Point p3,
		   int end0, int end1, int radius, Image *src, Point sp)
{
  Param p;
  p.src = src;
  p.dst = dst;
  p.sp  = sp;
  p.t   = radius;
  _bezier(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p);
  return 1;
}

/* void arc(Image *dst, Point c, int a, int b, int thick, Image *src, Point sp, */
/* 		 int alpha, int phi) */
/* { */
/*   Param p; */
/*   p.src = src; */
/*   p.dst = dst; */
/*   _line(x0, y0, x1, y1, p); */
/* }   */
