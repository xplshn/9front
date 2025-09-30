#include "inc.h"

int tabwidth;
bool scrolling;
bool notitle;
int ndeskx = 3;
int ndesky = 3;

RKeyboardctl *kbctl;
Mousectl *mctl;
char *startdir;
bool shiftdown, ctldown;
bool gotscreen;
bool servekbd;

Screen *wscreen;
Image *fakebg;

Channel *pickchan;

void
killprocs(void)
{
	int i;

	for(i = 0; i < nwintabs; i++)
		if(wintabs[i]->notefd >= 0)
			write(wintabs[i]->notefd, "hangup", 6);
}

static char *oknotes[] ={
	"delete",
	"hangup",
	"kill",
	"exit",
	nil,	// for debugging
	nil
};

int
notehandler(void*, char *msg)
{
	int i;

	killprocs();
	for(i = 0; oknotes[i]; i++)
		if(strncmp(oknotes[i], msg, strlen(oknotes[i])) == 0)
			threadexitsall(msg);
	fprint(2, "lola %d: abort: %s\n", getpid(), msg);
	abort();
}

/*
 * /dev/snarf updates when the file is closed, so we must open our own
 * fd here rather than use snarffd
 */
void
putsnarf(void)
{
	int fd, i, n;

	if(snarffd<0 || nsnarf==0)
		return;
	fd = open("/dev/snarf", OWRITE|OCEXEC);
	if(fd < 0)
		return;
	/* snarf buffer could be huge, so fprint will truncate; do it in blocks */
	for(i=0; i<nsnarf; i+=n){
		n = nsnarf-i;
		if(n >= 256)
			n = 256;
		if(fprint(fd, "%.*S", n, snarf+i) < 0)
			break;
	}
	close(fd);
}

void
setsnarf(char *s, int ns)
{
	free(snarf);
	snarf = runesmprint("%.*s", ns, s);
	nsnarf = runestrlen(snarf);
	snarfversion++;
}

void
getsnarf(void)
{
	int i, n;
	char *s, *sn;

	if(snarffd < 0)
		return;
	sn = nil;
	i = 0;
	seek(snarffd, 0, 0);
	for(;;){
		if(i > MAXSNARF)
			break;
		if((s = realloc(sn, i+1024+1)) == nil)
			break;
		sn = s;
		if((n = read(snarffd, sn+i, 1024)) <= 0)
			break;
		i += n;
	}
	if(i == 0)
		return;
	sn[i] = 0;
	setsnarf(sn, i);
	free(sn);
}

static int overridecursor;
static Cursor *ovcursor;
static Cursor *normalcursor;
Cursor *cursor;

void
setmousecursor(Cursor *c)
{
	if(cursor == c)
		return;
	cursor = c;
	setcursor(mctl, c);
}

void
setcursoroverride(Cursor *c, int ov)
{
	overridecursor = ov;
	ovcursor = c;
	setmousecursor(overridecursor ? ovcursor : normalcursor);
}

void
setcursornormal(Cursor *c)
{
	normalcursor = c;
	setmousecursor(overridecursor ? ovcursor : normalcursor);
}

char *rcargv[] = { "rc", "-i", nil };

WinTab*
new(Rectangle r)
{
	WinTab *w;

	w = wtcreate(r, FALSE, scrolling);
	assert(w);
	if(wincmd(w, 0, nil, rcargv) == 0)
		return nil;
	return w;
}

WinTab*
newtab(Window *ww)
{
	WinTab *w;

	w = tcreate(ww, scrolling);
	assert(w);
	if(wincmd(w, 0, nil, rcargv) == 0)
		return nil;
	return w;
}


void
drainmouse(Mousectl *mc, WinTab *w)
{
	if(w) send(w->mc.c, &mc->Mouse);
	while(mc->buttons){
		readmouse(mc);
		/* stop sending once focus changes.
		 * buttons released in wfocus() */
		if(w && w->w != focused) w = nil;
		if(w) send(w->mc.c, &mc->Mouse);
	}
}

