#include "inc.h"

Window *bottomwin, *topwin;
Window *windows[MAXWINDOWS];
int nwindows;
WinTab *wintabs[MAXWINDOWS];
int nwintabs;
Window *focused, *cursorwin;

Point screenoff;

static void wrepaint(Window *w);
static void winthread(void *arg);

static void
wlistpushback(Window *w)
{
	w->higher = bottomwin;
	if(bottomwin) bottomwin->lower = w;
	w->lower = nil;
	if(topwin == nil) topwin = w;
	bottomwin = w;
}

static void
wlistpushfront(Window *w)
{
	w->lower = topwin;
	if(topwin) topwin->higher = w;
	w->higher = nil;
	if(bottomwin == nil) bottomwin = w;
	topwin = w;
}

static void
wlistremove(Window *w)
{
	if(w->lower)
		w->lower->higher = w->higher;
	else
		bottomwin = w->higher;
	if(w->higher)
		w->higher->lower = w->lower;
	else
		topwin = w->lower;
	w->higher = nil;
	w->lower = nil;
}

void
wmaximize(Window *w)
{
	w->maximized = 1;
	w->noborder |= 2;
	w->origrect = w->frame->r;
	wresize(w, screen->r);
}

void
wrestore(Window *w)
{
	w->maximized = 0;
	w->noborder &= ~2;
	wresize(w, w->origrect);
}

static void
wcalcrects(Window *w, Rectangle r)
{
	w->rect = r;
	w->contrect = r;
	if(!w->noborder)
		w->contrect = insetrect(w->contrect, bordersz);

	w->titlerect = ZR;
	if(!w->notitle){
		w->titlerect = w->contrect;
		w->titlerect.max.y = w->titlerect.min.y + titlesz;
		w->contrect.min.y += titlesz;
	}

	w->tabrect = ZR;
	if(w->ref > 1){
		w->tabrect = w->contrect;
		w->tabrect.max.y = w->tabrect.min.y + tabsz;
		w->contrect.min.y += tabsz;
	}

	r = insetrect(w->contrect, 1);
	w->scrollr = r;
	w->scrollr.max.x = w->scrollr.min.x + 12;
	w->textr = r;
	w->textr.min.x = w->scrollr.max.x + 4;
}

int
wcolsel(Window *w)
{
	return (w != focused) + (w->cur && w->cur->holdmode)*2;
}

static void
wsetcolors(WinTab *w)
{
// TODO: this should use wcolsel
	int c = w->holdmode ?
		w->w == focused ? HOLDTEXT : PALEHOLDTEXT :
		w->w == focused ? TEXT : PALETEXT;
	w->text.cols[TEXT] = colors[c];
}

static void
tinit(WinTab *t)
{
	Window *w;

	if(t->deleted)
		return;
	w = t->w;
	t->mc.image = w->frame;
	t->content = allocwindow(w->screen, w->contrect, Refbackup, DNofill);
	assert(t->content);
	draw(t->content, t->content->r, colors[BACK], nil, ZP);
	xinit(&t->text, w->textr, w->scrollr, tabwidth, font, t->content, colors);
}

static void
tdeinit(WinTab *t)
{
	/* have to move image out of the way
	 * because client program can hold a ref
	 * and mess up our drawing. */
	if(t->content)
		originwindow(t->content, t->content->r.min, screen->r.max);
	freeimage(t->content);
	t->content = nil;
	t->mc.image = nil;
}

/* get rid of window visually */
static void
wremove(Window *w)
{
	if(w->frame)
		originwindow(w->frame, w->frame->r.min, screen->r.max);
}

static void
wfreeimages(Window *w)
{
	freescreen(w->screen);
	w->screen = nil;

	freeimage(w->frame);
	w->frame = nil;
}

/* create images, destroy first if they exist.
 * either only tab images e.g. when the tab bar appears/disappears
 * or full window when window is resized/moved. */
static void
wreinit(Window *w, bool all)
{
	Rectangle r, hr;

	for(WinTab *t = w->tab; t; t = t->next)
		tdeinit(t);

	if(all){
		r = w->rect;
		/* reference can be held by client program
		 * indefinitely which would keep this on screen. */
		wremove(w);
		wfreeimages(w);
		if(w->hidden){
			hr = rectaddpt(r, subpt(screen->r.max, r.min));
			w->frame = allocwindow(wscreen, hr, Refbackup, DNofill);
			originwindow(w->frame, r.min, hr.min);
		}else
			w->frame = allocwindow(wscreen, r, Refbackup, DNofill);
		w->screen = allocscreen(w->frame, colors[BACK], 0);
		assert(w->screen);
	}

	for(WinTab *t = w->tab; t; t = t->next)
		tinit(t);

	tfocus(w->cur);
}

