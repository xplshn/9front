#include "inc.h"

enum {
	Qroot,
	Qwsys,
	Qscreen,
	Qsnarf,
	Qwctl,
	Qtap,
	Qpick,
	Qglobal = Qpick,	/* last global one */

	/* these need a window */
	Qcons,
	Qconsctl,
	Qcursor,
	Qwinid,
	Qwinname,
	Qlabel,
	Qkbd,
	Qmouse,
	Qtext,
	Qwdir,
	Qwindow,

	NQids,
};

typedef struct Dirent Dirent;
struct Dirent
{
	int path;
	int type;
	char *name;
	uint mode;
};

Dirent dirents[] = {
	{ Qroot,	QTDIR,	".",	0500|DMDIR },
	{ Qwsys,	QTDIR,	"wsys",	0500|DMDIR },
	{ Qwinid,	QTFILE,	"winid",	0400 },
	{ Qwinname,	QTFILE,	"winname",	0400 },
	{ Qwdir,	QTFILE,	"wdir",	0600 },
	{ Qlabel,	QTFILE,	"label",	0600 },
	{ Qsnarf,	QTFILE,	"snarf",	0600 },
	{ Qtext,	QTFILE,	"text",	0600 },
	{ Qcons,	QTFILE, "cons",	0600 },
	{ Qconsctl,	QTFILE, "consctl",	0200 },
	{ Qkbd,	QTFILE, "kbd",	0600 },
	{ Qmouse,	QTFILE, "mouse",	0600 },
	{ Qcursor,	QTFILE, "cursor",	0600 },
	{ Qscreen,	QTFILE, "screen",	0400 },
	{ Qwindow,	QTFILE, "window",	0400 },
	{ Qwctl,	QTFILE, "wctl",	0600 },
	{ Qpick,	QTFILE, "pick",	0400 },
	{ Qtap,	QTFILE, "kbdtap",	0660 }
};

char Eperm[] = "permission denied";
char Eexist[] = "file does not exist";		// XXX
char Enotdir[] = "not a directory";		// XXX
char Ebadfcall[] = "bad fcall type";		// XXX
char Eoffset[] = "illegal offset";
char Enomem[] = "out of memory";

char Eflush[] =		"interrupted";
char Einuse[] =		"file in use";
char Edeleted[] =	"window deleted";
char Etooshort[] =	"buffer too small";
char Eshort[] =		"short i/o request";
char Elong[] = 		"snarf buffer too long";
char Eunkid[] = 	"unknown id in attach";
char Ebadrect[] = 	"bad rectangle in attach";		// XXX
char Ewindow[] = 	"cannot make window";
char Enowindow[] = 	"window has no image";			// XXX
char Ebadmouse[] = 	"bad format on /dev/mouse";

int fsysfd;
char srvpipe[64];
char *user;

/* Extension of a Req, req->aux. also has a thread. */
typedef struct Xreq Xreq;
struct Xreq
{
	Req *req;
	Channel *xc;
	Channel *flush;		/* cancel read/write */
	Xreq *next;
};
#define XR(req) ((Xreq*)(req)->aux)
static Xreq *xreqfree;

/* Extension of a Fid, fid->aux */
typedef struct Xfid Xfid;
struct Xfid
{
	WinTab *w;
	RuneConvBuf cnv;
};
#define XF(fid) ((Xfid*)(fid)->aux)

typedef struct XreqMsg XreqMsg;
struct XreqMsg
{
	Req *r;
	void (*f)(Req*);
};

static void
xreqthread(void *a)
{
	Xreq *xr = a;
	XreqMsg xm;

	threadsetname("xreg.%p", xr);
	for(;;){
		recv(xr->xc, &xm);
		xr->req = xm.r;
		xm.r->aux = xr;
		(*xm.f)(xm.r);
		/* return to pool */
		xr->req = nil;
		xr->next = xreqfree;
		xreqfree = xr;
	}
}