Window*
clickwindow(int but, Mousectl *mc)
{
	Window *w;

	but = 1<<(but-1);
	setcursoroverride(&sightcursor, TRUE);
	drainmouse(mc, nil);
	while(!(mc->buttons & but)){
		readmouse(mc);
		if(mc->buttons & (7^but)){
			setcursoroverride(nil, FALSE);
			drainmouse(mc, nil);
			return nil;
		}
	}
	w = wpointto(mc->xy);
	return w;
}

Rectangle
dragrect(int but, Rectangle r, Mousectl *mc)
{
	Rectangle rc;
	Point start, end;

	but = 1<<(but-1);
	setcursoroverride(&boxcursor, TRUE);
	start = mc->xy;
	end = mc->xy;
	do{
		rc = rectaddpt(r, subpt(end, start));
		drawgetrect(rc, 1);
		readmouse(mc);
		drawgetrect(rc, 0);
		end = mc->xy;
	}while(mc->buttons == but);

	setcursoroverride(nil, FALSE);
	if(mc->buttons & (7^but)){
		rc.min.x = rc.max.x = 0;
		rc.min.y = rc.max.y = 0;
		drainmouse(mc, nil);
	}
	return rc;
}

Rectangle
sweeprect(int but, Mousectl *mc)
{
	Rectangle r, rc;

	but = 1<<(but-1);
	setcursoroverride(&crosscursor, TRUE);
	drainmouse(mc, nil);
	while(!(mc->buttons & but)){
		readmouse(mc);
		if(mc->buttons & (7^but))
			goto Return;
	}
	r.min = mc->xy;
	r.max = mc->xy;
	do{
		rc = canonrect(r);
		drawgetrect(rc, 1);
		readmouse(mc);
		drawgetrect(rc, 0);
		r.max = mc->xy;
	}while(mc->buttons == but);

    Return:
	setcursoroverride(nil, FALSE);
	if(mc->buttons & (7^but)){
		rc.min.x = rc.max.x = 0;
		rc.min.y = rc.max.y = 0;
		drainmouse(mc, nil);
	}
	return rc;
}

int
whichside(int x, int lo, int hi)
{
	return	x < lo+20 ? 0 :
		x > hi-20 ? 2 :
		1;
}

/* 0 1 2
 * 3   5
 * 6 7 8 */
int
whichcorner(Rectangle r, Point p)
{
	int i, j;
	
	i = whichside(p.x, r.min.x, r.max.x);
	j = whichside(p.y, r.min.y, r.max.y);
	return 3*j+i;
}

/* replace corner or edge of rect with point */
Rectangle
changerect(Rectangle r, int corner, Point p)
{
	switch(corner){
	case 0: return Rect(p.x, p.y, r.max.x, r.max.y);
	case 1: return Rect(r.min.x, p.y, r.max.x, r.max.y);
	case 2: return Rect(r.min.x, p.y, p.x+1, r.max.y);
	case 3: return Rect(p.x, r.min.y, r.max.x, r.max.y);
	case 5: return Rect(r.min.x, r.min.y, p.x+1, r.max.y);
	case 6: return Rect(p.x, r.min.y, r.max.x, p.y+1);
	case 7: return Rect(r.min.x, r.min.y, r.max.x, p.y+1);
	case 8: return Rect(r.min.x, r.min.y, p.x+1, p.y+1);
	}
	return r;
}

Rectangle
bandrect(Rectangle r, int but, Mousectl *mc)
{
	Rectangle or, nr;
	int corner, ncorner;

	or = r;
	corner = whichcorner(r, mc->xy);
	setcursornormal(corners[corner]);

	do{
		drawgetrect(r, 1);
		readmouse(mc);
		drawgetrect(r, 0);
		nr = canonrect(changerect(r, corner, mc->xy));
		if(goodrect(nr))
			r = nr;
		ncorner = whichcorner(r, mc->xy);
		/* can switch from edge to corner, but not vice versa */
		if(ncorner != corner && ncorner != 4 && (corner|~ncorner) & 1){
			corner = ncorner;
			setcursornormal(corners[corner]);
		}
	}while(mc->buttons == but);

	if(mc->buttons){
		drainmouse(mctl, nil);
		return or;
	}

	setcursornormal(nil);
	return r;
}