// TODO: find better name
void
wrecreate(Window *w)
{
	wcalcrects(w, w->rect);
	wreinit(w, 0);
	for(WinTab *t = w->tab; t; t = t->next)
		wsendmsg(t, Resized);
}


static int id = 1;

Window*
wcreate(Rectangle r, bool hidden)
{
	Window *w;

	w = emalloc(sizeof(Window));
	w->hidden = hidden;
	w->notitle = notitle;	// TODO: argument?
	wcalcrects(w, r);
	wreinit(w, 1);
	wlistpushfront(w);
	// TODO: could be more graceful here
	assert(nwindows < MAXWINDOWS);
	windows[nwindows++] = w;

	wfocus(w);

	return w;
}

void
tfocus(WinTab *t)
{
	if(t == nil || t->deleted)
		return;
	t->w->cur = t;
	topwindow(t->content);
	wrepaint(t->w);
}

WinTab*
tcreate(Window *w, bool scrolling)
{
	WinTab *t, **tp;

	/* recreate window when tab bar appears
	 * before we attach the new tab. */
	incref(w);
	if(w->ref == 2)
		wrecreate(w);

	t = emalloc(sizeof(WinTab));
	incref(t);
	t->w = w;
	for(tp = &w->tab; *tp; tp = &(*tp)->next);
	*tp = t;
	tinit(t);
	t->id = id++;
	t->notefd = -1;
	wsetlabel(t, "<unnamed>");
	t->dir = estrdup(startdir);
	t->scrolling = scrolling;

	t->mc.c = chancreate(sizeof(Mouse), 16);
	t->gone = chancreate(sizeof(int), 0);
	t->kbd = chancreate(sizeof(char*), 16);
	t->ctl = chancreate(sizeof(int), 0);
	t->conswrite = chancreate(sizeof(Channel**), 0);
	t->consread = chancreate(sizeof(Channel**), 0);
	t->kbdread = chancreate(sizeof(Channel**), 0);
	t->mouseread = chancreate(sizeof(Channel**), 0);
	t->wctlread = chancreate(sizeof(Channel**), 0);
	t->complete = chancreate(sizeof(Completion*), 0);
	threadcreate(winthread, t, mainstacksize);

	wsetname(t);
	// TODO: could be more graceful here
	assert(nwintabs < MAXWINDOWS);
	wintabs[nwintabs++] = t;

	tfocus(t);

	return t;
}

WinTab*
wtcreate(Rectangle r, bool hidden, bool scrolling)
{
	return tcreate(wcreate(r, hidden), scrolling);
}

/* called from winthread when it exits */
static void
wfree(WinTab *w)
{
	if(w->notefd >= 0)
		close(w->notefd);
	xclear(&w->text);
	chanclose(w->mc.c);
	chanclose(w->gone);
	chanclose(w->kbd);
	chanclose(w->ctl);
	chanclose(w->conswrite);
	chanclose(w->consread);
	chanclose(w->kbdread);
	chanclose(w->mouseread);
	chanclose(w->wctlread);
	chanclose(w->complete);
	free(w->label);
	free(w);
}

static void
wdestroy(Window *w)
{
	int i;

	assert(w != focused);	/* this must be done elsewhere */
	assert(w->tab == nil);
	wlistremove(w);
	for(i = 0; i < nwindows; i++)
		if(windows[i] == w){
			nwindows--;
			memmove(&windows[i], &windows[i+1], (nwindows-i)*sizeof(Window*));
			break;
		}
	wfreeimages(w);
	free(w);
	flushimage(display, 1);
}

int
inwinthread(WinTab *w)
{
	return w->threadname == threadgetname();
}

/* decrement reference, close window once all tabs gone. */
static int
wrelease_(Window *w)
{
	if(decref(w) == 0){
		assert(w->tab == nil);
		assert(w->cur == nil);
		wremove(w);
		wunfocus(w);
		assert(w != focused);
		wdestroy(w);
		return 0;
	}else{
		assert(w->ref > 0);
		return w->ref;
	}
}

/* logically and visually close the tab.
 * struct, thread and link into window will stick
 * around until all references are gone.
 * safe to call multiple times. */
