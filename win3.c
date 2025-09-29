#include "inc.h"

int bordersz = 4;
int titlesz = 19;
int tabsz = 23;

enum {
	ColDefault,
	ColHilight,
	ColShadow,
	ColTitle,
	ColTitleInact,
	ColTitleText,
	ColTitleTextInact,

	ColFrame,
	ColBorder,
	ColBorderInact,

	NumWinColors
};

Image *wincolors[NumWinColors];
Image *icons[5];

void
winbtn(Image *img, Rectangle r, Image *icon, int down)
{
	draw(img, r, wincolors[ColDefault], nil, ZP);
	if(down){
		borderTL(img, r, wincolors[ColShadow]);
		r = insetrect(r, 1);
	}else{
		winborder(img, r, wincolors[ColHilight], wincolors[ColShadow]);
		r = insetrect(r, 1);
		borderBR(img, r, wincolors[ColShadow]);
	}

	r = centerrect(r, icon->r);
	if(down)
		r = rectaddpt(r, Pt(1,1));
	draw(img, r, icon, icon, ZP);
}

void
winbtnflat(Image *img, Rectangle r, Image *icon, Image *icondown, int down)
{
	if(down){
		draw(img, r, wincolors[ColShadow], nil, ZP);
	}else{
		draw(img, r, wincolors[ColDefault], nil, ZP);
	}

	r = centerrect(r, icon->r);
	if(down)
		icon = icondown;
	draw(img, r, icon, icon, ZP);
}

int
winbtnctl(Image *img, Rectangle r, Image *icon)
{
	int over, prevover;

	prevover = 1;
	winbtn(img, r, icon, 1);
	while(mctl->buttons){
		readmouse(mctl);
		over = ptinrect(mctl->xy, r);
		if(over != prevover)
			winbtn(img, r, icon, over);
		prevover = over;
	}
	if(prevover)
		winbtn(img, r, icon, 0);
	return ptinrect(mctl->xy, r);
}

int
winbtnctlflat(Image *img, Rectangle r, Image *icon, Image *icondown)
{
	int over, prevover;

	prevover = 1;
	winbtnflat(img, r, icon, icondown, 1);
	while(mctl->buttons){
		readmouse(mctl);
		over = ptinrect(mctl->xy, r);
		if(over != prevover)
			winbtnflat(img, r, icon, icondown, over);
		prevover = over;
	}
	if(prevover)
		winbtnflat(img, r, icon, icondown, 0);
	return ptinrect(mctl->xy, r);
}



