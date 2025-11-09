#include "inc.h"

enum
{
	HiWater	= 640000,	/* max size of history */
	LoWater	= 400000,	/* min size of history after max'ed */
	MinWater	= 20000,	/* room to leave available when reallocating */
};

void
xinit(Text *x, Rectangle textr, Rectangle scrollr, int tabwidth, Font *ft, Image *b, Image **cols)
{
	frclear(x, FALSE);
	frinit(x, textr, ft, b, cols);
	x->maxtab = x->maxtab/8*tabwidth;
	x->i = b;
	x->scrollr = scrollr;
	x->lastsr = ZR;
	xfill(x);
	xsetselect(x, x->q0, x->q1);
	xscrdraw(x);
}

void
xsetrects(Text *x, Rectangle textr, Rectangle scrollr)
{
	frsetrects(x, textr, x->b);
	x->scrollr = scrollr;
}

void
xclear(Text *x)
{
	free(x->r);
	x->r = nil;
	x->nr = 0;
	free(x->raw);
	x->r = nil;
	x->nraw = 0;
	frclear(x, TRUE);
};

void
xredraw(Text *x)
{
	frredraw(x);
	xscrdraw(x);
}

void
xfullredraw(Text *x)
{
	xfill(x);
	x->ticked = 0;
	if(x->p0 > 0)
		frdrawsel(x, frptofchar(x, 0), 0, x->p0, 0);
	if(x->p1 < x->nchars)
		frdrawsel(x, frptofchar(x, x->p1), x->p1, x->nchars, 0);
	frdrawsel(x, frptofchar(x, x->p0), x->p0, x->p1, 1);
	x->lastsr = ZR;
	xscrdraw(x);
}

uint
xinsert(Text *w, Rune *r, int n, uint q0)
{
	uint m;

	if(n == 0)
		return q0;
	if(w->nr+n>HiWater && q0>=w->org && q0>=w->qh){
		m = min(HiWater-LoWater, min(w->org, w->qh));
		w->org -= m;
		w->qh -= m;
		if(w->q0 > m)
			w->q0 -= m;
		else
			w->q0 = 0;
		if(w->q1 > m)
			w->q1 -= m;
		else
			w->q1 = 0;
		w->nr -= m;
		runemove(w->r, w->r+m, w->nr);
		q0 -= m;
	}
	if(w->nr+n > w->maxr){
		/*
		 * Minimize realloc breakage:
		 *	Allocate at least MinWater
		 * 	Double allocation size each time
		 *	But don't go much above HiWater
		 */
		m = max(min(2*(w->nr+n), HiWater), w->nr+n)+MinWater;
		if(m > HiWater)
			m = max(HiWater+MinWater, w->nr+n);
		if(m > w->maxr){
			w->r = runerealloc(w->r, m);
			w->maxr = m;
		}
	}
	runemove(w->r+q0+n, w->r+q0, w->nr-q0);
	runemove(w->r+q0, r, n);
	w->nr += n;
	/* if output touches, advance selection, not qh; works best for keyboard and output */
	if(q0 <= w->q1)
		w->q1 += n;
	if(q0 <= w->q0)
		w->q0 += n;
	if(q0 < w->qh)
		w->qh += n;
	if(q0 < w->org)
		w->org += n;
	else if(q0 <= w->org+w->nchars)
		frinsert(w, r, r+n, q0-w->org);
	xscrdraw(w);
	return q0;
}

void
xfill(Text *w)
{
	Rune *rp;
	int i, n, m, nl;

	while(w->lastlinefull == FALSE){
		n = w->nr-(w->org+w->nchars);
		if(n == 0)
			break;
		if(n > 2000)	/* educated guess at reasonable amount */
			n = 2000;
		rp = w->r+(w->org+w->nchars);

		/*
		 * it's expensive to frinsert more than we need, so
		 * count newlines.
		 */
		nl = w->maxlines-w->nlines;
		m = 0;
		for(i=0; i<n; ){
			if(rp[i++] == '\n'){
				m++;
				if(m >= nl)
					break;
			}
		}
		frinsert(w, rp, rp+i, w->nchars);
	}
}