static void
tclose(WinTab *w)
{
	int i;

	if(w->deleted)
		return;
	w->deleted = TRUE;
	for(i = 0; i < nwintabs; i++)
		if(wintabs[i] == w){
			nwintabs--;
			memmove(&wintabs[i], &wintabs[i+1], (nwintabs-i)*sizeof(WinTab*));
			break;
		}
	tdeinit(w);
}

/* detach tab from window */
void
tdetach(WinTab *t)
{
	WinTab **tp;
	Window *w = t->w;

	if(w == nil)
		return;

	/* remove tab from window */
	for(tp = &w->tab; *tp; tp = &(*tp)->next){
		if(*tp == t){
			(*tp) = t->next;
			t->next = nil;
			t->w = nil;
			break;
		}
	}
	assert(t->w == nil);
	tdeinit(t);

	/* find new focused tab */
	if(w->cur == t){
		w->cur = *tp;
		if(w->cur == nil)
			for(w->cur = w->tab;
			    w->cur && w->cur->next;
			    w->cur = w->cur->next);
	}
	if(wrelease_(w) > 0){
		/* complete redraw if tab bar disappears */
		if(w->ref == 1)
			wrecreate(w);
		else
			tfocus(w->cur);
	}
}

void
tmigrate(WinTab *t, Window *w)
{
	WinTab **tp;

	if(t->w == w)
		return;
	tdetach(t);

	/* recreate window when tab bar appears
	 * before we attach the new tab. */
	incref(w);
	if(w->ref == 2)
		wrecreate(w);

	t->w = w;
	for(tp = &w->tab; *tp; tp = &(*tp)->next);
	*tp = t;
	tinit(t);

	tfocus(t);
	wsendmsg(t, Resized);
}

/* this SUCKS, want doubly linked lists */
static WinTab**
getprevptr(WinTab *t)
{
	WinTab **tp;
	for(tp = &t->w->tab; *tp; tp = &(*tp)->next)
		if(*tp == t)
			return tp;
	return nil;
}
static WinTab*
getprev(WinTab *t)
{
	WinTab *tt;
	for(tt = t->w->tab; tt; tt = tt->next)
		if(tt->next == t)
			return tt;
	return nil;
}

static void
tswapadjacent(WinTab *l, WinTab *r)
{
	WinTab **tp;

	tp = getprevptr(l);
	assert(tp);
	l->next = r->next;
	r->next = l;
	*tp = r;
	wdecor(l->w);
}

void
tmoveleft(WinTab *r)
{
	WinTab *l;
	l = getprev(r);
	if(l == nil) return;
	tswapadjacent(l, r);
}

void
tmoveright(WinTab *l)
{
	WinTab *r;
	r = l->next;
	if(r == nil) return;
	tswapadjacent(l, r);
}

/* decrement reference, close tab once all references gone. */
void
wrelease(WinTab *t)
{
	if(decref(t) == 0){
		/* increment ref count temporarily
		 * so win thread doesn't exit too early */
		incref(t);
		tdetach(t);
		tclose(t);
		decref(t);
		if(!inwinthread(t))
			wsendmsg(t, Wakeup);
	}else
		assert(t->ref > 0);
}

void
tdelete(WinTab *t)
{
	assert(!t->deleted);
	tdetach(t);
	tclose(t);

	wsendmsg(t, Deleted);
}


void
wsendmsg(WinTab *w, int type)
{
	assert(!inwinthread(w));
	sendul(w->ctl, type);
}

WinTab*
wfind(int id)
{
	int i;

	for(i = 0; i < nwintabs; i++)
		if(wintabs[i]->id == id)
			return wintabs[i];
	return nil;
}

Window*
wpointto(Point pt)
{
	Window *w;

	for(w = topwin; w; w = w->lower)
		if(!w->hidden && ptinrect(pt, w->frame->r))
			return w;
	return nil;
}

void
wsetcursor(WinTab *w)
{
	if(w->w == cursorwin)
		setcursornormal(w->holdmode ? &whitearrow : w->cursorp);
}

void
wsetlabel(WinTab *w, char *label)
{
	free(w->label);
	w->label = estrdup(label);
	wdecor(w->w);
}

void
wsetname(WinTab *w)
{
	int i, n;
	char err[ERRMAX];
	
	n = snprint(w->name, sizeof(w->name)-2, "%s.%d.%d", "noborder", w->id, w->namecount++);
	for(i='A'; i<='Z'; i++){
		if(nameimage(w->content, w->name, 1) > 0)
			return;
		errstr(err, sizeof err);
		if(strcmp(err, "image name in use") != 0)
			break;
		w->name[n] = i;
		w->name[n+1] = 0;
	}
	w->name[0] = 0;
	fprint(2, "lola: setname failed: %s\n", err);
}

