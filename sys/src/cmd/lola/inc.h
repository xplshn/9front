#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <frame.h>
#include <fcall.h>
#include <9p.h>
#include <complete.h>
#include <plumb.h>

typedef uchar bool;
enum {
	FALSE = 0,
	TRUE = 1,

	BIG = 3,
	MAXWINDOWS = 1000
};

#define ALT(c, v, t) (Alt){ c, v, t, nil, nil, 0 }

#define CTRL(c) ((c)&0x1F)


typedef struct RKeyboardctl RKeyboardctl;
struct RKeyboardctl
{
	Keyboardctl;
	int kbdfd;
};
RKeyboardctl *initkbd(char *file, char *kbdfile);


extern Rune *snarf;
extern int nsnarf;
extern int snarfversion;
extern int snarffd;
enum { MAXSNARF = 100*1024 };
void putsnarf(void);
void getsnarf(void);
void setsnarf(char *s, int ns);

typedef struct Text Text;
struct Text
{
	Frame;
	Rectangle scrollr, lastsr;
	Image *i;
	Rune *r;
	uint nr;
	uint maxr;
	uint org;	/* start of Frame's text */
	uint q0, q1;	/* selection */
	uint qh;	/* host point, output here */

	/* not entirely happy with this in here */
	bool rawmode;
	Rune *raw;
	int nraw;

	int posx;
};

void xinit(Text *x, Rectangle textr, Rectangle scrollr, int tabwidth, Font *ft, Image *b, Image **cols);
void xsetrects(Text *x, Rectangle textr, Rectangle scrollr);
void xclear(Text *x);
void xredraw(Text *x);
void xfullredraw(Text *x);
uint xinsert(Text *x, Rune *r, int n, uint q0);
void xfill(Text *x);
void xdelete(Text *x, uint q0, uint q1);
void xsetselect(Text *x, uint q0, uint q1);
void xselect(Text *x, Mousectl *mc);
void xscrdraw(Text *x);
void xscroll(Text *x, Mousectl *mc, int but);
void xscrolln(Text *x, int n);
void xshow(Text *x, uint q0);
void xplacetick(Text *x, uint q);
void xtype(Text *x, Rune r);
int xninput(Text *x);
void xaddraw(Text *x, Rune *r, int nr);
void xlook(Text *x);
void xsnarf(Text *x);
void xcut(Text *x);
void xpaste(Text *x);
void xsend(Text *x);
int xplumb(Text *w, char *src, char *dir, int maxsize);

enum
{
	// NCOL is defined by libframe, add more after it
	PALETEXT = NCOL,
	HOLDTEXT,
	PALEHOLDTEXT,

	NumColors
};

extern Image *background;
extern Image *colors[NumColors];
extern Cursor whitearrow;
extern Cursor query;
extern Cursor crosscursor;
extern Cursor boxcursor;
extern Cursor sightcursor;
extern Cursor *corners[9];
void initdata(void);

extern int tabwidth;
extern bool scrolling;
extern bool notitle;
extern int ndeskx;
extern int ndesky;

extern Screen *wscreen;
extern Image *fakebg;
extern Mousectl *mctl;
extern char *startdir;
extern bool shiftdown, ctldown;
extern bool gotscreen;
extern bool servekbd;


typedef struct RuneConvBuf RuneConvBuf;
struct RuneConvBuf
{
	char *buf;
	int maxbuf;	// allocated size
	int nb;		// size
	int n;		// filled
};

typedef struct Stringpair Stringpair;
struct Stringpair	/* rune and nrune or byte and nbyte */
{
	void		*s;
	int		ns;
};

typedef struct Mousestate Mousestate;
struct Mousestate
{
	Mouse;
	ulong	counter;	/* serial no. of mouse event */
};

typedef struct Mousequeue Mousequeue;
struct Mousequeue
{
	Mousestate	q[16];
	int	ri;	/* read index into queue */
	int	wi;	/* write index */
	ulong	counter;	/* serial no. of last mouse event we received */
	ulong	lastcounter;	/* serial no. of last mouse event sent to client */
	int	lastb;	/* last button state we received */
	bool	full;	/* filled the queue; no more recording until client comes back */	
};

typedef struct Queue Queue;
struct Queue
{
	char *q[32];
	int ri;
	int wi;
	bool full;
};
int qadd(Queue *q, char *data);
char *qget(Queue *q);
int qempty(Queue *q);

enum
{
	Resized,
	Deleted,
	Refresh,
	Holdon,
	Holdoff,
	Rawon,
	Rawoff,
	Wakeup
};

extern int bordersz;
extern int titlesz;
extern int tabsz;

typedef struct Window Window;
typedef struct WinTab WinTab;

struct Window
{
	Ref;
	Window *lower;
	Window *higher;
	bool hidden;
	Image *frame;
	Screen *screen;
	int noborder;
	bool notitle;
	bool maximized;
	bool sticky;
	Rectangle rect;
	Rectangle titlerect;
	Rectangle tabrect;
	Rectangle contrect;
	Rectangle scrollr;
	Rectangle textr;
	Rectangle origrect;

	// tmp
	WinTab *tab;
	WinTab *cur;
};

