/* move char word line screen buffer */
/* del  char word line */
/* window obj n */
/* n is repeat - derive from prefix */
typedef struct {
  int q0, q1, qh, nr, org;
  char *r;
} Window;

typedef struct {
  char *s;
  void (*f)();
  void (*f1)(Window *w);
  void (*f2)(Window *w, int n);
  int argc;
} Command;

typedef struct {
  char k;
  boolean iscmd;
  union val {
	Keymap  map;
	Command cmd;
  }
} Keymap;

Window *win;
Keymap *global_map, cur_map;
int prefix;

void bol(Window *w) {
  int nb;
  if(w->q0 == 0 || w->q0 == w->qh || w->r[w->q0-1] == '\n')
	return;
  nb = wbswidth(w, 0x15 /* ^U */);
  wsetselect(w, w->q0-nb, w->q0-nb);
  wshow(w, w->q0);
}

void eol(Window *w) {
  int q0 = w->q0;
  while(q0 < w->nr && w->r[q0] != '\n')
	q0++;
  wsetselect(w, q0, q0);
  wshow(w, w->q0);
}

void bob(Window *w) {
  wshow(w, 0);
}

void eob(Window *w) {
  wshow(w, w->nr);
}

void point(Window *w) {
  wsetselect(w, w->qh, w->qh);
  wshow(w, w->q0);
}

void lineup(Window *w, int n) {
  int q0 = wbacknl(w, w->org, n);
  wsetorigin(w, q0, TRUE);
}  

void linedown(Window *w, int n) {
  int q0 = w->org + frcharofpt(w, Pt(w->Frame.r.min.x, w->Frame.r.min.y + n*w->font->height));
  wsetorigin(w, q0, TRUE);
}

void charleft(Window *w, int n) {
  if(w->q0 > 0) {
	int q0 = w->q0--;
	wsetselect(w, q0, q0);
	wshow(w, q0);
  }
}

void charright(Window *w, int n) {
  if(w->q1 < w->nr) {
	int q1 = w->q1++;
	wsetselect(w, q1, q1);
	wshow(w, q1);
  }
}

void cut(Window *w) {
  wsnarf(w);
  wcut(w);
}

void copy(Window *w) {
  wsnarf(w);
}

void paste(Window *w) {
  wpaste(w);
}

void delchar(Window *w, Rune r) {
  /* switch(r) */
  /* case Kbs:	/\* ^H: erase character *\/ */
  /* case Knack:	/\* ^U: erase line *\/ */
  /* case Ketb:	/\* ^W: erase word *\/ */
  if(w->q0 == 0 || w->q0 == w->qh)
	return;

  int nb, q0, q1;
  nb = wbswidth(w, r);
  q1 = w->q0;
  q0 = q1 - nb;
  if(q0 < w->org){
	q0 = w->org;
	nb = q1 - q0;
  }
  if(nb > 0){
	wdelete(w, q0, q0 + nb);
	wsetselect(w, q0, q0);
  }
}

void wordf(Window *w, int i) {
  uint p0, p1;

  p0 = w->q0;
  p1 = w->q1;
  if(i > 0)
	while (i-- > 0)
	  while(p0 > 0 && w->r[p0-1] != ' ' && w->r[p0-1] != '\t' && w->r[p0-1] != '\n')
		p0--;
  else
	while (i++ > 0)
	  while(p1 < w->nr && w->r[p1] != ' ' && w->r[p1] != '\t' && w->r[p1] != '\n')
		p1++;
  w->q0 = p0;
  w->q1 = p1;
}

void self_insert(Window *w, char *s) {
	int q0 = w->q0;
	q0 = winsert(w, s, 1, q0);
	wshow(w, q0 + 1);
}

struct { const char *s; int argc; void (*f); } prim[] = {
  {"bol", 1, bol}, {"eol", 1, eol}, {"bob", 1, bob}, {"eob", 1, eob},
  {"forward-char", 2, charleft}
};

void load_keymap(Keymap map) {
  for (int i = 0; prim[i].s; ++i)
	if (strcmp(prim[i].s, map.s)) {
	  map.f = prim[i].f;
	  map.argc = prim[i].argc;
	  switch(prim[i].argc) {
	  case 1:
		map.f1 = prim[i].f;
		break;
	  }
	}
}

void exec_keymap(char *seq) {
  Command cmd = NULL;
  if (cur_map == NULL && seq[0] == Kctl) {
	cur_map = *global_map;
	seq++;
  }
  for (int i = 0; seq[i]; ++i) {
	if (cur_map) {
	if (seq[i] >= '0' && seq[i] <= '9')
	  prefix = seq[i] - '0';
	if (seq[i] == cur_map.k)
	  if (map.iscmd) {
		cmd = cur_map.cmd;
		break;
	  } else
		cur_map = cur_map.map;
	}
  }
  if (cmd != NULL) {
	switch (cmd.argc) {
	case 1:
	  (*cmd.f1)(win);
	  break;
	case 2:
	  (*cmd.f2)(win, prefix ? prefix : 1);
  	  break;
	}
	prefix = 0;
  } else {
	// No binding
  }
}