void
wsetpid(WinTab *w, int pid, int dolabel)
{
	char buf[32];
	int ofd;

	ofd = w->notefd;
	if(pid <= 0)
		w->notefd = -1;
	else{
		if(dolabel){
			snprint(buf, sizeof(buf), "rc %lud", (ulong)pid);
			wsetlabel(w, buf);
		}
		snprint(buf, sizeof(buf), "/proc/%lud/notepg", (ulong)pid);
		w->notefd = open(buf, OWRITE|OCEXEC);
	}
	if(ofd >= 0)
		close(ofd);
}

void
wdelete(Window *w)
{
	wremove(w);
	wunfocus(w);
	while(w->tab)
		tdelete(w->tab);
}

static void
wrepaint(Window *w)
{
	wsetcolors(w->cur);
	wdecor(w);
	if(!w->cur->mouseopen)
		xredraw(&w->cur->text);
}

/* restore window order after reshaping has disturbed it */
void
worder(void)
{
	Window *w;
	for(w = bottomwin; w; w = w->higher)
		if(!w->hidden)
			topwindow(w->frame);
}

void
wresize(Window *w, Rectangle r)
{
	wcalcrects(w, r);
	wreinit(w, 1);
	if(w != topwin && !w->hidden)
		worder();
	for(WinTab *t = w->tab; t; t = t->next)
		wsendmsg(t, Resized);
}

void
wmove(Window *w, Point pos)
{
	wresize(w, rectaddpt(w->frame->r, subpt(pos, w->frame->r.min)));
}

void
wraise(Window *w)
{
	wlistremove(w);
	wlistpushfront(w);
	topwindow(w->frame);
	flushimage(display, 1);
}

void
wlower(Window *w)
{
	wlistremove(w);
	wlistpushback(w);
	bottomwindow(w->frame);
	bottomwindow(fakebg);
	flushimage(display, 1);
}

static void
wfocuschanged(Window *w)
{
// TODO(tab):
	if(w == nil || w->cur == nil)
		return;
	w->cur->wctlready = TRUE;
	wrepaint(w);
	if(!inwinthread(w->cur))
		wsendmsg(w->cur, Wakeup);
}

void
wfocus(Window *w)
{
	Window *prev;

	if(w == focused)
		return;
	prev = focused;
	focused = w;
	if(prev && prev->cur){
// TODO(tab): check this
		WinTab *t = prev->cur;
		/* release keys (if possible) */
		char *s = estrdup("K");
		if(nbsendp(t->kbd, s) != 1)
			free(s);
		/* release mouse buttons */
		if(t->mc.buttons){
			t->mc.buttons = 0;
			t->mq.counter++;
		}
	}
	wfocuschanged(prev);
	wfocuschanged(focused);
}

void
wunfocus(Window *w)
{
	if(w == focused)
		wfocus(nil);
}

// TODO(tab): wctl ready everyone?
int
whide(Window *w)
{
	if(w->hidden)
		return -1;
	incref(w->tab);
	wremove(w);
	wunfocus(w);
	w->hidden = TRUE;
	w->tab->wctlready = TRUE;
	wsendmsg(w->tab, Wakeup);
	wrelease(w->tab);
	return 1;
}

// TODO(tab): wctl ready everyone?
int
wunhide(Window *w)
{
	if(!w->hidden)
		return -1;
	incref(w->tab);
	w->hidden = FALSE;
	w->tab->wctlready = TRUE;
	originwindow(w->frame, w->frame->r.min, w->frame->r.min);
	wraise(w);
	wfocus(w);
	wrelease(w->tab);
	return 1;
}

void
wsethold(WinTab *w, int hold)
{
	int switched;

	if(hold)
		switched = w->holdmode++ == 0;
	else
		switched = --w->holdmode == 0;
	if(switched){
		wsetcursor(w);
		wrepaint(w->w);
	}
}

/* Normally the mouse will only be moved inside the window.
 * The force argument can move the mouse anywhere. */
void
wmovemouse(Window *w, Point pt, bool force)
{
	// TODO? rio also checks menuing and such
	if(force ||
	   w == focused && wpointto(mctl->xy) == w && ptinrect(pt, w->rect))
		moveto(mctl, pt);
}