void
xdelete(Text *w, uint q0, uint q1)
{
	uint n, p0, p1;

	n = q1-q0;
	if(n == 0)
		return;
	runemove(w->r+q0, w->r+q1, w->nr-q1);
	w->nr -= n;
	if(q0 < w->q0)
		w->q0 -= min(n, w->q0-q0);
	if(q0 < w->q1)
		w->q1 -= min(n, w->q1-q0);
	if(q1 < w->qh)
		w->qh -= n;
	else if(q0 < w->qh)
		w->qh = q0;
	if(q1 <= w->org)
		w->org -= n;
	else if(q0 < w->org+w->nchars){
		p1 = q1 - w->org;
		if(p1 > w->nchars)
			p1 = w->nchars;
		if(q0 < w->org){
			w->org = q0;
			p0 = 0;
		}else
			p0 = q0 - w->org;
		frdelete(w, p0, p1);
		xfill(w);
	}
}

void
xsetselect(Text *w, uint q0, uint q1)
{
	int p0, p1;

	w->posx = -1;
	/* w->p0 and w->p1 are always right; w->q0 and w->q1 may be off */
	w->q0 = q0;
	w->q1 = q1;
	/* compute desired p0,p1 from q0,q1 */
	p0 = q0-w->org;
	p1 = q1-w->org;
	if(p0 < 0)
		p0 = 0;
	if(p1 < 0)
		p1 = 0;
	if(p0 > w->nchars)
		p0 = w->nchars;
	if(p1 > w->nchars)
		p1 = w->nchars;
	if(p0==w->p0 && p1==w->p1)
		return;
	/* screen disagrees with desired selection */
	if(w->p1<=p0 || p1<=w->p0 || p0==p1 || w->p1==w->p0){
		/* no overlap or too easy to bother trying */
		frdrawsel(w, frptofchar(w, w->p0), w->p0, w->p1, 0);
		frdrawsel(w, frptofchar(w, p0), p0, p1, 1);
		goto Return;
	}
	/* overlap; avoid unnecessary painting */
	if(p0 < w->p0){
		/* extend selection backwards */
		frdrawsel(w, frptofchar(w, p0), p0, w->p0, 1);
	}else if(p0 > w->p0){
		/* trim first part of selection */
		frdrawsel(w, frptofchar(w, w->p0), w->p0, p0, 0);
	}
	if(p1 > w->p1){
		/* extend selection forwards */
		frdrawsel(w, frptofchar(w, w->p1), w->p1, p1, 1);
	}else if(p1 < w->p1){
		/* trim last part of selection */
		frdrawsel(w, frptofchar(w, p1), p1, w->p1, 0);
	}

    Return:
	w->p0 = p0;
	w->p1 = p1;
}

static void
xsetorigin(Text *w, uint org, int exact)
{
	int i, a, fixup;
	Rune *r;
	uint n;

	if(org>0 && !exact){
		/* org is an estimate of the char posn; find a newline */
		/* don't try harder than 256 chars */
		for(i=0; i<256 && org<w->nr; i++){
			if(w->r[org] == '\n'){
				org++;
				break;
			}
			org++;
		}
	}
	a = org-w->org;
	fixup = 0;
	if(a>=0 && a<w->nchars){
		frdelete(w, 0, a);
		fixup = 1;	/* frdelete can leave end of last line in wrong selection mode; it doesn't know what follows */
	}else if(a<0 && -a<w->nchars){
		n = w->org - org;
		r = w->r+org;
		frinsert(w, r, r+n, 0);
	}else
		frdelete(w, 0, w->nchars);
	w->org = org;
	xfill(w);
	xscrdraw(w);
	xsetselect(w, w->q0, w->q1);
	if(fixup && w->p1 > w->p0)
		frdrawsel(w, frptofchar(w, w->p1-1), w->p1-1, w->p1, 1);
}


/*
 * Scrolling
 */