Window*
pick(void)
{
	Window *w1, *w2;

	w1 = clickwindow(3, mctl);
	drainmouse(mctl, nil);
	setcursoroverride(nil, FALSE);
	w2 = wpointto(mctl->xy);
	if(w1 != w2)
		return nil;
	return w1;
}

void
grab(Window *w, int btn)
{
	if(w == nil)
		w = clickwindow(btn, mctl);
	if(w == nil)
		setcursoroverride(nil, FALSE);
	else{
		Rectangle r = dragrect(btn, w->frame->r, mctl);
		if((Dx(r) > 0 || Dy(r) > 0) && !eqrect(r, w->frame->r)){
			wmove(w, r.min);
			wfocus(w);
			flushimage(display, 1);
		}
	}
}

void
sweep(Window *w)
{
	Rectangle r = sweeprect(3, mctl);
	if(goodrect(r)){
		if(w){
			wresize(w, r);
			wraise(w);
			wfocus(w);
		}else{
			new(r);
		}
		flushimage(display, 1);
	}

//TODO(tab): temp hack
	else{
		Window *ww = wpointto(r.min);
		if(w == nil && ww)
			newtab(ww);
	}
}

void
bandresize(Window *w)
{
	Rectangle r;
	r = bandrect(w->frame->r, mctl->buttons, mctl);
	if(!eqrect(r, w->frame->r)){
		wresize(w, r);
		flushimage(display, 1);
	}
}

int
obscured(Window *w, Rectangle r, Window *t)
{
	if(Dx(r) < font->height || Dy(r) < font->height)
		return 1;
	if(!rectclip(&r, screen->r))
		return 1;
	for(; t; t = t->higher){
		if(t->hidden || Dx(t->frame->r) == 0 || Dy(t->frame->r) == 0 || rectXrect(r, t->frame->r) == 0)
			continue;
		if(r.min.y < t->frame->r.min.y)
			if(!obscured(w, Rect(r.min.x, r.min.y, r.max.x, t->frame->r.min.y), t))
				return 0;
		if(r.min.x < t->frame->r.min.x)
			if(!obscured(w, Rect(r.min.x, r.min.y, t->frame->r.min.x, r.max.y), t))
				return 0;
		if(r.max.y > t->frame->r.max.y)
			if(!obscured(w, Rect(r.min.x, t->frame->r.max.y, r.max.x, r.max.y), t))
				return 0;
		if(r.max.x > t->frame->r.max.x)
			if(!obscured(w, Rect(t->frame->r.max.x, r.min.y, r.max.x, r.max.y), t))
				return 0;
		return 1;
	}
	return 0;
}

/* Check that newly created window will be of manageable size */
int
goodrect(Rectangle r)
{
	if(badrect(r) || !eqrect(canonrect(r), r))
		return 0;
	/* reasonable sizes only please */
	if(Dx(r) > BIG*Dx(screen->r))
		return 0;
	if(Dy(r) > BIG*Dy(screen->r))
		return 0;
	/*
	 * the height has to be big enough to fit one line of text.
	 * that includes the border on each side with an extra pixel
	 * so that the text is still drawn
	 */
	if(Dx(r) < 100 || Dy(r) < 2*(bordersz+1)+font->height)
		return 0;
//TODO(vdesk) this changes
	/* window must be on screen */
	if(!rectXrect(screen->r, r))
		return 0;
	/* must have some screen and border visible so we can move it out of the way */
	if(rectinrect(screen->r, insetrect(r, bordersz)))
		return 0;
	return 1;
}

/* Rectangle for new window */
Rectangle
newrect(void)
{
	static int i = 0;
	int minx, miny, dx, dy;

	dx = min(600, Dx(screen->r) - 2*bordersz);
	dy = min(400, Dy(screen->r) - 2*bordersz);
	minx = 32 + 16*i;
	miny = 32 + 16*i;
	i++;
	i %= 10;

	return Rect(minx, miny, minx+dx, miny+dy);
}


