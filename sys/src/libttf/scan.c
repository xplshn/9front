#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <ttf.h>
#include "impl.h"

typedef struct Scan Scan;
typedef struct TTLine TTLine;

enum {
	LINEBLOCK = 32,
	PTBLOCK = 64,
};

struct TTLine {
	int x0, y0;
	int x1, y1;
	int link;
	u8int dir;
};

struct Scan {
	enum {
		DROPOUTS = 1,
		STUBDET = 2,
		SMART = 4,
	} flags;

	TTGlyph *g;
	
	TTLine *lines;
	int nlines;
	
	int *hpts, *vpts;
	int nhpts, nvpts;
	int *hscanl, *vscanl;
	
	u8int *bit;
	int width, height;
	int stride;
};

typedef struct Param	Param;
struct Param {
	Image	*dst;
	Image	*src;
	int     t;
	Point   sp;
	Scan    *s;
};

static void pixel(Scan *s, int x, int y);
void _bezier1(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, Param p);
void drawPixel(int x, int y, float brightness, Param p);

void swap(int* a , int* b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
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

void _bezier1(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, Param p)
{
    int i, j, x, y;
    float dx, step, fpart, rfpart;


    if (x0 > x2)
    {
        swap(&x0, &x2);
        swap(&y0, &y2);
    }
    dx = x2 - x0;
    step = (dx == 0.0) ? 1 : 1 / dx;
	i = x0;
	j = y0;
	fpart = 0;
	/* fprint(2, "c %d %d %d %d %d %d\n", x0, y0, x1, y1, x2, y2); */
	while ((fpart += step) < 1.0) {
	  rfpart = 1 - fpart;
	  x = rfpart * rfpart * x0 + 2 * fpart * rfpart * x1 + fpart * fpart * x2;
	  if (x - i < 1)
		continue;

	  y = rfpart * rfpart * y0 + 2 * fpart * rfpart * y1 + fpart * fpart * y2;
	  if (abs(j - y) > 1) {
		_line(i, j, x, y, p);
		/* fprint(2, "Line %d %d %d %d\n", i, j, x, y); */
	  } else {
		/* fprint(2, "%d %d %d %d %f\n", i, j, x, y, fpart); */
		drawPixel(x, y    , fpart, p);
		drawPixel(x, y - 1, rfpart, p);
	  }
	  i = x, j = y;
	}
}

void drawPixel(int x, int y, float brightness, Param p)
{
 Scan *s = p.s;
 x = x / 64;
 y = y / 64;
 if (!(x >= 0 && x < s->width && y >= 0 && y < s->height)) {
   fprint(2, "err %d %d %d %d", x, y, s->width, s->height);
   return;
 }
 assert(x >= 0 && x < s->width && y >= 0 && y < s->height);
#ifdef GREY
 s->bit[(s->height - 1 - y) * s->stride + x] |= (int)(255 * brightness);
#else
 s->bit[(s->height - 1 - y) * s->stride + (x>>3)] |= (1<<(7-(x&7)));
#endif
}

void
dobezier(Scan *s, TTPoint p, TTPoint q, TTPoint r)
{
  Param p1;
  p1.s   = s;
 /* print("0 %d %d %d %d\n", p.x, p.y, r.x, r.y); */
 _bezier1(p.x, p.y, q.x, q.y, r.x, r.y, 0, 0, p1);

 TTLine *l;
 if((s->nlines & LINEBLOCK - 1) == 0)
		s->lines = realloc(s->lines, sizeof(TTLine) * (s->nlines + LINEBLOCK));
	l = &s->lines[s->nlines++];
	if(p.y < r.y){
		l->x0 = p.x;
		l->y0 = p.y;
		l->x1 = r.x;
		l->y1 = r.y;
		l->dir = 0;
	}else{
		l->x0 = r.x;
		l->y0 = r.y;
		l->x1 = p.x;
		l->y1 = p.y;
		l->dir = 1;
	}
	l->link = -1;
}

static void
dobezier1(Scan *s, TTPoint p, TTPoint q, TTPoint r)
{
	vlong m, n;
	TTLine *l;

	m = (vlong)(q.x - p.x) * (r.y - p.y) - (vlong)(q.y - p.y) * (r.x - p.x);
	n = (vlong)(r.x - p.x) * (r.x - p.x) + (vlong)(r.y - p.y) * (r.y - p.y);
	if(m * m > 4 * n){
		dobezier(s, p, (TTPoint){(p.x+q.x+1)/2, (p.y+q.y+1)/2, 0}, (TTPoint){(p.x+2*q.x+r.x+2)/4, (p.y+2*q.y+r.y+2)/4, 0});
		dobezier(s, (TTPoint){(p.x+2*q.x+r.x+2)/4, (p.y+2*q.y+r.y+2)/4, 0}, (TTPoint){(r.x+q.x+1)/2, (r.y+q.y+1)/2, 0}, r);
		return;
	}
	if((s->nlines & LINEBLOCK - 1) == 0)
		s->lines = realloc(s->lines, sizeof(TTLine) * (s->nlines + LINEBLOCK));
	l = &s->lines[s->nlines++];
	if(p.y < r.y){
		l->x0 = p.x;
		l->y0 = p.y;
		l->x1 = r.x;
		l->y1 = r.y;
		l->dir = 0;
	}else{
		l->x0 = r.x;
		l->y0 = r.y;
		l->x1 = p.x;
		l->y1 = p.y;
		l->dir = 1;
	}
	l->link = -1;
}

static int
hlinecmp(void *va, void *vb)
{
	TTLine *a, *b;
	
	a = va;
	b = vb;
	if(a->y0 < b->y0) return -1;
	if(a->y0 > b->y0) return 1;
	return 0;
}

static int
vlinecmp(void *va, void *vb)
{
	TTLine *a, *b;
	
	a = va;
	b = vb;
	if(a->x0 < b->x0) return -1;
	if(a->x0 > b->x0) return 1;
	return 0;
}

static int
intcmp(void *va, void *vb)
{
	int a, b;
	
	a = *(int*)va;
	b = *(int*)vb;
	return (a>b) - (a<b);
}

static void
hprep(Scan *s)
{
	int i, j, x, y;
	TTLine *l;
	int watch, act, *p;

	qsort(s->lines, s->nlines, sizeof(TTLine), hlinecmp);
	s->hscanl = calloc(sizeof(int), (s->height + 1));
	act = -1;
	watch = 0;
	p = &act;
	for(i = 0; i < s->height; i++){
		y = 64 * i + 32;
		for(; watch < s->nlines && s->lines[watch].y0 <= y; watch++){
			if(s->lines[watch].y1 <= y || s->lines[watch].y0 == s->lines[watch].y1)
				continue;
			s->lines[watch].link = -1;
			*p = watch;
			p = &s->lines[watch].link;
		}
		s->hscanl[i] = s->nhpts;
		p = &act;
		while(j = *p, j >= 0){
			l = &s->lines[j];
			if(l->y1 <= y){
				j = l->link;
				l->link = -1;
				*p = j;
				continue;
			}
			x = l->x0 + ttfvrounddiv((vlong)(y - l->y0)*(l->x1 - l->x0), l->y1 - l->y0);
			if((s->nhpts & PTBLOCK - 1) == 0)
				s->hpts = realloc(s->hpts, (s->nhpts + PTBLOCK) * sizeof(int));
			s->hpts[s->nhpts++] = x << 1 | l->dir;
			p = &l->link;
		}
		qsort(s->hpts + s->hscanl[i], s->nhpts - s->hscanl[i], sizeof(int), intcmp);
	}
	s->hscanl[i] = s->nhpts;
}

static int
iswhite(Scan *s, int x, int y)
{
#ifdef GREY
	return (s->bit[(s->height - 1 - y) * s->stride + x]) == 0;
#else
	return (s->bit[(s->height - 1 - y) * s->stride + (x>>3)] >> 7-(x&7) & 1)==0;
#endif
}

static void
pixel(Scan *s, int x, int y)
{
#ifdef GREY
 Param p;
 p.s = s;
 drawPixel(x << 6, y << 6, 1, p);
#else
	assert(x >= 0 && x < s->width && y >= 0 && y < s->height);
	s->bit[(s->height - 1 - y) * s->stride + (x>>3)] |= (1<<7-(x&7));
#endif
}

static int
intersectsh(Scan *s, int x, int y)
{
	int a, b, c, vc, v;
	
	a = s->hscanl[y];
	b = s->hscanl[y+1]-1;
	v = x * 64 + 32;
	if(a > b || s->hpts[a]>>1 > v + 64 || s->hpts[b]>>1 < v) return 0;
	while(a <= b){
		c = (a + b) / 2;
		vc = s->hpts[c]>>1;
		if(vc < v)
			a = c + 1;
		else if(vc > v + 64)
			b = c - 1;
		else
			return 1;
	}
	return 0;
}

static int
intersectsv(Scan *s, int x, int y)
{
	int a, b, c, vc, v;
	
	a = s->vscanl[x];
	b = s->vscanl[x+1]-1;
	v = y * 64 + 32;
	if(a > b || s->vpts[a]>>1 > v + 64 || s->vpts[b]>>1 < v) return 0;
	while(a <= b){
		c = (a + b) / 2;
		vc = s->vpts[c]>>1;
		if(vc < v)
			a = c + 1;
		else if(vc > v + 64)
			b = c - 1;
		else
			return 1;
	}
	return 0;
}

static void
hscan(Scan *s)
{
	int i, j, k, e;
	int wind, match, seen, x;
	
	for(i = 0; i < s->height; i++){
		e = s->hscanl[i+1];
		k = s->hscanl[i];
		if(k == e) continue;
		wind = 0;
		for(j = 0; j < s->width; j++){
			x = 64 * j + 32;
			match = 0;
			seen = 0;
			while(k < e && (s->hpts[k] >> 1) <= x){
				wind += (s->hpts[k] & 1) * 2 - 1;
				seen |= 1<<(s->hpts[k] & 1);
				if((s->hpts[k] >> 1) == x)
					match++;
				k++;
			}
			if(match || wind)
				pixel(s, j, i);
			else if((s->flags & DROPOUTS) != 0 && seen == 3 && j > 0 && iswhite(s, j-1, i)){
				if((s->flags & STUBDET) == 0){
					pixel(s, j-1, i);
					continue;
				}
				if(i <= 0 || i > s->height - 1 || j <= 0 || j > s->width - 1)
					continue;
				if(!intersectsv(s, j-1, i-1) && !intersectsh(s, j-1, i-1) && !intersectsv(s, j, i-1) || !intersectsv(s, j-1, i) && !intersectsh(s, j-1, i+1) && !intersectsv(s, j, i))
					continue;
				pixel(s, j-1, i);
			}
		}
	}
}

static void
vprep(Scan *s)
{
	int i, j, x, y;
	TTLine *l;
	int watch, act, *p;

	for(i = 0; i < s->nlines; i++){
		l = &s->lines[i];
		if(l->x0 > l->x1){
			x = l->x0, l->x0 = l->x1, l->x1 = x;
			x = l->y0, l->y0 = l->y1, l->y1 = x;
			l->dir ^= 1;
		}
	}
	qsort(s->lines, s->nlines, sizeof(TTLine), vlinecmp);
	s->vscanl = calloc(sizeof(int), (s->width + 1));
	act = -1;
	watch = 0;
	p = &act;
	for(i = 0; i < s->width; i++){
		x = 64 * i + 32;
		for(; watch < s->nlines && s->lines[watch].x0 <= x; watch++){
			if(s->lines[watch].x1 <= x || s->lines[watch].x0 == s->lines[watch].x1)
				continue;
			s->lines[watch].link = -1;
			*p = watch;
			p = &s->lines[watch].link;
		}
		s->vscanl[i] = s->nvpts;
		p = &act;
		while(j = *p, j >= 0){
			l = &s->lines[j];
			if(l->x1 <= x){
				j = l->link;
				l->link = -1;
				*p = j;
				continue;
			}
			y = l->y0 + ttfvrounddiv((vlong)(x - l->x0) * (l->y1 - l->y0), l->x1 - l->x0);
			if((s->nvpts & PTBLOCK - 1) == 0)
				s->vpts = realloc(s->vpts, (s->nvpts + PTBLOCK) * sizeof(int));
			s->vpts[s->nvpts++] = y << 1 | l->dir;
			p = &l->link;
		}
		qsort(s->vpts + s->vscanl[i], s->nvpts - s->vscanl[i], sizeof(int), intcmp);
	}
	s->vscanl[i] = s->nvpts;

}

static void
vscan(Scan *s)
{
	int i, j, k, e;
	int seen, y;
	
	for(i = 0; i < s->width; i++){
		e = s->vscanl[i+1];
		k = s->vscanl[i];
		if(k == e) continue;
		for(j = 0; j < s->height; j++){
			y = 64 * j + 32;
			seen = 0;
			while(k < e && (s->vpts[k] >> 1) <= y){
				seen |= 1<<(s->vpts[k] & 1);
				k++;
			}
			if(seen == 3 && j > 0 && iswhite(s, i, j-1) && iswhite(s, i, j)){
				if((s->flags & STUBDET) == 0){
					pixel(s, i, j-1);
					continue;
				}
				if(i <= 0 || i > s->width - 1 || j <= 0 || j > s->height - 1)
					continue;
				if(!intersectsv(s, i-1, j-1) & !intersectsh(s, i-1, j-1) & !intersectsh(s, i-1, j) | !intersectsv(s, i+1, j-1) & !intersectsh(s, i, j-1) & !intersectsh(s, i, j))
					continue;
				pixel(s, i, j-1);
			}
		}
	}
}

static void fill(Scan *s) {
  int i, j, e, match;

  for(j = 0; j < s->height; j++) {
	e = -1;
	match = 0;
	fprint(2, "\nj=%d ", j);
	for(i = 0; i < s->width; i++) {
	  if (!iswhite(s, i, j)) {
		fprint(2, "%d ", i);
		if (e == -1) {
		  e = i;
		  match = 0;
		} else {
		  if ((i - e) > 1 && !match) {
			while (++e < i) {
			  fprint(2, "%d-", e);
			  pixel(s, e, j);
			}
			fprint(2, "%d, ", i);
			match = 1;
		  }
		  e = i;
		}
	  } else if (match) {
		  e = -1;
		  match = 0;
	  }
	}
  }
}

void
ttfscan(TTGlyph *g)
{
	int i, j, c;
	TTPoint p, q, r;
	Scan s;

	memset(&s, 0, sizeof(s));
	s.g = g;
	s.flags = 0;
	c = g->font->scanctrl;
	if((c & 1<<8) != 0 && g->font->ppem <= (c & 0xff))
		s.flags |= DROPOUTS;
	if((c & 1<<11) != 0 && g->font->ppem > (c & 0xff))
		s.flags &= ~DROPOUTS;
	if((c & 3<<12) != 0)
		s.flags &= ~DROPOUTS;
	if((s.flags & DROPOUTS) != 0)
		switch(g->font->scantype){
		case 0: break;
		case 1: s.flags |= STUBDET; break;
		case 2: case 3: case 6: case 7: s.flags &= ~DROPOUTS; break;
		case 4: s.flags |= SMART; break;
		case 5: s.flags |= SMART | STUBDET; break;
		}
	
//	s.width = (g->pt[g->npt - 1].x + 63) / 64;
//	s.height = g->font->ascentpx + g->font->descentpx;
	/* 1px for overshoot */
	s.width = -g->xminpx + g->xmaxpx + 1;
	s.height = -g->yminpx + g->ymaxpx + 1;
#ifdef GREY
	s.stride = s.width;
#else
	s.stride = s.width + 7 >> 3;
#endif
	s.bit = mallocz(s.height * s.stride, 1);
	assert(s.bit != nil);
	for(i = 0; i < g->npt; i++){
		g->pt[i].x -= g->xminpx * 64;
		g->pt[i].y -= g->yminpx * 64;
//		g->pt[i].y += g->font->descentpx * 64;
	}
	for(i = 0; i < g->ncon; i++){
		if(g->confst[i] + 1 >= g->confst[i+1]) continue;
		p = g->pt[g->confst[i]];
		/* assert((p.flags & 1) != 0); */
		for(j = g->confst[i]; j++ < g->confst[i+1]; ){
			if(j < g->confst[i+1] && (g->pt[j].flags & 1) == 0)
				q = g->pt[j++];
			else
				q = p;
			if(j >= g->confst[i+1])
				r = g->pt[g->confst[i]];
			else{
				r = g->pt[j];
				if((g->pt[j].flags & 1) == 0){
					r.x = (r.x + q.x) / 2;
					r.y = (r.y + q.y) / 2;
				}
			}
			dobezier(&s, p, q, r);
			p = r;
			if(j < g->confst[i+1] && (g->pt[j].flags & 1) == 0)
				j--;
		}
	}
	/* fill(&s); */
	/* hprep(&s); */
	/* if((s.flags & DROPOUTS) != 0) */
	/* 	vprep(&s); */
	/* hscan(&s); */
	/* if((s.flags & DROPOUTS) != 0) */
	/* 	vscan(&s); */
	free(s.hpts);
	free(s.vpts);
	free(s.hscanl);
	free(s.vscanl);
	free(s.lines);
	g->bit = s.bit;
	g->width = s.width;
	g->height = s.height;
	g->stride = s.stride;
}

int
ttfgetcontour(TTGlyph *g, int i, float **fp, int *np)
{
	float offy, scale;
	float *nf;
	int n, j;
	TTPoint p, q, r;

	if((uint)i >= g->ncon)
		return 0;
	if(g->confst[i]+1 >= g->confst[i+1]){
		if(np != nil)
			*np = 0;
		if(fp != nil)
			*fp = malloc(0);
		return g->ncon - i;
	}
	if(g->bit != nil){
		scale = 1.0f / 64;
		offy = g->yminpx;
	}else{
		scale = 1.0f * g->font->ppem / g->font->u->emsize;
		offy = 0;
	}
	p = g->pt[g->confst[i]];
	n = 1;
	if(fp != nil){
		*fp = malloc(2 * sizeof(float));
		if(*fp == nil) return -1;
		(*fp)[0] = p.x * scale;
		(*fp)[1] = p.y * scale + offy;
	}
	assert((p.flags & 1) != 0);
	for(j = g->confst[i]; j++ < g->confst[i+1]; ){
		if(j < g->confst[i+1] && (g->pt[j].flags & 1) == 0)
			q = g->pt[j++];
		else
			q = p;
		if(j >= g->confst[i+1])
			r = g->pt[g->confst[i]];
		else{
			r = g->pt[j];
			if((g->pt[j].flags & 1) == 0){
				r.x = (r.x + q.x) / 2;
				r.y = (r.y + q.y) / 2;
			}
		}
		if(fp != nil){
			nf = realloc(*fp, sizeof(float) * 2 * (n + 2));
			if(nf == nil){
				free(*fp);
				return -1;
			}
			*fp = nf;
			nf[2*n] = q.x * scale;
			nf[2*n+1] = q.y * scale + offy;
			nf[2*n+2] = r.x * scale;
			nf[2*n+3] = r.y * scale + offy;
		}
		p = r;
		n += 2;
		if(j < g->confst[i+1] && (g->pt[j].flags & 1) == 0)
			j--;
	}
	if(np != nil)
		*np = n;
	return g->ncon - i;
}