static Rectangle
scrpos(Rectangle r, uint p0, uint p1, uint tot)
{
	Rectangle q;
	int h;

	q = r;
	h = q.max.y-q.min.y;
	if(tot == 0)
		return q;
	if(tot > 1024*1024){
		tot>>=10;
		p0>>=10;
		p1>>=10;
	}
	if(p0 > 0)
		q.min.y += h*p0/tot;
	if(p1 < tot)
		q.max.y -= h*(tot-p1)/tot;
	if(q.max.y < q.min.y+2){
		if(q.min.y+2 <= r.max.y)
			q.max.y = q.min.y+2;
		else
			q.min.y = q.max.y-2;
	}
	return q;
}

void
xscrdraw(Text *w)
{
	Rectangle r1, r2;

	if(w->i == nil)
		return;

	r1 = w->scrollr;
	r2 = scrpos(r1, w->org, w->org+w->nchars, w->nr);
	if(!eqrect(r2, w->lastsr)){
		w->lastsr = r2;
		draw(w->i, r1, w->cols[BORD], nil, ZP);
		draw(w->i, insetrect(r2,1), w->cols[BACK], nil, ZP);
	}
}

static uint
xbacknl(Text *w, uint p, uint n)
{
	int i, j;

	/* look for start of this line if n==0 */
	if(n==0 && p>0 && w->r[p-1]!='\n')
		n = 1;
	i = n;
	while(i-->0 && p>0){
		--p;	/* it's at a newline now; back over it */
		if(p == 0)
			break;
		/* at 128 chars, call it a line anyway */
		for(j=128; --j>0 && p>0; p--)
			if(w->r[p-1]=='\n')
				break;
	}
	return p;
}

static void
xscrsleep(Mousectl *mc, uint dt)
{
	Timer	*timer;
	int y, b;
	static Alt alts[3];

	if(display->bufp > display->buf)
		flushimage(display, 1);
	timer = timerstart(dt);
	y = mc->xy.y;
	b = mc->buttons;
	alts[0] = ALT(timer->c, nil, CHANRCV);
	alts[1] = ALT(mc->c, &mc->Mouse, CHANRCV);
	alts[2].op = CHANEND;
	for(;;)
		switch(alt(alts)){
		case 0:
			timerstop(timer);
			return;
		case 1:
			if(abs(mc->xy.y-y)>2 || mc->buttons!=b){
				timercancel(timer);
				return;
			}
			break;
		}
}

void
xscroll(Text *w, Mousectl *mc, int but)
{
	uint p0, oldp0;
	Rectangle s;
	int y, my, h, first;

	s = insetrect(w->scrollr, 1);
	h = s.max.y-s.min.y;
	oldp0 = ~0;
	first = TRUE;
	do{
		my = mc->xy.y;
		if(my < s.min.y)
			my = s.min.y;
		if(my >= s.max.y)
			my = s.max.y;
		if(but == 2){
			y = my;
			if(y > s.max.y-2)
				y = s.max.y-2;
			if(w->nr > 1024*1024)
				p0 = ((w->nr>>10)*(y-s.min.y)/h)<<10;
			else
				p0 = w->nr*(y-s.min.y)/h;
			if(oldp0 != p0)
				xsetorigin(w, p0, FALSE);
			oldp0 = p0;
			readmouse(mc);
			continue;
		}
		if(but == 1 || but == 4){
			y = max(1, (my-s.min.y)/w->font->height);
			p0 = xbacknl(w, w->org, y);
		}else{
			y = max(my, s.min.y+w->font->height);
			p0 = w->org+frcharofpt(w, Pt(s.max.x, y));
		}
		if(oldp0 != p0)
			xsetorigin(w, p0, TRUE);
		oldp0 = p0;
		/* debounce */
		if(first){
			if(display->bufp > display->buf)
				flushimage(display, 1);
			if(but > 3)
				return;
			sleep(200);
			nbrecv(mc->c, &mc->Mouse);
			first = FALSE;
		}
		xscrsleep(mc, 100);
	}while(mc->buttons & (1<<(but-1)));
	while(mc->buttons)
		readmouse(mc);
}