void
btn2menu(WinTab *w)
{
	enum {
		Cut,
		Paste,
		Snarf,
		Plumb,
		Look,
		Send,
		Scroll
	};
	static char *str[] = {
		"cut",
		"paste",
		"snarf",
		"plumb",
		"look",
		"send",
		"scroll",
		nil
	};
	static Menu menu = { str };

	int sel;
	Text *x;
	Cursor *c;

	x = &w->text;
	str[Scroll] = w->scrolling ? "noscroll" : "scroll";
	sel = menuhit(2, mctl, &menu, wscreen);
	switch(sel){
	case Cut:
		xsnarf(x);
		xcut(x);
		break;
	case Paste:
		xpaste(x);
		break;
	case Snarf:
		xsnarf(x);
		break;
	case Plumb:
		if(xplumb(x, "lola", w->dir, fsys.msize-1024)){
			c = cursor;
			setcursoroverride(&query, TRUE);
			sleep(300);
			setcursoroverride(c, FALSE);
		}
		break;
	case Look:
		xlook(x);
		break;
	case Send:
		xsend(x);
		break;
	case Scroll:
		w->scrolling = !w->scrolling;
		if(w->scrolling)
			xshow(x, x->nr);
		break;
	}
	wsendmsg(w, Wakeup);
}

void
btn3menu(void)
{
	enum {
		New,
		Reshape,
		Move,
		Delete,
		Hide,
		Hidden
	};
	static char *str[Hidden+1 + MAXWINDOWS] = {
		"New",
		"Resize",
		"Move",
		"Delete",
		"Hide",
		nil
	};
	static Menu menu = { str };

	static Window *hidden[MAXWINDOWS];
	int nhidden;
	Window *w, *t;
	int i, sel;

	nhidden = 0;
	for(i = 0; i < nwindows; i++){
		t = windows[i];
		if(!rectXrect(screen->r, t->frame->r))
			continue;
		if(t->hidden || obscured(t, t->frame->r, t->higher)){
			hidden[nhidden] = windows[i];
			str[nhidden+Hidden] = windows[i]->cur->label;
			nhidden++;	
		}
	}
	str[nhidden+Hidden] = nil;

	sel = menuhit(3, mctl, &menu, wscreen);
	switch(sel){
	case New:
		sweep(nil);
		break;
	case Reshape:
		w = pick();
		if(w) sweep(w);
		break;
	case Move:
		grab(nil, 3);
		break;
	case Delete:
		w = pick();
		if(w) wdelete(w);
		break;
	case Hide:
		w = pick();
		if(w) whide(w);
		break;
	default:
		if(sel >= Hidden){
			w = hidden[sel-Hidden];
			if(w->hidden)
				wunhide(w);
			else{
				wraise(w);
				wfocus(w);
			}
		}
		break;
	}
}

void
btn13menu(void)
{
	enum {
		RefreshScreen,
		Scroll,
		Title,
		Exit
	};
	static char *str[] = {
		"Refresh",
		"Scroll",
		"Title",
		"Exit",
		nil
	};
	static Menu menu = { str };

	str[Scroll] = scrolling ? "!Scroll" : "Scroll";
	str[Title] = notitle ? "Title" : "!Title";
	switch(menuhit(3, mctl, &menu, wscreen)){
	case RefreshScreen:
		refresh();
		break;
	case Scroll:
		scrolling = !scrolling;
		break;
	case Title:
		notitle = !notitle;
		break;
	case Exit:
		killprocs();
		threadexitsall(nil);
	}
}

void
btn12menu(void)
{
	int dx, dy, i, j;

	dx = Dx(screen->r);
	dy = Dy(screen->r);
	i = screenoff.x/dx;
	j = screenoff.y/dy;
	Point ssel = dmenuhit(2, mctl, ndeskx, ndesky, Pt(i,j));
	if(ssel.x >= 0 && ssel.y >= 0 && 
	   (ssel.x*dx != screenoff.x || ssel.y*dy != screenoff.y))
		screenoffset(ssel.x*dx, ssel.y*dy);
}

