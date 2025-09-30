#include "inc.h"

/*
 * TODO: i feel like this could use some cleanup
 */

char	Ebadwr[]		= "bad rectangle in wctl request";
char	Ewalloc[]		= "window allocation failed in wctl request";

/* >= Top are disallowed if mouse button is pressed.
 * > New need a window. */
enum
{
	Screenoffset,
	Screenrefresh,
	New,
	Newtab,
	Resize,
	Move,
	Scroll,
	Noscroll,
	Border,
	Noborder,
	Title,
	Notitle,
	Sticky,
	Nosticky,
	Set,
	Top,
	Bottom,
	Current,
	Hide,
	Unhide,
	Delete,
};

static char *cmds[] = {
	[Screenoffset] = "screenoffset",
	[Screenrefresh] = "refresh",
	[New]	= "new",
	[Newtab]	= "newtab",
	[Resize]	= "resize",
	[Move]	= "move",
	[Scroll]	= "scroll",
	[Noscroll]	= "noscroll",
	[Border]	= "border",
	[Noborder]	= "noborder",
	[Title]	= "title",
	[Notitle]	= "notitle",
	[Sticky]	= "sticky",
	[Nosticky]	= "nosticky",
	[Set]		= "set",
	[Top]	= "top",
	[Bottom]	= "bottom",
	[Current]	= "current",
	[Hide]	= "hide",
	[Unhide]	= "unhide",
	[Delete]	= "delete",
	nil
};

enum
{
	Cd,
	Deltax,
	Deltay,
	Hidden,
	Id,
	Maxx,
	Maxy,
	Minx,
	Miny,
	PID,
	R,
	Scrolling,
	Noscrolling,
};

static char *params[] = {
	[Cd]	 			= "-cd",
	[Deltax]			= "-dx",
	[Deltay]			= "-dy",
	[Hidden]			= "-hide",
	[Id]				= "-id",
	[Maxx]			= "-maxx",
	[Maxy]			= "-maxy",
	[Minx]			= "-minx",
	[Miny]			= "-miny",
	[PID]				= "-pid",
	[R]				= "-r",
	[Scrolling]			= "-scroll",
	[Noscrolling]		= "-noscroll",
	nil
};

static int
word(char **sp, char *tab[])
{
	char *s, *t;
	int i;

	s = *sp;
	while(isspacerune(*s))
		s++;
	t = s;
	while(*s!='\0' && !isspacerune(*s))
		s++;
	for(i=0; tab[i]!=nil; i++)
		if(s-t > 0 && strncmp(tab[i], t, s-t) == 0){
			*sp = s;
			return i;
		}
	return -1;
}

int
set(int sign, int neg, int abs, int pos)
{
	if(sign < 0)
		return neg;
	if(sign > 0)
		return pos;
	return abs;
}

void
shift(int *minp, int *maxp, int min, int max)
{
	if(*maxp > max){
		*minp += max-*maxp;
		*maxp = max;
	}
	if(*minp < min){
		*maxp += min-*minp;
		if(*maxp > max)
			*maxp = max;
		*minp = min;
	}
}

Rectangle
rectonscreen(Rectangle r)
{
//TODO(vdesk) this changes
return r;
	shift(&r.min.x, &r.max.x, screen->r.min.x, screen->r.max.x);
	shift(&r.min.y, &r.max.y, screen->r.min.y, screen->r.max.y);
	return r;
}

/* permit square brackets, in the manner of %R */
int
riostrtol(char *s, char **t)
{
	int n;

	while(*s!='\0' && (*s==' ' || *s=='\t' || *s=='['))
		s++;
	if(*s == '[')
		s++;
	n = strtol(s, t, 10);
	if(*t != s)
		while((*t)[0] == ']')
			(*t)++;
	return n;
}

Wctlcmd
parsewctl(char *s, Rectangle r)
{
	Wctlcmd cmd;

	int n, nt, param, xy, sign;
	char *f[2], *t;

	cmd.id = 0;
	cmd.pid = 0;
	cmd.hidden = FALSE;
	cmd.scrolling = scrolling;
	cmd.dir = nil;
	cmd.error = nil;
	cmd.cmd = word(&s, cmds);
	if(cmd.cmd < 0)
		goto Lose;
	switch(cmd.cmd){
	case Screenoffset:
		r = ZR;
		break;
	case New:
		r = newrect();
		break;
	}

	while((param = word(&s, params)) >= 0){
		switch(param){	/* special cases */
		case Hidden:
			cmd.hidden = TRUE;
			continue;
		case Scrolling:
			cmd.scrolling = TRUE;
			continue;
		case Noscrolling:
			cmd.scrolling = FALSE;
			continue;
		case R:
			r.min.x = riostrtol(s, &t);
			if(t == s)
				goto Lose;
			s = t;
			r.min.y = riostrtol(s, &t);
			if(t == s)
				goto Lose;
			s = t;
			r.max.x = riostrtol(s, &t);
			if(t == s)
				goto Lose;
			s = t;
			r.max.y = riostrtol(s, &t);
			if(t == s)
				goto Lose;
			s = t;
			continue;
		}
		while(isspacerune(*s))
			s++;
		if(param == Cd){
			cmd.dir = s;
			if((nt = gettokens(cmd.dir, f, nelem(f), " \t\r\n\v\f")) < 1)
				goto Lose;
			n = strlen(cmd.dir);
			if(cmd.dir[0] == '\'' && cmd.dir[n-1] == '\'')
				(cmd.dir++)[n-1] = '\0'; /* drop quotes */
			s += n+(nt-1);
			continue;
		}
		sign = 0;
		if(*s == '-'){
			sign = -1;
			s++;
		}else if(*s == '+'){
			sign = +1;
			s++;
		}
		if(!isdigitrune(*s))
			goto Lose;
		xy = riostrtol(s, &s);
		switch(param){
		case -1:
			cmd.error = "unrecognized wctl parameter";
			return cmd;
		case Minx:
			r.min.x = set(sign, r.min.x-xy, xy, r.min.x+xy);
			break;
		case Miny:
			r.min.y = set(sign, r.min.y-xy, xy, r.min.y+xy);
			break;
		case Maxx:
			r.max.x = set(sign, r.max.x-xy, xy, r.max.x+xy);
			break;
		case Maxy:
			r.max.y = set(sign, r.max.y-xy, xy, r.max.y+xy);
			break;
		case Deltax:
			r.max.x = set(sign, r.max.x-xy, r.min.x+xy, r.max.x+xy);
			break;
		case Deltay:
			r.max.y = set(sign, r.max.y-xy, r.min.y+xy, r.max.y+xy);
			break;
		case Id:
			cmd.id = xy;
			break;
		case PID:
			cmd.pid = xy;
			break;
		}
	}
	if(cmd.cmd == Screenoffset)
		cmd.r = r;
	else
		cmd.r = rectonscreen(rectaddpt(r, screen->r.min));
	while(isspacerune(*s))
		s++;
	if(cmd.cmd != New && cmd.cmd != Newtab && *s != '\0'){
		cmd.error = "extraneous text in wctl message";
		return cmd;
	}
	cmd.args = s;
	return cmd;
Lose:
	cmd.error = "unrecognized wctl command";
	return cmd;
}