void
xscrolln(Text *x, int n)
{
	uint q0;

	if(n < 0)
		q0 = xbacknl(x, x->org, -n);
	else
		q0 = x->org+frcharofpt(x, Pt(x->Frame.r.min.x, x->Frame.r.min.y+n*x->font->height));
	xsetorigin(x, q0, TRUE);
}

static Text	*selecttext;
static Mousectl *selectmc;
static uint	selectq;

static void
xframescroll(Text *x, int dl)
{
	uint endq;

	if(dl == 0){
		xscrsleep(selectmc, 100);
		return;
	}
	if(dl < 0){
		endq = x->org+x->p0;
	}else{
		if(x->org+x->nchars == x->nr)
			return;
		endq = x->org+x->p1;
	}
	xscrolln(x, dl);
	xsetselect(x, min(selectq, endq), max(selectq, endq));
}

static void
framescroll(Frame *f, int dl)
{
	if(f != &selecttext->Frame)
		panic("frameselect not right frame");
	xframescroll(selecttext, dl);
}

/*
 * Selection and deletion helpers
 */

int
iswordrune(Rune r)
{
	return r == '_' || isalpharune(r) || isdigitrune(r);
}

static int
xbswidth(Text *w, Rune c)
{
	uint q, stop;
	Rune r;
	int wd, inword;

	/* there is known to be at least one character to erase */
	if(c == Kbs)	/* ^H: erase character */
		return 1;
	q = w->q0;
	stop = 0;
	if(q > w->qh)
		stop = w->qh;
	inword = FALSE;
	while(q > stop){
		r = w->r[q-1];
		if(r == '\n'){		/* eat at most one more character */
			if(q == w->q0)	/* eat the newline */
				--q;
			break; 
		}
		/* ^W: erase word.
		 * delete a bunch of non-word characters
		 * followed by word characters */
		if(c == CTRL('W')){
			wd = iswordrune(r);
			if(wd && !inword)
				inword = TRUE;
			else if(!wd && inword)
				break;
		}
		--q;
	}
	return w->q0-q;
}

static Rune left1[] =  { L'{', L'[', L'(', L'<', L'«', 0 };
static Rune right1[] = { L'}', L']', L')', L'>', L'»', 0 };
static Rune left2[] =  { L'\n', 0 };
static Rune left3[] =  { L'\'', L'"', L'`', 0 };

static Rune *left[] = {
	left1,
	left2,
	left3,
	nil
};
static Rune *right[] = {
	right1,
	left2,
	left3,
	nil
};

static int
xclickmatch(Text *x, int cl, int cr, int dir, uint *q)
{
	Rune c;
	int nest;

	nest = 1;
	for(;;){
		if(dir > 0){
			if(*q == x->nr)
				break;
			c = x->r[*q];
			(*q)++;
		}else{
			if(*q == 0)
				break;
			(*q)--;
			c = x->r[*q];
		}
		if(c == cr){
			if(--nest==0)
				return 1;
		}else if(c == cl)
			nest++;
	}
	return cl=='\n' && nest==1;
}

static int
inmode(Rune r, int mode)
{
	return (mode == 1) ? iswordrune(r) : r && !isspacerune(r);
}

static void
xstretchsel(Text *x, uint pt, uint *q0, uint *q1, int mode)
{
	int c, i;
	Rune *r, *l, *p;
	uint q;

	*q0 = pt;
	*q1 = pt;
	for(i=0; left[i]!=nil; i++){
		q = *q0;
		l = left[i];
		r = right[i];
		/* try matching character to left, looking right */
		if(q == 0)
			c = '\n';
		else
			c = x->r[q-1];
		p = runestrchr(l, c);
		if(p != nil){
			if(xclickmatch(x, c, r[p-l], 1, &q))
				*q1 = q-(c!='\n');
			return;
		}
		/* try matching character to right, looking left */
		if(q == x->nr)
			c = '\n';
		else
			c = x->r[q];
		p = runestrchr(r, c);
		if(p != nil){
			if(xclickmatch(x, c, l[p-r], -1, &q)){
				*q1 = *q0+(*q0<x->nr && c=='\n');
				*q0 = q;
				if(c!='\n' || q!=0 || x->r[0]=='\n')
					(*q0)++;
			}
			return;
		}
	}
	/* try filling out word to right */
	while(*q1<x->nr && inmode(x->r[*q1], mode))
		(*q1)++;
	/* try filling out word to left */
	while(*q0>0 && inmode(x->r[*q0-1], mode))
		(*q0)--;
}