static void
wtabctl(Window *w)
{
	Rectangle r;

	if(mctl->buttons & 7){
		wraise(w);
		wfocus(w);
		r = w->tabrect;
		int n = w->ref;
		if(n > 1){
			int wd = Dx(r)/n;
			r.max.x = r.min.x + wd;
			for(WinTab *t = w->tab; t; t = t->next){
				if(ptinrect(mctl->xy, r)){
					if(mctl->buttons & 1){
						tfocus(t);
						/* chording */
						while(mctl->buttons){
							int b = mctl->buttons;
							if(b & 6){
								if(b & 2)
									tmoveleft(t);
								else
									tmoveright(t);
							}
							while(mctl->buttons == b)
								readmouse(mctl);
						}
					}else if(mctl->buttons & 2){
						tdelete(t);
						while(mctl->buttons)
							readmouse(mctl);
					}else if(mctl->buttons & 4){
						Point pt = mctl->xy;
						Window *ww = pick();
						if(ww){
							/* move tab into clicked window */
							tmigrate(t, ww);
							wraise(ww);
							wfocus(ww);
						}else{
							/* HACK: pick doesn't say whether we cancelled
							 * or clicked background */
							ww = wpointto(mctl->xy);
							if(ww == nil){
								r = rectaddpt(w->rect, subpt(mctl->xy, pt));
								ww = wcreate(r, 0);
								tmigrate(t, ww);
							}
						}
						return;
					}
					break;
				}
				r = rectaddpt(r, Pt(wd,0));
			}
		}
	}
}

void
mthread(void*)
{
	Window *w;
	Channel *wc;

	threadsetname("mousethread");
	enum { Amouse, Apick, NALT };
	Alt alts[NALT+1] = {
		[Amouse]	{.c = mctl->c, .v = &mctl->Mouse, .op = CHANRCV},
		[Apick]		{.c = pickchan, .v = &wc, .op = CHANRCV},
		[NALT]		{.op = CHANEND},
	};
	for(;;){
		// normally done in readmouse
		Display *d = mctl->image->display;
		if(d->bufp > d->buf)
			flushimage(d, 1);
		switch(alt(alts)){
		case Apick:
			sendp(wc, pick());
			break;
		case Amouse:
			w = wpointto(mctl->xy);
			cursorwin = w;
again:
			if(w == nil){
				/* background */
				setcursornormal(nil);
				while(mctl->buttons & 1){
					if(mctl->buttons & 2)
						btn12menu();
					else if(mctl->buttons & 4)
						btn13menu();
					readmouse(mctl);
				}
				if(mctl->buttons & 4)
					btn3menu();
			}else if(!ptinrect(mctl->xy, w->contrect)){
				/* decoration */
				if(!w->noborder &&
				   !ptinrect(mctl->xy, insetrect(w->frame->r, bordersz))){
					/* border */
					setcursornormal(corners[whichcorner(w->frame->r, mctl->xy)]);
					if(mctl->buttons & 7){
						wraise(w);
						wfocus(w);
						if(mctl->buttons & 4)
							grab(w, 3);
						if(mctl->buttons & 3)
							bandresize(w);
					}
				}else{
					/* title bar */
					setcursornormal(nil);
					if(ptinrect(mctl->xy, w->titlerect))
						wtitlectl(w);
					else
						wtabctl(w);
				}
			}else if(w->cur == nil){
				/* no tab in window */
			}else if(w != focused){
				/* inactive window */
				wsetcursor(w->cur);
				if(mctl->buttons & 7 ||
				   mctl->buttons & (8|16) && focused && focused->cur->mouseopen){
					wraise(w);
					wfocus(w);
					if(mctl->buttons & 1)
						drainmouse(mctl, nil);
					else
						goto again;
				}
			}else if(!w->cur->mouseopen){
				/* active text window */
				wsetcursor(w->cur);
				if(mctl->buttons && topwin != w)
					wraise(w);
				if(mctl->buttons & (1|8|16) || ptinrect(mctl->xy, w->cur->text.scrollr))
					drainmouse(mctl, w->cur);
				if(mctl->buttons & 2){
					incref(w->cur);
					btn2menu(w->cur);
					wrelease(w->cur);
				}
				if(mctl->buttons & 4)
					btn3menu();
			}else{
				/* active graphics window */
				wsetcursor(w->cur);
				drainmouse(mctl, w->cur);
			}
		}
	}
}

