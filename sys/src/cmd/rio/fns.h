int	whide(Window*);
int	wunhide(Window*);
int wbswidth(Window *w, Rune c);
void wsetselect(Window *w, uint q0, uint q1);
void wdelete(Window *w, uint q0, uint q1);
uint winsert(Window *w, Rune *r, int n, uint q0);
void wclosewin(Window *w);
void wclosepopup(Window *w);
void		winctl(void*);
void		winshell(void*);
Window*	wlookid(int);
Window*	wmk(Image*, Mousectl*, Channel*, Channel*, int);
Window*	wpointto(Point);
Window*	wtop(Point);
void		wtopme(Window*);
void		wbottomme(Window*);
char*	wcontents(Window*, int*);
int		wclose(Window*);
uint		wbacknl(Window*, uint, uint);
void		wcurrent(Window*);
void		wuncurrent(Window*);
void		wcut(Window*);
void		wpaste(Window*);
void		wplumb(Window*);
void		wlook(Window*);
void		wrlook(Window*);
void		wscrdraw(Window*);
void		wscroll(Window*, int);
void		wsend(Window*);
void		wsendctlmesg(Window*, int, Rectangle, void*);
void		wsetcursor(Window*, int);
void		wsetname(Window*);
void		wsetorigin(Window*, uint, int);
void		wsetpid(Window*, int, int);
void		wshow(Window*, uint);
void		wsnarf(Window*);
void 		wscrsleep(Window*, uint);

Channel*	xfidinit(void);
void		xfidctl(void*);
void		xfidflush(Xfid*);
void		xfidattach(Xfid*);
void		xfidopen(Xfid*);
void		xfidclose(Xfid*);
void		xfidread(Xfid*);
void		xfidwrite(Xfid*);

Filsys*	filsysinit(Channel*);
int		filsysmount(Filsys*, int);
Xfid*		filsysrespond(Filsys*, Xfid*, Fcall*, char*);
void		filsyscancel(Xfid*);

void		deletetimeoutproc(void*);


void	freescrtemps(void);
int	parsewctl(char**, Rectangle, Rectangle*, int*, int*, int*, int*, char**, char*, char*);
int	writewctl(Xfid*, char*);
Window *new(Image*, int, int, int, char*, char*, char**);
void	riosetcursor(Cursor*);
int	min(int, int);
int	max(int, int);
Rune*	strrune(Rune*, Rune);
/* int	isalnum(Rune); */
/* int	isspace(Rune); */
void	timerstop(Timer*);
void	timercancel(Timer*);
Timer*	timerstart(int);
void	error(char*);
void	killprocs(void);
int	shutdown(void*, char*);
void	iconinit(void);
void	*erealloc(void*, uint);
void *emalloc(uint);
char *estrdup(char*);
void	button3menu(void);
void	button2menu(Window*);
void	cvttorunes(char*, int, Rune*, int*, int*, int*);
/* was (byte*,int)	runetobyte(Rune*, int); */
char* runetobyte(Rune*, int, int*);
void	putsnarf(void);
void	getsnarf(void);
void	timerinit(void);
int	goodrect(Rectangle);
int	inborder(Rectangle, Point);

#define	runemalloc(n)		malloc((n)*sizeof(Rune))
#define	runerealloc(a, n)	realloc(a, (n)*sizeof(Rune))
#define	runemove(a, b, n)	memmove(a, b, (n)*sizeof(Rune))
