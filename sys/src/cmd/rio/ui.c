/* move char word line screen buffer */
/* del  char word line */
/* window obj n */
/* n is repeat - derive from prefix */

/* #include <u.h> */
/* #include <libc.h> */
/* #include <draw.h> */
/* #include <thread.h> */
/* #include <cursor.h> */
/* #include <mouse.h> */
/* #include <keyboard.h> */
/* #include <frame.h> */
/* #include <fcall.h> */
/* /\* #include <plumb.h> *\/ */
/* /\* #include <complete.h> *\/ */
/* #include "dat.h" */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ui.h"

/* #define NULL ((void*)0) */

typedef struct {
  char *s;
  void (*f)();
  void (*f1)(Window *w);
  void (*f2)(Window *w, int n);
  int argc;
} Command;

typedef struct keymap Keymap;
typedef struct keymap {
  char *k;
  Keymap **val; // Array of Keymap*
  int n;

  bool iscmd;
  Command *cmd;
} Keymap;

typedef struct {
  char *k, *cmd;
} Keydef;

Window *win;
Keymap *global_map, *cur_map;
int prefix, map_mode;

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
  int q0;
  /* q0 = w->org + frcharofpt(w, Pt(w->Frame.r.min.x, w->Frame.r.min.y + n*w->font->height)); */
  wsetorigin(w, q0, TRUE);
}

void charleft(Window *w, int n) {
  if(w->q0 > 0) {
	int q0 = w->q0 - n;
	wsetselect(w, q0, q0);
	wshow(w, q0);
  }
}

void charright(Window *w, int n) {
  if(w->q1 < w->nr) {
	int q1 = w->q1 + n;
	wsetselect(w, q1, q1);
	wshow(w, q1);
  }
}

void cut(Window *w) {
  printf("cut\n");
  wsnarf(w);
  wcut(w);
}

void copy(Window *w) {
  printf("copy\n");
  wsnarf(w);
}

void paste(Window *w) {
  printf("paste\n");
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

void self_insert(Window *w, Rune s, int n) {
  printf("insert %c\n", s);
	uint q0 = w->q0;
	q0 = winsert(w, &s, n, q0);
	wshow(w, q0 + 1);
}

struct { const char *s; int argc; void (*f); } prim[] = {
  /* {"bol", 1, bol}, {"eol", 1, eol}, {"bob", 1, bob}, {"eob", 1, eob}, */
  /* {"forward-char", 2, charleft}, */
  {"cut", 1, cut}, {"copy", 1, copy}, {"paste", 1, paste},
};

void keymap_set_key(Keymap *map, char k, char *cmd) {
  int n = map->n, match = 0;
  for (int i = 0; prim[i].s && !match; ++i) {
	printf("%d %s\n", i, prim[i].s);
	if (strcmp(prim[i].s, cmd) == 0) {
	  Keymap  *cmap = malloc(sizeof(Keymap));
	  Command *c = malloc(sizeof(Command));
	  c->s = prim[i].s;
	  c->argc = prim[i].argc;
	  switch(prim[i].argc) {
	  case 1:
		c->f1 = prim[i].f;
		break;
	  }
	  cmap->iscmd = 1;
	  cmap->cmd = c;
	  
	  map->k[n] = k;
	  map->val[n] = cmap;
	  map->n = ++n;
	  match = 1;
	}
  }
}

void keymap_reset(void) {
  /* Reset after command execution */
  cur_map = NULL;
  prefix = 0;
  map_mode = 0;
}

void keymap_exec(Rune *seq) {
  Command *cmd = NULL;
  for (int i = 0; seq[i] && !cmd; ++i) {
	if (cur_map) {
	  for (int j = 0; j <= cur_map->n; ++j) {
		if (map_mode == 0 && seq[i] >= '0' && seq[i] <= '9') {
		  prefix = seq[i] - '0';
		  break;
		} else if (seq[i] == cur_map->k[j]) {
		  map_mode = 1;
		  cur_map = cur_map->val[j];
		}
		if (cur_map->iscmd) {
		  cmd = cur_map->cmd;
		  break;
		}
	  }
	} else if (seq[i] == Kctl || seq[i] == Kalt || seq[i] == Kmod4)  {
	  int n;
	  char k = seq[i];
	  if (k == Kctl) {
		n = 0;
	  } else if (k == Kalt) {
		n = 1;
	  } else if (k == Kshift) {
		n = 2;
	  }
	  cur_map = global_map->val[n];
	} else {
	  self_insert(win, seq[i], prefix ? prefix : 1);
	  keymap_reset();
	}
  }
  if (cmd != NULL) {
	switch (cmd->argc) {
	case 1:
	  (*cmd->f1)(win);
	  break;
	case 2:
	  (*cmd->f2)(win, prefix ? prefix : 1);
  	  break;
	}
	keymap_reset();
  } else {
	// No binding
  }
}

Keydef keys[] = {
  {"C-x", "cut"}, {"C-y", "copy"}, {"C-v", "paste"}
};

void keymap_load(Keydef keys[]) {
  for(int i=0; keys[i].k; i++) {
	cur_map = NULL;
	for(int j=0; keys[i].k[j]; j++) {
	  char k = keys[i].k[j];
	  if ((k == '-') || (k == ' '))
		continue;

	  if ((k == 'C') || (k == 'M') || (k == 'S')) {
		int i;
		if (k == 'C') {
		  k = Kctl;
		  i = 0;
		} else if (k == 'M') {
		  k = Kalt;
		  i = 1;
		} else if (k == 'S') {
		  k = Kshift;
		  i = 2;
		}
		if (cur_map == NULL && global_map->val[i] != NULL) {
		  cur_map = global_map->val[i];
		} else {
		  int n = 3;
		  cur_map = malloc(sizeof(Keymap));
		  cur_map->k   = malloc(n * sizeof(int));
		  cur_map->val = malloc(n * sizeof(Keymap));
		  global_map->k[i] = k;
		  global_map->val[i] = cur_map;
		}
	  } else
		keymap_set_key(cur_map, k, keys[i].cmd);
	}
  }
  cur_map = NULL;
}

void main(int argc, char *argv) {
  int n = 3;
  global_map = malloc(sizeof(Keymap));
  global_map->k   = malloc(n * sizeof(int));
  global_map->val = malloc(n * sizeof(Keymap));
  keymap_load(keys);

  char i = 1;
  /* while (i) { */
  /* 	/\* scanf("%s", &i); *\/ */
  /* 	i = getchar(); */
  /* 	keymap_exec(&i); */
  /* } */
  char s[] = {29, 'x', 0};
  keymap_exec(s);
  s[1] = 'y';
  keymap_exec(s);
  s[1] = 'v';
  keymap_exec(s);
}