static Mouse	lastclick;
static Text	*clickfrm;
static uint	clickcount;

/* should be called with button 1 down */
void
xselect(Text *x, Mousectl *mc)
{
	uint q0, q1;
	int dx, dy, dt, b;

	/* reset click state if mouse is too different from last time */
	dx = abs(mc->xy.x - lastclick.xy.x);
	dy = abs(mc->xy.y - lastclick.xy.y);
	dt = mc->msec - lastclick.msec;
	if(x != clickfrm || dx > 3 || dy > 3 || dt >= 500)
		clickcount = 0;

	/* first button down can be a dragging selection or a click.
	 * subsequent buttons downs can only be clicks.
	 * both cases can be ended by chording. */
	selectq = x->org+frcharofpt(x, mc->xy);
	if(clickcount == 0){
		/* what a kludge - can this be improved? */
		selecttext = x;
		selectmc = mc;
		x->scroll = framescroll;
		frselect(x, mc);
		/* this is correct if the whole selection is visible */
		q0 = x->org + x->p0;
		q1 = x->org + x->p1;
		/* otherwise replace one end with selectq */
		if(selectq < x->org)
			q0 = selectq;
		if(selectq > x->org+x->nchars)
			q1 = selectq;
		xsetselect(x, q0, q1);

		/* figure out whether it was a click */
		if(q0 == q1 && mc->buttons == 0){
			clickcount = 1;
			clickfrm = x;
		}
	}else{
		clickcount++;
		xstretchsel(x, selectq, &q0, &q1, min(clickcount-1, 2));
		xsetselect(x, q0, q1);
		if(clickcount >= 3)
			clickcount = 0;
		b = mc->buttons;
		while(mc->buttons == b)
			readmouse(mc);
	}
	lastclick = mc->Mouse;		/* a bit unsure if this is correct */

	/* chording */
	while(mc->buttons){
		clickcount = 0;
		b = mc->buttons;
		if(b & 6){
			if(b & 2){
				xsnarf(x);
				xcut(x);
			}else{
				xpaste(x);
			}
		}
		while(mc->buttons == b)
			readmouse(mc);
	}
}

void
xshow(Text *w, uint q0)
{
	int qe;
	int nl;
	uint q;

	qe = w->org+w->nchars;
	if(w->org<=q0 && (q0<qe || (q0==qe && qe==w->nr)))
		xscrdraw(w);
	else{
		nl = 4*w->maxlines/5;
		q = xbacknl(w, q0, nl);
		/* avoid going backwards if trying to go forwards - long lines! */
		if(!(q0>w->org && q<w->org))
			xsetorigin(w, q, TRUE);
		while(q0 > w->org+w->nchars)
			xsetorigin(w, w->org+1, FALSE);
	}
}

void
xplacetick(Text *x, uint q)
{
	xsetselect(x, q, q);
	xshow(x, q);
}

void
xtype(Text *x, Rune r)
{
	uint q0, q1;
	int nb;

	xsnarf(x);
	xcut(x);
	switch(r){
	case CTRL('H'):	/* erase character */
	case CTRL('W'):	/* erase word */
	case CTRL('U'):	/* erase line */
		if(x->q0==0 || x->q0==x->qh)
			return;
		nb = xbswidth(x, r);
		q1 = x->q0;
		q0 = q1-nb;
		if(q0 < x->org){
			q0 = x->org;
			nb = q1-q0;
		}
		if(nb > 0){
			xdelete(x, q0, q0+nb);
			xsetselect(x, q0, q0);
		}
		break;
	default:
		xinsert(x, &r, 1, x->q0);
		xshow(x, x->q0);
		break;
	}
}