static Xreq*
getxreq(void)
{
	Xreq *xr;
	if(xreqfree){
		xr = xreqfree;
		xreqfree = xr->next;
	}else{
		xr = emalloc(sizeof(Xreq));
		xr->xc = chancreate(sizeof(XreqMsg), 0);
		xr->flush = chancreate(sizeof(int), 0);
		threadcreate(xreqthread, xr, mainstacksize);
	}
	xr->next = nil;
	return xr;
}

static void
toxreq(Req *r, void (*f)(Req*))
{
	Xreq *xr;
	XreqMsg xm;

	xr = getxreq();
	xm.r = r;
	xm.f = f;
	send(xr->xc, &xm);
}

static Xfid*
getxfid(WinTab *w)
{
	Xfid *xf;
	xf = emalloc(sizeof(Xfid));
	memset(&xf->cnv, 0, sizeof(xf->cnv));
	xf->w = w;
	if(w)
		incref(w);
	return xf;
}

#define QID(w, q) ((w)<<8|(q))
#define QWIN(q) ((q)>>8)
#define QFILE(q) ((int)(q)&0xFF)
#define ID(w) ((w) ? (w)->id : 0)

static void
fsattach(Req *r)
{
	WinTab *w;
	char *end;
	int id;
	Wctlcmd cmd;

	if(strcmp(r->ifcall.uname, user) != 0){
		respond(r, Eperm);
		return;
	}

	if(strncmp(r->ifcall.aname, "new", 3) == 0){
		cmd = parsewctl(r->ifcall.aname, ZR);
		if(cmd.error){
			respond(r, cmd.error);
			return;
		}
		if(cmd.id > 0){
			w = wfind(cmd.id);
			if(w == nil){
				respond(r, Eunkid);
				return;
			}
			w = tcreate(w->w, cmd.scrolling);
		}else
			w = wtcreate(cmd.r, cmd.hidden, cmd.scrolling);
		if(w == nil){
			respond(r, Ewindow);
			return;
		}
		wincmd(w, cmd.pid, cmd.dir, nil);
		flushimage(display, 1);
		decref(w);	/* don't delete, xfid will take it */
	}else if(strncmp(r->ifcall.aname, "none", 4) == 0){
		w = nil;
	}else if(id = strtol(r->ifcall.aname, &end, 10), *end == '\0'){
		w = wfind(id);
		if(w == nil){
			respond(r, Eunkid);
			return;
		}
	}else{
		respond(r, Eunkid);
		return;
	}

	r->fid->aux = getxfid(w);
	r->fid->qid = (Qid){QID(ID(w),Qroot),0,QTDIR};
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

static char*
fsclone(Fid *fid, Fid *newfid)
{
	if(XF(fid))
		newfid->aux = getxfid(XF(fid)->w);
	return nil;
}

int
skipfile(char *name)
{
	return gotscreen && strcmp(name, "screen") == 0 ||
	   snarffd >= 0 && strcmp(name, "snarf") == 0 ||
	   !servekbd && strcmp(name, "kbd") == 0;
}

static char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	int i;
	Dirent *d;
	Xfid *xf;
	WinTab *w;
	int dir;

	xf = fid->aux;
	w = xf->w;
	dir = QFILE(fid->qid.path);
	if(dir == Qroot){
		if(strcmp(name, "..") == 0){
			/* This sucks because we don't know which window we came from
			 * error out for now */
			return "vorwärts immer, rückwärts nimmer";
		}
		for(i = 0; i < nelem(dirents); i++){
			d = &dirents[i];
			if((w || d->path <= Qglobal) &&
			   !skipfile(d->name) && strcmp(name, d->name) == 0){
				fid->qid = (Qid){QID(ID(w),d->path), 0, d->type};
				*qid = fid->qid;
				return nil;
			}
		}
	}else if(dir == Qwsys){
		char *end;
		int id;
		if(strcmp(name, "..") == 0){
			fid->qid = (Qid){QID(ID(w),Qroot), 0, QTDIR};
			*qid = fid->qid;
			return nil;
		}
		if(id = strtol(name, &end, 10), *end == '\0'){
			w = wfind(id);
			if(w || id == 0){
				if(w)
					incref(w);
				wrelease(xf->w);
				xf->w = w;
				fid->qid = (Qid){QID(ID(w),Qroot), 0, QTDIR};
				*qid = fid->qid;
				return nil;
			}
		}
	}
	return "no such file";
}