/*
 * Need to do this in a separate proc because if process we're interrupting
 * is dying and trying to print tombstone, kernel is blocked holding p->debug lock.
 */
static void
interruptproc(void *v)
{
	int *notefd;

	notefd = v;
	write(*notefd, "interrupt", 9);
	close(*notefd);
	free(notefd);
}

/*
 * Filename completion
 */

typedef struct Completejob Completejob;
struct Completejob
{
	char	*dir;
	char	*str;
	WinTab	*win;
};

static void
completeproc(void *arg)
{
	Completejob *job;
	Completion *c;

	job = arg;
	threadsetname("namecomplete %s", job->dir);

	c = complete(job->dir, job->str);
	if(c != nil && sendp(job->win->complete, c) <= 0)
		freecompletion(c);

	wrelease(job->win);

	free(job->dir);
	free(job->str);
	free(job);
}

static int
windfilewidth(WinTab *w, uint q0, int oneelement)
{
	uint q;
	Rune r;

	q = q0;
	while(q > 0){
		r = w->text.r[q-1];
		if(r<=' ' || r=='=' || r=='^' || r=='(' || r=='{')
			break;
		if(oneelement && r=='/')
			break;
		--q;
	}
	return q0-q;
}

static void
namecomplete(WinTab *w)
{
	Text *x;
	int nstr, npath;
	Rune *path, *str;
	char *dir, *root;
	Completejob *job;

	x = &w->text;
	/* control-f: filename completion; works back to white space or / */
	if(x->q0<x->nr && x->r[x->q0]>' ')	/* must be at end of word */
		return;
	nstr = windfilewidth(w, x->q0, TRUE);
	str = x->r+(x->q0-nstr);
	npath = windfilewidth(w, x->q0-nstr, FALSE);
	path = x->r+(x->q0-nstr-npath);

	/* is path rooted? if not, we need to make it relative to window path */
	if(npath>0 && path[0]=='/')
		dir = smprint("%.*S", npath, path);
	else {
		if(strcmp(w->dir, "") == 0)
			root = ".";
		else
			root = w->dir;
		dir = smprint("%s/%.*S", root, npath, path);
	}
	if(dir == nil)
		return;

	/* run in background, winctl will collect the result on w->complete chan */
	job = emalloc(sizeof *job);
	job->str = smprint("%.*S", nstr, str);
	job->dir = cleanname(dir);
	job->win = w;
	incref(w);
	proccreate(completeproc, job, mainstacksize);
}

static void
showcandidates(WinTab *w, Completion *c)
{
	Text *x;
	int i;
	Fmt f;
	Rune *rp;
	uint nr, qline;
	char *s;

	x = &w->text;
	runefmtstrinit(&f);
	if (c->nmatch == 0)
		s = "[no matches in ";
	else
		s = "[";
	if(c->nfile > 32)
		fmtprint(&f, "%s%d files]\n", s, c->nfile);
	else{
		fmtprint(&f, "%s", s);
		for(i=0; i<c->nfile; i++){
			if(i > 0)
				fmtprint(&f, " ");
			fmtprint(&f, "%s", c->filename[i]);
		}
		fmtprint(&f, "]\n");
	}
	rp = runefmtstrflush(&f);
	nr = runestrlen(rp);

	/* place text at beginning of line before cursor and host point */
	qline = min(x->qh, x->q0);
	while(qline>0 && x->r[qline-1] != '\n')
		qline--;

	if(qline == x->qh){
		/* advance host point to avoid readback */
		x->qh = xinsert(x, rp, nr, qline)+nr;
	}else{
		xinsert(x, rp, nr, qline);
	}
	free(rp);
}