char*
wctlcmd(WinTab *w, Rectangle r, int cmd)
{
	Window *ww = w->w;
	switch(cmd){
	case Move:
		r = rectaddpt(Rect(0,0,Dx(ww->frame->r),Dy(ww->frame->r)), r.min);
		if(!goodrect(r))
			return Ebadwr;
		if(!eqpt(r.min, ww->frame->r.min))
			wmove(ww, r.min);
		break;
	case Resize:
		if(!goodrect(r))
			return Ebadwr;
		if(!eqrect(r, ww->frame->r))
			wresize(ww, r);
		break;
// TODO: these three work somewhat differently in rio
	case Top:
		wraise(ww);
		break;
	case Bottom:
		wlower(ww);
		break;
	case Current:
		if(ww->hidden)
			return "window is hidden";
		wfocus(ww);
		wraise(ww);
		break;
	case Hide:
		switch(whide(ww)){
		case -1: return "window already hidden";
		case 0: return "hide failed";
		}
		break;
	case Unhide:
		switch(wunhide(ww)){
		case -1: return "window not hidden";
		case 0: return "hide failed";
		}
		break;
	case Delete:
		wdelete(ww);
		break;
	case Scroll:
		w->scrolling = TRUE;
		xshow(&w->text, w->text.nr);
		wsendmsg(w, Wakeup);
		break;
	case Noscroll:
		w->scrolling = FALSE;
		wsendmsg(w, Wakeup);
		break;
	case Border:
		ww->noborder &= ~1;
		wrecreate(ww);
		break;
	case Noborder:
		ww->noborder |= 1;
		wrecreate(ww);
		break;
	case Title:
		ww->notitle = FALSE;
		wrecreate(ww);
		break;
	case Notitle:
		ww->notitle = TRUE;
		wrecreate(ww);
		break;
	case Sticky:
		ww->sticky = TRUE;
		break;
	case Nosticky:
		ww->sticky = FALSE;
		break;
	default:
		return "invalid wctl message";
	}
	return nil;
}

char*
wctlnew(WinTab *w, Wctlcmd cmd)
{
	char *argv[4], **args;

	args = nil;
	if(cmd.pid == 0){
		argv[0] = "rc";
		argv[1] = "-c";
		while(isspacerune(*cmd.args))
			cmd.args++;
		if(*cmd.args == '\0'){
			argv[1] = "-i";
			argv[2] = nil;
		}else{
			argv[2] = cmd.args;
			argv[3] = nil;
		}
		args = argv;
	}
	if(wincmd(w, cmd.pid, cmd.dir, args) == 0)
		return "window creation failed";		
	return nil;
}

char*
writewctl(WinTab *w, char *data)
{
	Rectangle r;
	Wctlcmd cmd;

	if(w == nil)
		r = ZR;
	else
		r = rectsubpt(w->w->frame->r, screen->r.min);
	cmd = parsewctl(data, r);
	if(cmd.error)
		return cmd.error;

	if(cmd.id != 0){
		w = wfind(cmd.id);
		if(w == nil)
			return "no such window id";
	}

	if(w == nil && cmd.cmd > New)
		return "command needs to be run within a window";

	switch(cmd.cmd){
	case Screenoffset:
		screenoffset(cmd.r.min.x, cmd.r.min.y);
		return nil;
	case Screenrefresh:
		refresh();
		return nil;
	case New:
		w = wtcreate(cmd.r, cmd.hidden, cmd.scrolling);
		if(w == nil)
			return "window creation failed";
		return wctlnew(w, cmd);
	case Newtab:
		w = tcreate(w->w, cmd.scrolling);
		if(w == nil)
			return "window creation failed";
		return wctlnew(w, cmd);
	case Set:
		if(cmd.pid > 0)
			wsetpid(w, cmd.pid, 0);
		return nil;
	default:
		incref(w);
		cmd.error = wctlcmd(w, cmd.r, cmd.cmd);
		wrelease(w);
		return cmd.error;
	}
}