static int
genrootdir(int n, Dir *d, void *a)
{
	WinTab *w = a;
	int i;

	n++;	/* -1 is root dir */
	i = 0;
	while(n--){
		i++;
		if(i >= nelem(dirents))
			return -1;
		/* we know the last file is never skipped */
		while(w == nil && dirents[i].path > Qglobal ||
		      skipfile(dirents[i].name))
			i++;
	}

	d->atime = time(nil);
	d->mtime = d->atime;
	d->uid = estrdup9p(user);
	d->gid = estrdup9p(d->uid);
	d->muid = estrdup9p(d->uid);
	d->qid = (Qid){QID(ID(w),dirents[i].path), 0, dirents[i].type};
	if(dirents[i].path == Qsnarf)
		d->qid.vers = snarfversion;
	d->mode = dirents[i].mode;
	d->name = estrdup9p(dirents[i].name);
	d->length = 0;
	return 0;
}

static int
genwsysdir(int n, Dir *d, void*)
{
	WinTab *w;

	if(n == -1){
		genrootdir(0, d, nil);
		free(d->name);
		d->name = estrdup9p("wsys");
		return 0;
	}
	if(n < nwintabs){
		w = wintabs[n];
		genrootdir(-1, d, w);
		free(d->name);
		d->name = smprint("%d", w->id);
		return 0;
	}
	return -1;
}

static int ntsnarf;
static char *tsnarf;

static void
fsopen(Req *r)
{
	Xfid *xf;
	WinTab *w;
	int rd, wr;

	xf = XF(r->fid);
	w = xf->w;

	/* TODO: check and sanitize mode */

	if(w && w->deleted){
		respond(r, Edeleted);
		return;
	}

	/* only text can be truncated (not implemented yet) */
	if(QFILE(r->fid->qid.path) != Qtext)
		r->ifcall.mode &= (OREAD|OWRITE|ORDWR);		

	rd = r->ifcall.mode==ORDWR || r->ifcall.mode==OREAD;
	wr = r->ifcall.mode==ORDWR || r->ifcall.mode==OWRITE;
	switch(QFILE(r->fid->qid.path)){
	case Qtext:
		if(r->ifcall.mode & OTRUNC)
			xdelete(&w->text, 0, w->text.nr);
		break;

	case Qsnarf:
		if(wr)
			ntsnarf = 0;
		break;

	case Qconsctl:
		if(w->consctlopen){
			respond(r, Einuse);
			return;
		}
		w->consctlopen = TRUE;
		break;

	case Qkbd:
		if(w->kbdopen){
			respond(r, Einuse);
			return;
		}
		w->kbdopen = TRUE;
		break;

	case Qmouse:
		if(w->mouseopen){
			respond(r, Einuse);
			return;
		}
		w->resized = FALSE;
		w->mouseopen = TRUE;
		break;

	case Qwctl:
		if(w && rd){
			/* can only have one reader of wctl */
			if(w->wctlopen){
				respond(r, Einuse);
				return;
			}
			w->wctlopen = TRUE;
			w->wctlready = TRUE;
			wsendmsg(w, Wakeup);
		}
		break;

	case Qpick:
		if(xf->w){
			wrelease(xf->w);
			xf->w = nil;
		}
		/* pick window from main thread.
		 * TODO: this may not be optimal because
		 *       it might block this thread. */
		Channel *wc = chancreate(sizeof(WinTab*), 0);
		sendp(pickchan, wc);
		w = recvp(wc);
		/* actually want the current tab */
		if(w) w = ((Window*)w)->cur;
		xf->w = w;
		if(w)
			incref(w);
		chanfree(wc);
		break;

	case Qtap:
		if(rd && totap || wr && fromtap){
			respond(r, Einuse);
			return;
		}
		if(rd){
			totap = chancreate(sizeof(Channel**), 0);
			sendp(opentap, totap);
		}
		if(wr){
			fromtap = chancreate(sizeof(char*), 32);
			sendp(opentap, fromtap);
		}
		break;
	}

	respond(r, nil);
}

