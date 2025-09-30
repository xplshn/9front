#include "inc.h"

int bordersz = 4;
int titlesz = 18;
int tabsz = 18;

enum {
	TITLE,
	LTITLE,
	TITLEHOLD,
	LTITLEHOLD,
	TITLETEXT,
	LTITLETEXT,
	TITLEHOLDTEXT,
	LTITLEHOLDTEXT,

	NumWinColors
};

Image *wincolors[NumWinColors];
Image *shadecol;

void
wdecor(Window *w)
{
	if(w->frame == nil)
		return;
	int c = wcolsel(w);
	int tc = TITLE + wcolsel(w);

	Rectangle r;
	int margin;

	if(!w->noborder){
		r = w->rect;
		border(w->frame, r, bordersz, wincolors[tc], ZP);
	}

	if(!w->notitle){
		r = w->titlerect;
		r.max.y = r.min.y + titlesz;
		draw(w->frame, r, wincolors[tc], nil, ZP);

		margin = w->noborder ? titlesz : titlesz + bordersz;
		margin = (margin - font->height)/2;
		Point pt = Pt(r.min.x, w->rect.min.y + margin + 1);
		if(w->cur)
			string(w->frame, pt, wincolors[TITLETEXT+c], pt, font, w->cur->label);
	}

	r = rectsubpt(w->tabrect, Pt(0,1));
	draw(w->frame, r, wincolors[c], nil, ZP);

	int n = w->ref;
	if(n > 1){
		int wd = Dx(r)/n;
		int xxx = r.max.x;
		r.max.x = r.min.x + wd;
		for(WinTab *t = w->tab; t; t = t->next){
			if(t->next == nil)
				r.max.x = xxx;
			if(t != w->cur)
				draw(w->frame, r, shadecol, nil, ZP);
			margin = (tabsz - font->height)/2;
			Point pt = Pt(r.min.x+bordersz/2, r.min.y + margin);
			string(w->frame, pt, wincolors[TITLETEXT+c], pt, font, t->label);
			r = rectaddpt(r, Pt(wd,0));
		}
	}
}

void
wtitlectl(Window *w)
{
	if(mctl->buttons & 7){
		wraise(w);
		wfocus(w);
		if(mctl->buttons & 1) {
			if(!w->maximized)
				grab(w, 1);
		}
		if(mctl->buttons & 4){
			Window *ww = pick();
			if(ww){
				tmigrate(w->cur, ww);
				wraise(ww);
				wfocus(ww);
			}
		}
	}
}

void
inittheme(void)
{
	wincolors[TITLE] = getcolor("title", DGreygreen);
	wincolors[LTITLE] = getcolor("ltitle", DPalegreygreen);
//	wincolors[TITLE] = getcolor("title", 0x2F78EDFF);
//	wincolors[LTITLE] = getcolor("ltitle", 0x7C9DE3FF);

	wincolors[TITLEHOLD] = getcolor("titlehold", DMedblue);
	wincolors[LTITLEHOLD] = getcolor("ltitlehold", DPalegreyblue);


	wincolors[TITLETEXT] = getcolor("titletext", 0xFFFFFFFF);
	wincolors[LTITLETEXT] = getcolor("ltitletext", 0x808080FF);
	wincolors[TITLEHOLDTEXT] = getcolor("titleholdtext", 0xFFFFFFFF);
	wincolors[LTITLEHOLDTEXT] = getcolor("ltitleholdtext", 0xC0C0C0FF);

	shadecol = getcolor(nil, 0x00000020);
}