void
resthread(void*)
{
	Window *w;
	Rectangle or, nr;
	Point delta;

	threadsetname("resizethread");
	for(;;){
		recvul(mctl->resizec);
		or = screen->clipr;
		if(getwindow(display, Refnone) < 0)
			sysfatal("resize failed: %r");
		nr = screen->clipr;

		freeimage(fakebg);
		freescreen(wscreen);
		wscreen = allocscreen(screen, background, 0);
		fakebg = allocwindow(wscreen, screen->r, Refbackup, DNofill);
		draw(fakebg, fakebg->r, background, nil, ZP);

		delta = subpt(nr.min, or.min);
		for(w = bottomwin; w; w = w->higher){
			if(w->maximized){
				wrestore(w);
				wresize(w, rectaddpt(w->frame->r, delta));
				wmaximize(w);
			}else
				wresize(w, rectaddpt(w->frame->r, delta));
		}

		flushimage(display, 1);
	}
}

void
refresh(void)
{
	Window *w;

	draw(fakebg, fakebg->r, background, nil, ZP);
	for(w = bottomwin; w; w = w->higher){
		if(w->maximized){
			wrestore(w);
			wresize(w, w->frame->r);
			wmaximize(w);
		}else
			wresize(w, w->frame->r);
	}
}

/*
 *    kbd    -----+-------> to tap
 *                 \
 *                  \
 * from tap  --------+----> window
 */

Channel *opentap;	/* open fromtap or totap */
Channel *closetap;	/* close fromtap or totap */
Channel *fromtap;	/* input from kbd tap program to window */
Channel *totap;		/* our keyboard input to tap program */

void
keyboardtap(void*)
{
	char *s, *z;
	Channel *fschan, *chan;
	int n;
	Stringpair pair;
	WinTab *cur, *prev;
	Queue tapq;

	threadsetname("keyboardtap");

	fschan = chancreate(sizeof(Stringpair), 0);
	enum { Akbd, Afromtap, Atotap, Aopen, Aclose,  NALT };
	Alt alts[NALT+1] = {
		[Akbd]		{.c = kbctl->c, .v = &s, .op = CHANRCV},
		[Afromtap]	{.c = nil, .v = &s, .op = CHANNOP},
		[Atotap]	{.c = nil, .v = &fschan, .op = CHANNOP},
		[Aopen]		{.c = opentap, .v = &chan, .op = CHANRCV},
		[Aclose]	{.c = closetap, .v = &chan, .op = CHANRCV},
		[NALT]		{.op = CHANEND},
	};

	memset(&tapq, 0, sizeof(tapq));
	cur = nil;
	for(;;){
		if(alts[Atotap].c && !qempty(&tapq))
			alts[Atotap].op = CHANSND;
		else
			alts[Atotap].op = CHANNOP;
		switch(alt(alts)){
		case Akbd:
			/* from keyboard to tap or to window */
			if(*s == 'k' || *s == 'K'){
				shiftdown = utfrune(s+1, Kshift) != nil;
				ctldown = utfrune(s+1, Kctl) != nil;
			}
			prev = cur;
			cur = focused ? focused->cur : nil;
			if(totap){
				if(cur != prev && cur){
					/* notify tap of focus change */
					z = smprint("z%d", cur->id);
					if(!qadd(&tapq, z))
						free(z);
				}
				/* send to tap */
				if(qadd(&tapq, s))
					break;
				/* tap is wedged, send directly instead */
			}
			if(cur)
				sendp(cur->kbd, s);
			else
				free(s);
			break;

		case Afromtap:
			/* from tap to window */
			if(cur && focused && cur == focused->cur)
				sendp(cur->kbd, s);
			else
				free(s);
			break;

		case Atotap:
			/* send queued up messages */
			recv(fschan, &pair);
			s = qget(&tapq);
			n = strlen(s)+1;
			pair.ns = min(n, pair.ns);
			memmove(pair.s, s, pair.ns);
			free(s);
			send(fschan, &pair);
			break;

		case Aopen:
			if(chan == fromtap){
				alts[Afromtap].c = fromtap;
				alts[Afromtap].op = CHANRCV;
			}
			if(chan == totap)
				alts[Atotap].c = totap;
			break;

		case Aclose:
			if(chan == fromtap){
				fromtap = nil;
				alts[Afromtap].c = nil;
				alts[Afromtap].op = CHANNOP;
				// TODO: empty chan
			}
			if(chan == totap){
				totap = nil;
				alts[Atotap].c = nil;
				alts[Atotap].op = CHANNOP;
				while(!qempty(&tapq))
					free(qget(&tapq));
			}
			chanfree(chan);
			break;
		}
	}
}