static void
fsclose(Fid *fid)
{
	Xfid *xf;
	WinTab *w;
	Text *x;
	int rd, wr;

	xf = XF(fid);
	if(xf == nil)
		return;
	w = xf->w;
	x = &w->text;

	rd = fid->omode==ORDWR || fid->omode==OREAD;
	wr = fid->omode==ORDWR || fid->omode==OWRITE;
	if(fid->omode != -1)
	switch(QFILE(fid->qid.path)){
	/* replace snarf buffer when /dev/snarf is closed */
	case Qsnarf:
		if(wr){
			setsnarf(tsnarf, ntsnarf);
			ntsnarf = 0;
		}
		break;

	case Qconsctl:
		if(x->rawmode){
			x->rawmode = 0;
			wsendmsg(w, Rawoff);
		}
		if(w->holdmode > 0){
			w->holdmode = 1;
			wsendmsg(w, Holdoff);
		}
		w->consctlopen = FALSE;
		break;

	case Qkbd:
		w->kbdopen = FALSE;
		break;

	case Qmouse:
		w->mouseopen = FALSE;
		w->resized = FALSE;
		wsendmsg(w, Refresh);
		break;

	case Qcursor:
		w->cursorp = nil;
		wsetcursor(w);
		break;

	case Qwctl:
		if(w && rd)
			w->wctlopen = FALSE;
		break;

	case Qtap:
		if(wr && fromtap)
			sendp(closetap, fromtap);
		if(rd && totap)
			sendp(closetap, totap);
		break;
	}

	if(xf->w)
		wrelease(xf->w);
	free(xf->cnv.buf);
	free(xf);
	fid->aux = nil;
}

static int
readimgdata(Image *i, char *t, Rectangle r, int offset, int n)
{
	int ww, oo, y, m;
	uchar *tt;

	ww = bytesperline(r, i->depth);
	r.min.y += offset/ww;
	if(r.min.y >= r.max.y)
		return 0;
	y = r.min.y + (n + ww-1)/ww;
	if(y < r.max.y)
		r.max.y = y;
	m = ww * Dy(r);
	oo = offset % ww;
	if(oo == 0 && n >= m)
		return unloadimage(i, r, (uchar*)t, n);
	if((tt = malloc(m)) == nil)
		return -1;
	m = unloadimage(i, r, tt, m) - oo;
	if(m > 0){
		if(n < m) m = n;
		memmove(t, tt + oo, m);
	}
	free(tt);
	return m;
}

/* Fill request from image,
 * returns only either header or data */
char*
readimg(Req *r, Image *img)
{
	char *head;
	char cbuf[30];
	Rectangle rect;
	int n;

	rect = img->r;
	if(r->ifcall.offset < 5*12){
		head = smprint("%11s %11d %11d %11d %11d ",
			chantostr(cbuf, img->chan),
			rect.min.x, rect.min.y, rect.max.x, rect.max.y);
		readstr(r, head);
		free(head);
	}else{
		/* count is unsigned, so check with n */
		n = readimgdata(img, r->ofcall.data, rect, r->ifcall.offset-5*12, r->ifcall.count);
		if(n < 0)
			return Enomem;
		r->ofcall.count = n;
	}
	return nil;
}