int
xninput(Text *x)
{
	uint q;
	Rune r;

	for(q = x->qh; q < x->nr; q++){
		r = x->r[q];
		if(r == '\n')
			return q - x->qh + 1;
		if(r == CTRL('D'))
			return q - x->qh;
	}
	return -1;
}

void
xaddraw(Text *x, Rune *r, int nr)
{
	x->raw = runerealloc(x->raw, x->nraw+nr);
	runemove(x->raw+x->nraw, r, nr);
	x->nraw += nr;
}

/* TODO: maybe pass what we're looking for in a string */
void
xlook(Text *x)
{
	int i, n, e;

	i = x->q1;
	n = i - x->q0;
	e = x->nr - n;
	if(n <= 0 || e < n)
		return;

	if(i > e)
		i = 0;

	while(runestrncmp(x->r+x->q0, x->r+i, n) != 0){
		if(i < e)
			i++;
		else
			i = 0;
	}

	xsetselect(x, i, i+n);
	xshow(x, i);
}

Rune *snarf;
int nsnarf;
int snarfversion;
int snarffd;

void
xsnarf(Text *x)
{
	if(x->q1 == x->q0)
		return;
	nsnarf = x->q1-x->q0;
	snarf = runerealloc(snarf, nsnarf);
	snarfversion++;
	runemove(snarf, x->r+x->q0, nsnarf);
	putsnarf();
}

void
xcut(Text *x)
{
	if(x->q1 == x->q0)
		return;
	xdelete(x, x->q0, x->q1);
	xsetselect(x, x->q0, x->q0);
	xscrdraw(x);
}

void
xpaste(Text *x)
{
	uint q0;

	getsnarf();
	if(nsnarf == 0)
		return;
	xcut(x);
	q0 = x->q0;
	if(x->rawmode && q0==x->nr){
		xaddraw(x, snarf, nsnarf);
		xsetselect(x, q0, q0);
	}else{
		q0 = xinsert(x, snarf, nsnarf, x->q0);
		xsetselect(x, q0, q0+nsnarf);
	}
	xscrdraw(x);
}

void
xsend(Text *x)
{
	getsnarf();
	xsnarf(x);
	if(nsnarf == 0)
		return;
	if(x->rawmode){
		xaddraw(x, snarf, nsnarf);
		if(snarf[nsnarf-1]!='\n' && snarf[nsnarf-1]!=CTRL('D'))
			xaddraw(x, L"\n", 1);
	}else{
		xinsert(x, snarf, nsnarf, x->nr);
		if(snarf[nsnarf-1]!='\n' && snarf[nsnarf-1]!=CTRL('D'))
			xinsert(x, L"\n", 1, x->nr);
	}
	xplacetick(x, x->nr);
}

int
xplumb(Text *w, char *src, char *dir, int maxsize)
{
	Plumbmsg *m;
	static int fd = -2;
	char buf[32];
	uint p0, p1;

	if(fd == -2)
		fd = plumbopen("send", OWRITE|OCEXEC);
	if(fd < 0)
		return 0;
	m = emalloc(sizeof(Plumbmsg));
	m->src = estrdup(src);
	m->dst = nil;
	m->wdir = estrdup(dir);
	m->type = estrdup("text");
	p0 = w->q0;
	p1 = w->q1;
	if(w->q1 > w->q0)
		m->attr = nil;
	else{
		while(p0>0 && w->r[p0-1]!=' ' && w->r[p0-1]!='\t' && w->r[p0-1]!='\n')
			p0--;
		while(p1<w->nr && w->r[p1]!=' ' && w->r[p1]!='\t' && w->r[p1]!='\n')
			p1++;
		snprint(buf, sizeof(buf), "click=%d", w->q0-p0);
		m->attr = plumbunpackattr(buf);
	}
	if(p1-p0 > maxsize){
		plumbfree(m);
		return 0;	/* too large for 9P */
	}
	m->data = smprint("%.*S", p1-p0, w->r+p0);
	m->ndata = strlen(m->data);
	if(plumbsend(fd, m) < 0){
		plumbfree(m);
		return 1;
	}
	plumbfree(m);
	return 0;
}