void
wkeyctl(WinTab *w, Rune r)
{
	Text *x;
	int nlines, n;
	int *notefd;

	x = &w->text;
	nlines = x->maxlines;	/* need signed */
	if(!w->mouseopen){
		switch(r){

		/* Scrolling */
		case Kscrollonedown:
			n = mousescrollsize(x->maxlines);
			xscrolln(x, max(n, 1));
			return;
		case Kdown:
			xscrolln(x, shiftdown ? 1 : nlines/3);
			return;
		case Kpgdown:
			xscrolln(x, nlines*2/3);
			return;
		case Kscrolloneup:
			n = mousescrollsize(x->maxlines);
			xscrolln(x, -max(n, 1));
			return;
		case Kup:
			xscrolln(x, -(shiftdown ? 1 : nlines/3));
			return;
		case Kpgup:
			xscrolln(x, -nlines*2/3);
			return;

		case Khome:
			xshow(x, 0);
			return;
		case Kend:
			xshow(x, x->nr);
			return;

		/* Cursor movement */
		case Kleft:
			if(x->q0 > 0)
				xplacetick(x, x->q0-1);
			return;
		case Kright:
			if(x->q1 < x->nr)
				xplacetick(x, x->q1+1);
			return;
		case CTRL('A'):
			while(x->q0 > 0 && x->r[x->q0-1] != '\n' &&
			      x->q0 != x->qh)
				x->q0--;
			xplacetick(x, x->q0);
			return;
		case CTRL('E'):
			while(x->q0 < x->nr && x->r[x->q0] != '\n')
				x->q0++;
			xplacetick(x, x->q0);
			return;
		case CTRL('B'):
			xplacetick(x, x->qh);
			return;

		/* Hold mode */
		case Kesc:
			wsethold(w, !w->holdmode);
			return;
		case Kdel:
			if(w->holdmode)
				wsethold(w, FALSE);
			break;
		}
	}

	if(x->rawmode && (x->q0 == x->nr || w->mouseopen))
		xaddraw(x, &r, 1);
	else if(r == Kdel){
		x->qh = x->nr;
		xshow(x, x->qh);
		if(w->notefd < 0)
			return;
		notefd = emalloc(sizeof(int));
		*notefd = dup(w->notefd, -1);
		proccreate(interruptproc, notefd, 4096);
	}else if(r == CTRL('F') || r == Kins)
		namecomplete(w);
	else
		xtype(x, r);
}

void
wmousectl(WinTab *w)
{
	int but;

	for(but = 1; but < 6; but++)
		if(w->mc.buttons == 1<<(but-1))
			goto found;
	return;
found:

	incref(w);
	if(shiftdown && but > 3)
		wkeyctl(w, but == 4 ? Kscrolloneup : Kscrollonedown);
	else if(ptinrect(w->mc.xy, w->text.scrollr) || but > 3)
		xscroll(&w->text, &w->mc, but);
	else if(but == 1)
		xselect(&w->text, &w->mc);
	wrelease(w);
}

void
winctl(WinTab *w, int type)
{
	Text *x;
	int i;

	if(type == Deleted)
		if(w->notefd >= 0)
			write(w->notefd, "hangup", 6);
	if(w->deleted)
		return;

	x = &w->text;
	switch(type){
	case Resized:
		wsetname(w);
		w->resized = TRUE;
		w->wctlready = TRUE;
		w->mc.buttons = 0;	/* avoid re-triggering clicks on resize */
		w->mq.counter++;	/* cause mouse to be re-read */
		break;

	case Refresh:
		/* take over window again */
		draw(w->content, w->content->r, x->cols[BACK], nil, ZP);
		xfullredraw(&w->text);
		break;

	case Holdon:
		wsethold(w, TRUE);
		break;
	case Holdoff:
		wsethold(w, FALSE);
		break;

	case Rawon:
		break;
	case Rawoff:
// TODO: better to remove one by one? not sure if wkeyctl is safe
		for(i = 0; i < x->nraw; i++)
			wkeyctl(w, x->raw[i]);
		x->nraw = 0;
		break;
	}
}