static char*
waitblocking(Req *r, Channel *waitchan, Channel **replychan)
{
	WinTab *w;
	enum { Adata, Agone, Aflush, NALT };
	Alt alts[NALT+1];

	w = XF(r->fid)->w;

	*replychan = nil;
	alts[Adata] = ALT(waitchan, replychan, CHANRCV);
	alts[Agone] = w ? ALT(w->gone, nil, CHANRCV)
			: ALT(nil, nil, CHANNOP);
	alts[Aflush] = ALT(XR(r)->flush, nil, CHANRCV);
	alts[NALT].op = CHANEND;
	switch(alt(alts)){
	case Adata: return nil;
	case Agone: return Edeleted;
	case Aflush: return Eflush;
	}
	assert(0);	/* can't happen */
	return nil;
}

static char*
readblocking(Req *r, Channel *readchan)
{
	Channel *chan;
	Stringpair pair;
	char *err;

	if(err = waitblocking(r, readchan, &chan))
		return err;
	pair.s = r->ofcall.data;
	pair.ns = r->ifcall.count;
	send(chan, &pair);
	recv(chan, &pair);
	r->ofcall.count = min(r->ifcall.count, pair.ns);
	return nil;
}

static void
xread(Req *r)
{
	WinTab *w;
	char *data;

	w = XF(r->fid)->w;

	if(w && w->deleted){
		respond(r, Edeleted);
		return;
	}

	switch(QFILE(r->fid->qid.path)){
	case Qwinid:
		data = smprint("%11d ", w->id);
		readstr(r, data);
		free(data);
		break;
	case Qwinname:
		readstr(r, w->name);
		break;
	case Qlabel:
		readstr(r, w->label);
		break;
	case Qsnarf:
		data = smprint("%.*S", nsnarf, snarf);
		readstr(r, data);
		free(data);
		break;
	case Qtext:
		data = smprint("%.*S", w->text.nr, w->text.r);
		readstr(r, data);
		free(data);
		break;
	case Qcons:
		respond(r, readblocking(r, w->consread));
		return;
	case Qkbd:
		respond(r, readblocking(r, w->kbdread));
		return;
	case Qmouse:
		respond(r, readblocking(r, w->mouseread));
		return;
	case Qcursor:
		respond(r, "cursor read not implemented");
		return;
	case Qscreen:
		respond(r, readimg(r, screen));
		return;
	case Qwindow:
		respond(r, readimg(r, w->w->frame));
		return;
	case Qwctl:
/* TODO: what's with the Etooshort conditions?? */
		if(w == nil){
			if(r->ifcall.count < 4*12){
				respond(r, Etooshort);
				return;
			}
//			data = smprint("%11d %11d %11d %11d %11s %11s ",
			data = smprint("%11d %11d %11d %11d %11d %11d ",
				screen->r.min.x, screen->r.min.y, screen->r.max.x, screen->r.max.y,
//				"nowindow", "nowindow");
				screenoff.x, screenoff.y);
			readstr(r, data);
			free(data);
		}else{
			if(r->ifcall.count < 4*12){
				respond(r, Etooshort);
				return;
			}
			respond(r, readblocking(r, w->wctlread));
			return;
		}
		break;
	case Qpick:
		data = smprint("%11d ", w ? w->id : -1);
		readstr(r, data);
		free(data);
		break;
	case Qtap:
		respond(r, readblocking(r, totap));
		return;
	default:
		respond(r, "cannot read");
		return;
	}
	respond(r, nil);
}