void
initcmd(void *arg)
{
	char *cmd;
	char *wsys;
	int fd;

	cmd = arg;
	rfork(RFENVG|RFFDG|RFNOTEG|RFNAMEG);
	wsys = getenv("wsys");
	fd = open(wsys, ORDWR);
	if(fd < 0)
		fprint(2, "lola: failed to open wsys: %r\n");
	if(mount(fd, -1, "/mnt/wsys", MREPL, "none") < 0)
		fprint(2, "lola: failed to mount wsys: %r\n");
	if(bind("/mnt/wsys", "/dev/", MBEFORE) < 0)
		fprint(2, "lola: failed to bind wsys: %r\n");
	free(wsys);
	close(fd);
	procexecl(nil, "/bin/rc", "rc", "-c", cmd, nil);
	fprint(2, "lola: exec failed: %r\n");
	exits("exec");
}

void
usage(void)
{
	fprint(2, "usage: lola [-i initcmd] [-s] [-t]\n");
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	char *initstr, *s;
	char buf[256];
if(strcmp(argv[0]+1, ".out") == 0){
rfork(RFENVG);
newwindow("-dx 1280 -dy 800");
scrolling = TRUE;
notitle = FALSE;
oknotes[nelem(oknotes)-2] = "interrupt";
}

	initstr = nil;
	ARGBEGIN{
	case 'i':
		initstr = EARGF(usage());
		break;
	case 's':
		scrolling = TRUE;
		break;
	case 't':
		notitle = TRUE;
		break;
	default:
		usage();
	}ARGEND

	if(getwd(buf, sizeof(buf)) == nil)
		startdir = estrdup(".");
	else
		startdir = estrdup(buf);
	s = getenv("tabstop");
	if(s)
		tabwidth = strtol(s, nil, 0);
	if(tabwidth == 0)
		tabwidth = 4;
	free(s);

	if(initdraw(nil, nil, "lola") < 0)
		sysfatal("initdraw: %r");
	kbctl = initkbd(nil, nil);
	if(kbctl == nil)
		sysfatal("inikeyboard: %r");
	mctl = initmouse(nil, screen);
	if(mctl == nil)
		sysfatal("initmouse: %r");
	opentap = chancreate(sizeof(Channel*), 0);
	closetap = chancreate(sizeof(Channel*), 0);

	pickchan = chancreate(sizeof(Channel*), 0);

	servekbd = kbctl->kbdfd >= 0;
	snarffd = open("/dev/snarf", OREAD|OCEXEC);
	gotscreen = access("/dev/screen", AEXIST)==0;

	initdata();
	/* hack to get menu colors referenced,
	 * so setting them with initstr will work */
	btn12menu();

	wscreen = allocscreen(screen, background, 0);
	fakebg = allocwindow(wscreen, screen->r, Refbackup, DNofill);
	draw(fakebg, fakebg->r, background, nil, ZP);

	timerinit();


	threadcreate(mthread, nil, mainstacksize);
	threadcreate(resthread, nil, mainstacksize);
	threadcreate(keyboardtap, nil, mainstacksize);

	flushimage(display, 1);

	startfs();

	if(initstr)
		proccreate(initcmd, initstr, mainstacksize);

	threadnotify(notehandler, 1);
}
