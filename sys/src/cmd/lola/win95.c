#include "inc.h"

int bordersz = 4;
int titlesz = 19;
int tabsz = 20;

enum {
	ColDefault,
	ColLight1,
	ColLight2,
	ColDark1,
	ColDark2,
	ColTitle,
	ColTitleInact,
	ColTitleText,
	ColTitleTextInact,

	NumWinColors
};

Image *wincolors[NumWinColors];
Image *icons[5];

void
winbtn(Image *img, Rectangle r, Image *icon, int down)
{
	if(down){
		winborder(img, r, wincolors[ColDark2], wincolors[ColLight2]);
		r = insetrect(r, 1);
		winborder(img, r, wincolors[ColDark1], wincolors[ColLight1]);
	}else{
		winborder(img, r, wincolors[ColLight2], wincolors[ColDark2]);
		r = insetrect(r, 1);
		winborder(img, r, wincolors[ColLight1], wincolors[ColDark1]);
	}
	r = insetrect(r, 1);
	draw(img, r, wincolors[ColDefault], nil, ZP);

	r = insetrect(r,-2);
	if(down)
		r = rectaddpt(r, Pt(1,1));
	draw(img, r, icon, icon, ZP);
}

void
winframe(Image *img, Rectangle r)
{
	winborder(img, r, wincolors[ColLight1], wincolors[ColDark2]);
	r = insetrect(r, 1);
	winborder(img, r, wincolors[ColLight2], wincolors[ColDark1]);
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

void
wdecor(Window *w)
{
	if(w->frame == nil)
		return;

	int inact = w != focused;
	Rectangle r;

	if(!w->noborder){
		r = w->rect;
		border(w->frame, r, bordersz, wincolors[ColDefault], ZP);
		winframe(w->frame, r);
	}

	if(!w->notitle){
		r = w->titlerect;
		r.max.y -= 1;
		draw(w->frame, r, wincolors[ColTitle + inact], nil, ZP);
		draw(w->frame, Rect(r.min.x,r.max.y,r.max.x,r.max.y+1), wincolors[ColDefault], nil, ZP);

		// draw buttons
		Rectangle br = insetrect(r, 2);
		br.min.x = br.max.x - Dy(br) - 2;
		winbtn(w->frame, br, icons[3], 0);
		br = rectaddpt(br, Pt(-Dx(br)-2, 0));
		winbtn(w->frame, br, icons[1+w->maximized], 0);
		br = rectaddpt(br, Pt(-Dx(br), 0));
		winbtn(w->frame, br, icons[0], 0);

		br = rectaddpt(icons[4]->r, insetrect(r,1).min);
		draw(w->frame, br, icons[4], icons[4], ZP);

		Point pt = Pt(r.min.x + 2 + titlesz-1, r.min.y);
		if(w->cur)
			string(w->frame, pt, wincolors[ColTitleText + inact], pt, font, w->cur->label);
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
			Rectangle r = w->titlerect;
			r.max.y -= 1;
			Rectangle br1 = insetrect(r, 2);
			br1.min.x = br1.max.x - Dy(br1) - 2;
			Rectangle br2 = rectaddpt(br1, Pt(-Dx(br1)-2, 0));
			Rectangle br3 = rectaddpt(br1, Pt(-2*Dx(br1)-2, 0));

			if(ptinrect(mctl->xy, br1)){
				if(winbtnctl(w->frame, br1, icons[3]))
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
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
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
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char closebtn[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char appbtn[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1,
	0, 0, 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 1,
	0, 0, 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 1,
	0, 0, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 1,
	0, 0, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1,
	0, 0, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1,
	0, 0, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1,
	0, 0, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1,
	0, 0, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1,
	0, 0, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1,
	0, 0, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1,
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void
inittheme(void)
{
	background = getcolor("background", 0x008080FF);

	wincolors[ColDefault] = getcolor("3d_face", 0xC0C0C0FF);
//	wincolors[ColLight1] = getcolor("3d_hilight1", 0xDFDFDFFF);
	wincolors[ColLight1] = getcolor("3d_hilight1", 0xC0C0C0FF);
	wincolors[ColLight2] = getcolor("3d_hilight2", 0xFFFFFFFF);
	wincolors[ColDark1] = getcolor("3d_shadow1", 0x808080FF);
	wincolors[ColDark2] = getcolor("3d_shadow2", 0x000000FF);
	wincolors[ColTitle] = getcolor("titlebar_active", 0x000080FF);
	wincolors[ColTitleInact] = getcolor("titlebar_inactive", 0x808080FF);
	wincolors[ColTitleText] = getcolor("titlebar_text_active", 0xFFFFFFFF);
	wincolors[ColTitleTextInact] = getcolor("titlebar_text_inactive", 0xC0C0C0FF);

	icons[0] = mkicon(minbtn, 16, 14);
	icons[1] = mkicon(maxbtn, 16, 14);
	icons[2] = mkicon(rstbtn, 16, 14);
	icons[3] = mkicon(closebtn, 16, 14);
	icons[4] = mkicon(appbtn, 16, 16);
}