static void
xwrite(Req *r)
{
	Xfid *xf;
	WinTab *w;
	Text *x;
	vlong offset;
	u32int count;
	char *data, *p, *e, *err;
	Point pt;
	Channel *chan;
	Stringpair pair;

	xf = XF(r->fid);
	w = xf->w;
	x = &w->text;
	offset = r->ifcall.offset;
	count = r->ifcall.count;
	data = r->ifcall.data;
	r->ofcall.count = count;

	/* custom emalloc9p allows us this */
	data[count] = '\0';

	if(w && w->deleted){
		respond(r, Edeleted);
		return;
	}
	int f = QFILE(r->fid->qid.path);
	switch(f){
	case Qtext:
	case Qcons:
		if(err = waitblocking(r, w->conswrite, &chan)){
			respond(r, err);
			return;
		}
		cnvsize(&xf->cnv, count);
		memmove(xf->cnv.buf+xf->cnv.n, data, count);
		xf->cnv.n += count;
		pair = b2r(&xf->cnv);
		send(chan, &pair);
		break;

	case Qconsctl:
		if(strncmp(data, "holdon", 6) == 0){
			wsendmsg(w, Holdon);
			break;
		}
		if(strncmp(data, "holdoff", 7) == 0){
			wsendmsg(w, Holdoff);
			break;
		}
		if(strncmp(data, "rawon", 5) == 0){
			if(w->holdmode){
				w->holdmode = 1;
				wsendmsg(w, Holdoff);
			}
			if(x->rawmode++ == 0)
				wsendmsg(w, Rawon);
			break;
		}
		if(strncmp(data, "rawoff", 6) == 0){
			if(--x->rawmode == 0)
				wsendmsg(w, Rawoff);
			break;
		}
		respond(r, "unknown control message");
		return;

	case Qmouse:
		if(data[0] != 'm' && data[0] != 'M'){
			respond(r, Ebadmouse);
			return;
		}
		p = nil;
		pt.x = strtoul(data+1, &p, 0);
		if(p == nil){
			respond(r, Eshort);
			return;
		}
		pt.y = strtoul(p, nil, 0);
		wmovemouse(w->w, pt, data[0] == 'M');
		break;

	case Qcursor:
		if(count < 2*4+2*2*16)
			w->cursorp = nil;
		else{
			w->cursor.offset.x = BGLONG(data+0*4);
			w->cursor.offset.y = BGLONG(data+1*4);
			memmove(w->cursor.clr, data+2*4, 2*2*16);
			w->cursorp = &w->cursor;
		}
		cursor = (void*)(uintptr)~0;	/* invalide cache */
		wsetcursor(w);
		break;

	case Qlabel:
		if(offset != 0){
			respond(r, "non-zero offset writing label");
			return;
		}
		wsetlabel(w, data);
		break;

	case Qsnarf:
		if(count == 0)
			break;
		/* always append only */
		if(ntsnarf > MAXSNARF){	/* avoid thrashing when people cut huge text */
			respond(r, Elong);
			return;
		}
		p = realloc(tsnarf, ntsnarf+count);
		if(p == nil){
			respond(r, Enomem);
			return;
		}
		tsnarf = p;
		memmove(tsnarf+ntsnarf, data, count);
		ntsnarf += count;
		break;

	case Qwdir:
		if(count > 0 && data[count-1] == '\n')
			data[--count] = '\0';
		if(count == 0)
			break;
		/* assume data comes in a single write */
		if(data[0] == '/')
			p = smprint("%.*s", count, data);
		else
			p = smprint("%s/%.*s", w->dir, count, data);
		if(p == nil){
			respond(r, Enomem);
			return;
		}
		free(w->dir);
		w->dir = cleanname(p);
		break;

	case Qwctl:
		respond(r, writewctl(w, data));
		return;

	case Qtap:
		if(count < 2){
			respond(r, "malformed key");
			return;
		}
		e = data + count;
		for(p = data; p < e; p += strlen(p)+1){
			switch(*p){
			case '\0':
				r->ofcall.count = p - data;
				respond(r, "null message type");
				return;
			case 'z':
				/* ignore context change */
				break;
			default:
				chanprint(fromtap, "%s", p);
				break;	
			}
		}
		break;

	default:
		respond(r, "cannot write");
		return;
	}
	respond(r, nil);
}

static void
fsread(Req *r)
{
	if((r->fid->qid.type & QTDIR) == 0){
		toxreq(r, xread);
		return;
	}

	switch(QFILE(r->fid->qid.path)){
	case Qroot:
		dirread9p(r, genrootdir, XF(r->fid)->w);
		break;
	case Qwsys:
		dirread9p(r, genwsysdir, nil);
		break;
	}
	respond(r, nil);
}