static void
winthread(void *arg)
{
	WinTab *w;
	Text *x;
	Rune r, *rp;
	char *s;
	int cm;
	enum { AKbd, AMouse, ACtl, AConsWrite, AConsRead,
		AKbdRead, AMouseRead, AWctlRead, AComplete, Agone, NALT };
	Alt alts[NALT+1];
	Channel *fsc;
	Stringpair pair;
	int i, nb, nr, initial;
	uint q0;
	RuneConvBuf cnv;
	Mousestate m;
	Completion *comp;

	w = arg;
	threadsetname("winthread-%d", w->id);
	w->threadname = threadgetname();
	x = &w->text;
	nr = 0;
	memset(&cnv, 0, sizeof(cnv));
	fsc = chancreate(sizeof(Stringpair), 0);

	alts[AKbd] = ALT(w->kbd, &s, CHANRCV);
	alts[AMouse] = ALT(w->mc.c, &w->mc.Mouse, CHANRCV);
	alts[ACtl] = ALT(w->ctl, &cm, CHANRCV);
	alts[AConsWrite] = ALT(w->conswrite, &fsc, CHANSND);
	alts[AConsRead] = ALT(w->consread, &fsc, CHANSND);
	alts[AKbdRead] = ALT(w->kbdread, &fsc, CHANSND);
	alts[AMouseRead] = ALT(w->mouseread, &fsc, CHANSND);
	alts[AWctlRead] = ALT(w->wctlread, &fsc, CHANSND);
	alts[AComplete] = ALT(w->complete, &comp, CHANRCV);
	alts[Agone] = ALT(w->gone, nil, CHANNOP);
	alts[NALT].op = CHANEND;

	for(;;){
		if(w->deleted){
			alts[Agone].op = CHANSND;
			alts[AConsWrite].op = CHANNOP;
			alts[AConsRead].op = CHANNOP;
			alts[AKbdRead].op = CHANNOP;
			alts[AMouseRead].op = CHANNOP;
			alts[AWctlRead].op = CHANNOP;
		}else{
			nr = xninput(x);
			if(!w->holdmode && (nr >= 0 || cnv.n > 0 || x->rawmode && x->nraw > 0))
				alts[AConsRead].op = CHANSND;
			else
				alts[AConsRead].op = CHANNOP;
			if(w->scrolling || w->mouseopen || x->qh <= x->org+x->nchars)
				alts[AConsWrite].op = CHANSND;
			else
				alts[AConsWrite].op = CHANNOP;
			if(w->kbdopen && !qempty(&w->kq))
				alts[AKbdRead].op = CHANSND;
			else
				alts[AKbdRead].op = CHANNOP;
			if(w->mouseopen && w->mq.counter != w->mq.lastcounter)
				alts[AMouseRead].op = CHANSND;
			else
				alts[AMouseRead].op = CHANNOP;
			alts[AWctlRead].op = w->wctlready ? CHANSND : CHANNOP;
		}

		switch(alt(alts)){
		case AKbd:
			if(!qadd(&w->kq, s))
				free(s);
			if(!w->kbdopen)
			while(!qempty(&w->kq)){
				s = qget(&w->kq);
				if(*s == 'c'){
					chartorune(&r, s+1);
					if(r)
						wkeyctl(w, r);
				}
				free(s);
			}
			break;

		case AKbdRead:
			recv(fsc, &pair);
			nb = 0;
			while(!qempty(&w->kq)){
				s = w->kq.q[w->kq.ri];
				i = strlen(s)+1;
				if(nb+i > pair.ns)
					break;
				qget(&w->kq);
				memmove((char*)pair.s + nb, s, i);
				free(s);
				nb += i;
			}
			pair.ns = nb;
			send(fsc, &pair);
			break;

		case AMouse:
			if(w->mouseopen){
				Mousestate *mp;
				w->mq.counter++;
				/* queue click events in ring buffer.
				 * pure movement only in else branch of the case below */
				if(!w->mq.full && w->mq.lastb != w->mc.buttons){
					mp = &w->mq.q[w->mq.wi++];
					w->mq.wi %= nelem(w->mq.q);
					w->mq.full = w->mq.wi == w->mq.ri;
					mp->Mouse = w->mc;
					mp->counter = w->mq.counter;
					w->mq.lastb = w->mc.buttons;
				}
			}else
				wmousectl(w);
			break;

		case AMouseRead:
			recv(fsc, &pair);
			w->mq.full = FALSE;
			/* first return queued clicks, then current state */
			if(w->mq.wi != w->mq.ri){
				m = w->mq.q[w->mq.ri++];
				w->mq.ri %= nelem(w->mq.q);
			}else
				m = (Mousestate){w->mc.Mouse, w->mq.counter};
			w->mq.lastcounter = m.counter;

			pair.ns = snprint(pair.s, pair.ns, "%c%11d %11d %11d %11ld ",
				"mr"[w->resized], m.xy.x, m.xy.y, m.buttons, m.msec);
			w->resized = FALSE;
			send(fsc, &pair);
			break;

		case AConsWrite:
			recv(fsc, &pair);
			initial = handlebs(&pair);
			if(initial){
				initial = min(initial, x->qh);
				xdelete(x, x->qh-initial, x->qh);
			}
			x->qh = xinsert(x, pair.s, pair.ns, x->qh) + pair.ns;
			free(pair.s);
			if(w->scrolling || w->mouseopen)
				xshow(x, x->qh);
			xscrdraw(x);
			break;

		case AConsRead:
			recv(fsc, &pair);
			cnvsize(&cnv, pair.ns);
			nr = r2bfill(&cnv, x->r+x->qh, nr);
			x->qh += nr;
			/* if flushed by ^D, skip the ^D */
			if(!(nr > 0 && x->r[x->qh-1] == '\n') &&
			   x->qh < x->nr && x->r[x->qh] == CTRL('D'))
				x->qh++;
			if(x->rawmode){
				nr = r2bfill(&cnv, x->raw, x->nraw);
				x->nraw -= nr;
				runemove(x->raw, x->raw+nr, x->nraw);
			}
			r2bfinish(&cnv, &pair);
			send(fsc, &pair);
			break;

		case AWctlRead:
			w->wctlready = FALSE;
			recv(fsc, &pair);
Window *ww = w->w;
			pair.ns = snprint(pair.s, pair.ns, "%11d %11d %11d %11d %11s %11s ",
				ww->frame->r.min.x, ww->frame->r.min.y,
				ww->frame->r.max.x, ww->frame->r.max.y,
				ww->cur == w ? ww == focused ? "current" : "notcurrent"
					: "tab",
				ww->hidden ? "hidden" : "visible");
			send(fsc, &pair);
			break;

		case ACtl:
			winctl(w, cm);
			break;

		case AComplete:
			if(w->content){
				if(!comp->advance)
					showcandidates(w, comp);
				if(comp->advance){
					rp = runesmprint("%s", comp->string);
					if(rp){
						nr = runestrlen(rp);
						q0 = x->q0;
						q0 = xinsert(x, rp, nr, q0);
						xshow(x, q0+nr);
						free(rp);
					}
				}
			}
			freecompletion(comp);
			break;
		}
		flushimage(display, 1);

		/* window is gone, clean up and exit thread */
		if(w->ref == 0){
			wfree(w);
			chanfree(fsc);
			free(cnv.buf);
			return;
		}
	}
}

