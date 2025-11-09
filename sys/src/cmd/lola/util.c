#include "inc.h"

/* Center rect s in rect r */
Rectangle
centerrect(Rectangle r, Rectangle s)
{
	int dx = (Dx(r) - Dx(s))/2;
	int dy = (Dy(r) - Dy(s))/2;
	return rectaddpt(Rect(0, 0, Dx(s), Dy(s)), Pt(r.min.x+dx, r.min.y+dy));
}

void
borderTL(Image *img, Rectangle r, Image *c)
{
	// left
	draw(img, Rect(r.min.x, r.min.y, r.min.x+1, r.max.y),
		c, nil, ZP);
	// top
	draw(img, Rect(r.min.x, r.min.y, r.max.x, r.min.y+1),
		c, nil, ZP);
}

void
borderBR(Image *img, Rectangle r, Image *c)
{
	// bottom
	draw(img, Rect(r.min.x, r.max.y-1, r.max.x, r.max.y),
		c, nil, ZP);
	// right
	draw(img, Rect(r.max.x-1, r.min.y, r.max.x, r.max.y),
		c, nil, ZP);
}

void
winborder(Image *img, Rectangle r, Image *c1, Image *c2)
{
	borderTL(img, r, c1);
	borderBR(img, r, c2);
}


void
panic(char *s)
{
	fprint(2, "error: %s: %r\n", s);
	threadexitsall("error");
}

void*
emalloc(ulong size)
{
	void *p;

	p = malloc(size);
	if(p == nil)
		panic("malloc failed");
	memset(p, 0, size);
	return p;
}

void*
erealloc(void *p, ulong size)
{
	p = realloc(p, size);
	if(p == nil)
		panic("realloc failed");
	return p;
}

char*
estrdup(char *s)
{
	char *p;

	p = malloc(strlen(s)+1);
	if(p == nil)
		panic("strdup failed");
	strcpy(p, s);
	return p;
}

/* Handle backspaces in a rune string.
 * Set number of final runes,
 * return number of runes to be deleted initially */
int
handlebs(Stringpair *pair)
{
	int initial;
	Rune *start, *rp, *wp;
	int i;

	initial = 0;
	start = rp = wp = pair->s;
	for(i = 0; i < pair->ns; i++){
		if(*rp == '\b'){
			if(wp == start)
				initial++;
			else
				wp--;
		}else
			*wp++ = *rp;
		rp++;
	}
	pair->ns = wp - start;
	return initial;
}


void
cnvsize(RuneConvBuf *cnv, int nb)
{
	cnv->nb = nb;
	if(cnv->maxbuf < nb+UTFmax){
		cnv->maxbuf = nb+UTFmax;
		cnv->buf = erealloc(cnv->buf, cnv->maxbuf);
	}
}

int
r2bfill(RuneConvBuf *cnv, Rune *rp, int nr)
{
	int i;
	for(i = 0; cnv->n < cnv->nb && i < nr; i++)
		cnv->n += runetochar(&cnv->buf[cnv->n], &rp[i]);
	return i;
}
void
r2bfinish(RuneConvBuf *cnv, Stringpair *pair)
{
	int nb;

	nb = pair->ns;
	pair->ns = min(nb, cnv->n);
	memmove(pair->s, cnv->buf, pair->ns);
	cnv->n = max(0, cnv->n-nb);
	memmove(cnv->buf, cnv->buf+nb, cnv->n);
}

// TODO: not sure about the signature of this...
// maybe pass in allocated pair?
// don't include null runes
Stringpair
b2r(RuneConvBuf *cnv)
{
	Stringpair pair;
	Rune *rp;
	int i;

	rp = runemalloc(cnv->n);
	pair.s = rp;
	pair.ns = 0;
	i = 0;
	// TODO: optimize this
	// we know there are full runes until the end
	while(fullrune(cnv->buf+i, cnv->n-i)){
		i += chartorune(rp, cnv->buf+i);
		if(*rp){
			rp++;
			pair.ns++;
		}
	}
	memmove(cnv->buf, cnv->buf+i, cnv->n-i);
	cnv->n -= i;

	return pair;
}

int
qadd(Queue *q, char *data)
{
	if(q->full)
		return 0;
	q->q[q->wi++] = data;
	q->wi %= nelem(q->q);
	q->full = q->wi == q->ri;
	return 1;
}

char*
qget(Queue *q)
{
	char *data;

	data = q->q[q->ri++];
	q->ri %= nelem(q->q);
	q->full = FALSE;
	return data;
}

int
qempty(Queue *q)
{
	return q->ri == q->wi && !q->full;
}