static void
fswrite(Req *r)
{
	toxreq(r, xwrite);
}

static void
fsflush(Req *r)
{
	Xreq *xr;
	int dummy = 0;

	xr = XR(r->oldreq);
	assert(xr);

	/* TODO: not entirely sure this is right.
	 * is it possible no-one is listening? */
	send(xr->flush, &dummy);
	respond(r, nil);
}

static void
fsstat(Req *r)
{
	int f;

	f = QFILE(r->fid->qid.path);
	genrootdir(f-1, &r->d, XF(r->fid)->w);
	respond(r, nil);
}

Srv fsys = {
	.attach		fsattach,
	.open		fsopen,
	.read		fsread,
	.write		fswrite,
	.stat		fsstat,
	.flush		fsflush,
	.walk1		fswalk1,
	.clone		fsclone,
	.destroyfid	fsclose,
	nil
};

static Ioproc *io9p;

/* copy & paste from /sys/src/libc/9sys/read9pmsg.c
 * changed to use ioreadn instead of readn */
int
read9pmsg(int fd, void *abuf, uint n)
{
	int m, len;
	uchar *buf;

	buf = abuf;

	/* read count */
	m = ioreadn(io9p, fd, buf, BIT32SZ);
	if(m != BIT32SZ){
		if(m < 0)
			return -1;
		return 0;
	}

	len = GBIT32(buf);
	if(len <= BIT32SZ || len > n){
		werrstr("bad length in 9P2000 message header");
		return -1;
	}
	len -= BIT32SZ;
	m = ioreadn(io9p, fd, buf+BIT32SZ, len);
	if(m < len)
		return 0;
	return BIT32SZ+m;
}

/* +1 so we can always zero-terminate a write buffer */
void *emalloc9p(ulong sz) { return emalloc(sz+1); }
void *erealloc9p(void *v, ulong sz) { return erealloc(v, sz+1); }
char *estrdup9p(char *s) { return estrdup(s); }

void
post(char *name, int srvfd)
{
	char buf[80];
	int fd;

	snprint(buf, sizeof buf, "/srv/%s", name);
	fd = create(buf, OWRITE|ORCLOSE|OCEXEC, 0600);
	if(fd < 0)
		panic(buf);
	if(fprint(fd, "%d", srvfd) < 0)
		panic("post");
	putenv("wsys", buf);
	/* leave fd open */
}

/*
 * Build pipe with OCEXEC set on second fd.
 * Can't put it on both because we want to post one in /srv.
 */
int
cexecpipe(int *p0, int *p1)
{
	/* pipe the hard way to get close on exec */
	if(bind("#|", "/mnt/temp", MREPL) == -1)
		return -1;
	*p0 = open("/mnt/temp/data", ORDWR);
	*p1 = open("/mnt/temp/data1", ORDWR|OCEXEC);
	unmount(nil, "/mnt/temp");
	if(*p0<0 || *p1<0)
		return -1;
	return 0;
}

static void
srvthread(void*)
{
	threadsetname("fs");
	srv(&fsys);
}

void
startfs(void)
{
	io9p = ioproc();

	if(cexecpipe(&fsysfd, &fsys.infd) < 0)
		panic("pipe");
	fsys.outfd = fsys.infd;
	user = getuser();
	snprint(srvpipe, sizeof(srvpipe), "lola.%s.%lud", user, (ulong)getpid());
	post(srvpipe, fsysfd);
//	chatty9p++;
	threadcreate(srvthread, nil, mainstacksize);
}

int
fsmount(int id)
{	char buf[32];

	close(fsys.infd);	/* close server end so mount won't hang if exiting */
	snprint(buf, sizeof buf, "%d", id);
	if(mount(fsysfd, -1, "/mnt/wsys", MREPL, buf) == -1){
		fprint(2, "mount failed: %r\n");
		return -1;
	}
	if(bind("/mnt/wsys", "/dev", MBEFORE) == -1){
		fprint(2, "bind failed: %r\n");
		return -1;
	}
	return 0;
}