static void
shellproc(void *args)
{
	WinTab *w;
	Channel *pidc;
	void **arg;
	char *cmd, *dir;
	char **argv;

	arg = args;
	w = arg[0];
	pidc = arg[1];
	cmd = arg[2];
	argv = arg[3];
	dir = arg[4];
	rfork(RFNAMEG|RFFDG|RFENVG);
	if(fsmount(w->id) < 0){
		fprint(2, "mount failed: %r\n");
		sendul(pidc, 0);
		threadexits("mount failed");
	}
	close(0);
	if(open("/dev/cons", OREAD) < 0){
		fprint(2, "can't open /dev/cons: %r\n");
		sendul(pidc, 0);
		threadexits("/dev/cons");
	}
	close(1);
	if(open("/dev/cons", OWRITE) < 0){
		fprint(2, "can't open /dev/cons: %r\n");
		sendul(pidc, 0);
		threadexits("open");	/* BUG? was terminate() */
	}
	/* remove extra ref hanging from creation.
	 * not in main proc here, so be careful with wrelease! */
	assert(w->ref > 1);
	wrelease(w);

	notify(nil);
	dup(1, 2);
	if(dir)
		chdir(dir);
	procexec(pidc, cmd, argv);
	_exits("exec failed");
}

int
wincmd(WinTab *w, int pid, char *dir, char **argv)
{
	Channel *cpid;
	void *args[5];

	if(argv){
		cpid = chancreate(sizeof(int), 0);
		assert(cpid);
		args[0] = w;
		args[1] = cpid;
		args[2] = "/bin/rc";
		args[3] = argv;
		args[4] = dir;
		proccreate(shellproc, args, mainstacksize);
		pid = recvul(cpid);
		chanfree(cpid);
		if(pid == 0){
			wdelete(w->w);
			return 0;
		}
	}

	wsetpid(w, pid, 1);
	if(dir){
		free(w->dir);
		w->dir = estrdup(dir);
	}

	return pid;
}

void
screenoffset(int offx, int offy)
{
	Window *w;
	Point off, delta;

	off = Pt(offx, offy);
	delta = subpt(off, screenoff);
	screenoff = off;
	for(w = bottomwin; w; w = w->higher){
		if(w->sticky){
			/* Don't move but cause resize event because
			 * program may want some kind of notification */
			wmove(w, w->frame->r.min);
			continue;
		}
		if(w->maximized){
			wrestore(w);
			wmove(w, subpt(w->frame->r.min, delta));
			wmaximize(w);
		}else
			wmove(w, subpt(w->frame->r.min, delta));
	}

	flushimage(display, 1);
}
