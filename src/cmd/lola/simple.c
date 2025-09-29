#include "inc.h"

int bordersz = 4;
int titlesz = 17;//19;
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
	FRAME,
	LFRAME,

	NumWinColors
};

Image *wincolors[NumWinColors];
Image *icons[4];

Image *shadecol;

void
btn(Image *img, Rectangle r, Image *col, Image *icon, int down)
{
	USED(down);

	r = centerrect(r, icon->r);
	draw(img, r, col, icon, ZP);
}

int
btnctl(Image *img, Rectangle r, Image *col, Image *icon)
{
	int over, prevover;

	prevover = 1;
	btn(img, r, col, icon, 1);
	while(mctl->buttons){
		readmouse(mctl);
		over = ptinrect(mctl->xy, r);
		if(over != prevover)
			btn(img, r, col, icon, over);
		prevover = over;
	}
	if(prevover)
		btn(img, r, col, icon, 0);
	return ptinrect(mctl->xy, r);
}

void
wdecor(Window *w)
{
	if(w->frame == nil)
		return;
	int sel = wcolsel(w);
	int c = TITLE + sel;
	int c1 = TITLETEXT + sel;
	int c2 = FRAME + (sel&1);

	Rectangle r, b1, b2, b3;
	int margin;

	if(!w->noborder){
		r = w->rect;
		border(w->frame, r, bordersz, wincolors[c], ZP);
		border(w->frame, r, 1, wincolors[c2], ZP);
	}

	if(!w->notitle){
		r = w->titlerect;
		draw(w->frame, r, wincolors[c], nil, ZP);

		b1 = r;
		b1.max.x -= bordersz/2;
		b1.min.x = b1.max.x - titlesz + bordersz;
		b1.max.y = b1.min.y + Dx(b1);
		b2 = rectsubpt(b1, Pt(titlesz, 0));
		b3 = rectsubpt(b2, Pt(titlesz, 0));
		btn(w->frame, b1, wincolors[c1], icons[3], 0);
		btn(w->frame, b2, wincolors[c1], icons[1+w->maximized], 0);
		btn(w->frame, b3, wincolors[c1], icons[0], 0);

		margin = w->noborder ? titlesz : titlesz + bordersz;
		margin = (margin - font->height)/2;
		Point pt = Pt(r.min.x, w->rect.min.y + margin + 1);
		if(w->cur)
			string(w->frame, pt, wincolors[c1], pt, font, w->cur->label);
	}
	border(w->frame, insetrect(w->contrect,-1), 1, wincolors[c2], ZP);

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
			string(w->frame, pt, wincolors[c1], pt, font, t->label);
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
			int c = TITLETEXT + wcolsel(w);

			Rectangle r = w->titlerect;
			r.max.x -= bordersz/2;
			Rectangle br = Rect(0,0,titlesz-bordersz,titlesz-bordersz);
			Rectangle br1 = rectaddpt(br, Pt(r.max.x-titlesz+bordersz, r.min.y));
			Rectangle br2 = rectsubpt(br1, Pt(titlesz, 0));
			Rectangle br3 = rectsubpt(br2, Pt(titlesz, 0));

			if(ptinrect(mctl->xy, br1)){
				if(btnctl(w->frame, br1, wincolors[c], icons[3]))
					wdelete(w);
			}else if(ptinrect(mctl->xy, br2)){
				if(btnctl(w->frame, br2, wincolors[c], icons[1+w->maximized])){
					if(w->maximized)
						wrestore(w);
					else
						wmaximize(w);
				}
			}else if(ptinrect(mctl->xy, br3)){
				if(btnctl(w->frame, br3, wincolors[c], icons[0]))
					whide(w);
			}else if(!w->maximized)
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

static char minbtn[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char maxbtn[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char rstbtn[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char closebtn[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void
inittheme(void)
{
	freeimage(colors[HOLDTEXT]);
	freeimage(colors[PALEHOLDTEXT]);
	colors[HOLDTEXT] = getcolor("holdtext", 0x990000FF);
	colors[PALEHOLDTEXT] = getcolor("paleholdtext", 0xBB5D00FF);

//	wincolors[TITLE] = getcolor("title", 0x607DA1FF);
//	wincolors[LTITLE] = getcolor("ltitle", 0xA1A1A1FF);

//	wincolors[TITLE] = getcolor("title", 0x2F78EDFF);
//	wincolors[LTITLE] = getcolor("ltitle", 0x7C9DE3FF);

	wincolors[TITLE] = getcolor("title", 0x5297F9FF);
	wincolors[LTITLE] = getcolor("ltitle", 0x2C60B2FF);
	wincolors[TITLEHOLD] = getcolor("titlehold", 0xED2F2FFF);
	wincolors[LTITLEHOLD] = getcolor("ltitlehold", 0xE36A6AFF);

	wincolors[FRAME] = getcolor("frame", 0x000000FF);
	wincolors[LFRAME] = getcolor("lframe", 0x000000FF);

	wincolors[TITLETEXT] = getcolor("titletext", 0xFFFFFFFF);
	wincolors[LTITLETEXT] = getcolor("ltitletext", 0xFFFFFFFF);
	wincolors[TITLEHOLDTEXT] = getcolor("titleholdtext", 0xFFFFFFFF);
	wincolors[LTITLEHOLDTEXT] = getcolor("ltitleholdtext", 0xFFFFFFFF);

	icons[0] = mkicon(minbtn, 16, 13);
	icons[1] = mkicon(maxbtn, 16, 13);
	icons[2] = mkicon(rstbtn, 16, 13);
	icons[3] = mkicon(closebtn, 16, 13);

	shadecol = getcolor(nil, 0x00000020);
}