struct WinTab
{
	Ref;
	bool deleted;
	Window *w;
	WinTab *next;
	Image *content;
	int id;
	char name[32];
	int namecount;
	char *label;
	int notefd;
	char *dir;

	Text text;
	int holdmode;
	bool scrolling;
	bool wctlready;
	bool wctlopen;

	Mousectl mc;
	Mousequeue mq;
	int mouseopen;
	int resized;

	Cursor *cursorp;
	Cursor cursor;

	Channel *kbd;
	Queue kq;
	bool consctlopen;
	bool kbdopen;

	Channel *gone;		/* window gone */
	Channel *ctl;		/* Wctlmesg */
	/* channels to xreqs */
	Channel *conswrite;
	Channel *consread;
	Channel *kbdread;
	Channel *mouseread;
	Channel *wctlread;
	Channel *complete;

	char *threadname;	/* for debugging */
};

extern Window *bottomwin, *topwin;
extern Window *windows[MAXWINDOWS];
extern int nwindows;
extern WinTab *wintabs[MAXWINDOWS];
extern int nwintabs;
extern Window *focused, *cursorwin;
extern Point screenoff;

Window *wcreate(Rectangle r, bool hidden);
int wcolsel(Window *w);
void wtitlectl(Window *w);
void wdecor(Window *w);
void wmaximize(Window *w);
void wrestore(Window *w);
void wresize(Window *w, Rectangle r);
void wrecreate(Window *w);
Window *wpointto(Point pt);
void wdelete(Window *w);
void wmove(Window *w, Point pos);
void wraise(Window *w);
void wlower(Window *w);
void wfocus(Window *w);
void wunfocus(Window *w);
int whide(Window *w);
int wunhide(Window *w);
void wmovemouse(Window *w, Point pt, bool force);

void wrelease(WinTab *w);
void wsendmsg(WinTab *w, int type);
WinTab *wfind(int id);
void wsetcursor(WinTab *w);
void wsetlabel(WinTab *w, char *label);
void wsetname(WinTab *w);
void wsetpid(WinTab *w, int pid, int dolabel);
void wsethold(WinTab *w, int hold);
void wtype(WinTab *w, Rune r);
int wincmd(WinTab *w, int pid, char *dir, char **argv);

WinTab *tcreate(Window *w, bool scrolling);
void tfocus(WinTab *t);
void tdelete(WinTab *t);
void tmigrate(WinTab *t, Window *w);
void tmoveleft(WinTab *t);
void tmoveright(WinTab *t);

WinTab *wtcreate(Rectangle r, bool hidden, bool scrolling);

void screenoffset(int offx, int offy);

typedef struct Wctlcmd Wctlcmd;
struct Wctlcmd
{
	int cmd;
	Rectangle r;
	char *args;
	int pid;
	int id;
	bool hidden;
	bool scrolling;
	char *dir;
	char *error;
};

Wctlcmd parsewctl(char *s, Rectangle r);
char *writewctl(WinTab *w, char *data);


extern Cursor *cursor;
void setcursoroverride(Cursor *c, int ov);
void setcursornormal(Cursor *c);

Rectangle newrect(void);
int goodrect(Rectangle r);
Rectangle centerrect(Rectangle r, Rectangle s);
void borderTL(Image *img, Rectangle r, Image *c);
void borderBR(Image *img, Rectangle r, Image *c);
void winborder(Image *img, Rectangle r, Image *c1, Image *c2);

void refresh(void);
Point dmenuhit(int but, Mousectl *mc, int nx, int ny, Point last);
void drainmouse(Mousectl *mc, WinTab *w);
Window *pick(void);
void grab(Window *w, int btn);
void btn3menu(void);

void inittheme(void);
Image *getcolor(char *name, ulong defcol);
Image *mkicon(char *px, int w, int h);


extern Channel *opentap;	/* open fromtap or totap */
extern Channel *closetap;	/* close fromtap or totap */
extern Channel *fromtap;	/* input from kbd tap program to window */
extern Channel *totap;		/* our keyboard input to tap program */

extern Channel *pickchan;


extern Srv fsys;
void startfs(void);
int fsmount(int id);

#define	runemalloc(n)		malloc((n)*sizeof(Rune))
#define	runerealloc(a, n)	realloc(a, (n)*sizeof(Rune))
#define	runemove(a, b, n)	memmove(a, b, (n)*sizeof(Rune))
#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) > (b) ? (a) : (b))

void panic(char *s);
void *emalloc(ulong size);
void *erealloc(void *p, ulong size);
char *estrdup(char *s);
int handlebs(Stringpair *pair);
void cnvsize(RuneConvBuf *cnv, int nb);
int r2bfill(RuneConvBuf *cnv, Rune *rp, int nr);
void r2bfinish(RuneConvBuf *cnv, Stringpair *pair);
Stringpair b2r(RuneConvBuf *cnv);


typedef struct Timer Timer;
struct Timer
{
	int		dt;
	int		cancel;
	Channel	*c;	/* chan(int) */
	Timer	*next;
};
void timerinit(void);
Timer *timerstart(int dt);
void timerstop(Timer *t);
void timercancel(Timer *t);