void
wdecor(Window *w)
{
	if(w->frame == nil)
		return;

	int inact = w != focused;
	Rectangle r;

	if(!w->noborder){
		r = w->rect;
		border(w->frame, r, bordersz, wincolors[ColBorder + inact], ZP);
		border(w->frame, r, 1, wincolors[ColFrame], ZP);
		border(w->frame, insetrect(r,3), 1, wincolors[ColFrame], ZP);

		Rectangle br = rectaddpt(Rect(0,0,1,bordersz), r.min);
		int dx = Dx(r);
		int dy = Dy(r);
		int off = bordersz+titlesz-1;
		draw(w->frame, rectaddpt(br, Pt(off,0)), wincolors[ColFrame], nil, ZP);
		draw(w->frame, rectaddpt(br, Pt(off,dy-bordersz)), wincolors[ColFrame], nil, ZP);
		draw(w->frame, rectaddpt(br, Pt(dx-1-off,0)), wincolors[ColFrame], nil, ZP);
		draw(w->frame, rectaddpt(br, Pt(dx-1-off,dy-bordersz)), wincolors[ColFrame], nil, ZP);

		br = rectaddpt(Rect(0,0,bordersz,1), r.min);
		draw(w->frame, rectaddpt(br, Pt(0,off)), wincolors[ColFrame], nil, ZP);
		draw(w->frame, rectaddpt(br, Pt(dx-bordersz,off)), wincolors[ColFrame], nil, ZP);
		draw(w->frame, rectaddpt(br, Pt(0,dy-1-off)), wincolors[ColFrame], nil, ZP);
		draw(w->frame, rectaddpt(br, Pt(dx-bordersz,dy-1-off)), wincolors[ColFrame], nil, ZP);

		r = insetrect(r, bordersz);
	}

	if(!w->notitle){
		r = w->titlerect;
		r.max.y -= 1;
		draw(w->frame, r, wincolors[ColTitle + inact], nil, ZP);
		draw(w->frame, Rect(r.min.x,r.max.y,r.max.x,r.max.y+1), wincolors[ColFrame], nil, ZP);

		// menu
		Rectangle br = Rect(r.min.x,r.min.y,r.min.x+titlesz-1,r.min.y+titlesz-1);
		winbtnflat(w->frame, br, icons[3], icons[4], 0);
		border(w->frame, insetrect(br,-1), 1, display->black, ZP);

		// max/restore
		br.max.x = r.max.x;
		br.min.x = br.max.x-titlesz+1;
		winbtn(w->frame, br, icons[1+w->maximized], 0);
		border(w->frame, insetrect(br,-1), 1, display->black, ZP);

		// min
		br = rectaddpt(br, Pt(-titlesz,0));
		winbtn(w->frame, br, icons[0], 0);
		border(w->frame, insetrect(br,-1), 1, display->black, ZP);

		if(w->cur){
			int sp = (Dx(r)-stringwidth(font, w->cur->label))/2;
			Point pt = Pt(r.min.x+sp, r.min.y);
			string(w->frame, pt, wincolors[ColTitleText + inact], pt, font, w->cur->label);
		}
	}

	r = w->tabrect;
	draw(w->frame, r, wincolors[ColDefault], nil, ZP);
	draw(w->frame, Rect(r.min.x,r.max.y-1,r.max.x,r.max.y), display->black, nil, ZP);

	int n = w->ref;
	if(n > 1){
		int wd = Dx(r)/n;
		r.max.y -= 1;
		int xxx = r.max.x;
		r.max.x = r.min.x + wd;
		for(WinTab *t = w->tab; t; t = t->next){
			if(t->next == nil)
				r.max.x = xxx;
			if(t == w->cur)
				border(w->frame, insetrect(r,1), 1, display->black, ZP);
			int margin = (tabsz - font->height)/2;
			Point pt = Pt(r.min.x+bordersz/2, r.min.y + margin);
			string(w->frame, pt, display->black, pt, font, t->label);
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
			Rectangle r = w->rect;
			if(!w->noborder)
				r = insetrect(r, bordersz);
			Rectangle br = Rect(0,0,titlesz-1,titlesz-1);
			Rectangle br1 = rectaddpt(br, r.min);
			Rectangle br2 = rectaddpt(br1, Pt(Dx(r)-titlesz+1, 0));
			Rectangle br3 = rectaddpt(br2, Pt(-titlesz, 0));

			if(ptinrect(mctl->xy, br1)){
				if(winbtnctlflat(w->frame, br1, icons[3], icons[4]))
					wdelete(w);
			}else if(ptinrect(mctl->xy, br2)){
				if(winbtnctl(w->frame, br2, icons[1+w->maximized])){
					if(w->maximized)
						wrestore(w);
					else
						wmaximize(w);
				}
			}else if(ptinrect(mctl->xy, br3)){
				if(winbtnctl(w->frame, br3, icons[0]))
					whide(w);
			}else if(!w->maximized)
				grab(w, 1);
		}
		if(mctl->buttons & 4)
			btn3menu();
	}
}


static char minbtn[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char maxbtn[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char rstbtn[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char menubtn[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 6, 0,
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 6, 0,
	0, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static char menubtninv[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0,
	0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 7, 0,
	0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 7, 0,
	0, 0, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};


void
inittheme(void)
{
	background = getcolor("background", 0xC0C7C8FF);

	wincolors[ColDefault] = getcolor("button_face", 0xC0C7C8FF);
	wincolors[ColHilight] = getcolor("button_hilight", 0xFFFFFFFF);
	wincolors[ColShadow] = getcolor("button_shadow", 0x87888FFF);
	wincolors[ColTitle] = getcolor("titlebar_active", 0x5787a8FF);
	wincolors[ColTitleInact] = getcolor("titlebar_inactive", 0xFFFFFFFF);
	wincolors[ColTitleText] = getcolor("titlebar_text_active", 0xFFFFFFFF);
	wincolors[ColTitleTextInact] = getcolor("titlebar_text_inactive", 0x000000FF);
	wincolors[ColFrame] = getcolor("window_frame", 0x000000FF);
	wincolors[ColBorder] = getcolor("border_active", 0xC0C7C8FF);
	wincolors[ColBorderInact] = getcolor("border_inactive", 0xFFFFFFFF);

	icons[0] = mkicon(minbtn, 16, 16);
	icons[1] = mkicon(maxbtn, 16, 16);
	icons[2] = mkicon(rstbtn, 16, 16);
	icons[3] = mkicon(menubtn, 16, 16);
	icons[4] = mkicon(menubtninv, 16, 16);
}
