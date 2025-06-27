/* move char word line screen buffer */
/* del  char word line */
/* window obj n */
/* n is repeat - derive from prefix */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
/* #include <plumb.h> */
/* #include <complete.h> */
#include "dat.h"
#include "fns.h"

#define NULL ((void*)0)

int wbswidth(Window *w, Rune c);
void wsetselect(Window *w, uint q0, uint q1);
void wdelete(Window *w, uint q0, uint q1);
uint winsert(Window *w, Rune *r, int n, uint q0);

typedef int bool;
/* #define NULL ((void*)0) */

typedef struct {
  const char *s;
  void (*f0)(void);
  void (*f1)(Window *w);
  void (*f2)(Window *w, int n);
  int argc;
} Command;

typedef struct keymap Keymap;
typedef struct keymap {
  Rune *k;
  Keymap **val; // Array of Keymap*
  int n, len;

  bool iscmd;
  Command *cmd;
} Keymap;

/* typedef struct { */
/*   char *k, *cmd; */
/* } Keydef; */

/* enum { */
/*   Mctl  , */
/*   Malt  , */
/*   Mshift, */
/*   Mesc  , */
/*   Mmod4 , */
/* }; */

extern Window* input;
Keymap *global_map, *cur_map;
int prefix, map_mode;

int runecmp(Rune r1, Rune r2) {
  char str[4];
  if (runetochar(str, &r1) == Runeerror)
	return 0;
  return utfrune(str, r2) != nil;
}

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
  print("left\n");
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
  print("cut\n");
  /* wsnarf(w); */
  /* wcut(w); */
}

void copy(Window *w) {
  print("copy\n");
  /* wsnarf(w); */
}

void paste(Window *w) {
  print("paste\n");
  /* wpaste(w); */
}

/* void exit() { */
/* 	int f; */

/* 	wsupdate(); */
/* 	if(wcur == nil || (f = wwctl(wcur->id, OWRITE)) < 0) */
/* 		return; */
/* 	fprint(f, "delete"); */
/* 	close(f); */
/* } */

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
  print("insert %c\n", s);
  uint q0 = w->q0;
  q0 = winsert(w, &s, n, q0);
  wshow(w, q0 + 1);
}

void confirmexit(void);

struct { const char *s; int argc; void (*f); } prim[] = {
  /* {"bol", 1, bol}, {"eol", 1, eol}, {"bob", 1, bob}, {"eob", 1, eob}, */
  {"charleft", 2, charleft},
  {"cut", 1, cut}, {"copy", 1, copy}, {"paste", 1, paste},
  {"exit", 0, confirmexit}, nil
};

void keymap_set_key(Keymap *map, Rune k, char *cmd) {
  int n = map->n, match = 0;
  for (int i = 0; prim[i].s && !match; ++i) {
	print("%d %s\n", i, prim[i].s);
	if (strcmp(prim[i].s, cmd) == 0) {
	  Keymap  *cmap = emalloc(sizeof(Keymap));
	  Command *c = emalloc(sizeof(Command));
	  c->s = prim[i].s;
	  c->argc = prim[i].argc;
	  switch(prim[i].argc) {
	  case 0:
		c->f0 = prim[i].f;
		break;
	  case 1:
		c->f1 = prim[i].f;
		break;
	  case 2:
		c->f2 = prim[i].f;
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

struct {char *s; Rune r;} keys_mapped[] = { {"C", Kctl}, {"left", Kleft} };
/* , Kalt, Kshift, Kleft, Kright, Kup, Kdown}; */
void keymap_exec(Rune seq) {
  Command *cmd = NULL;
  Window  *win = input;
  print("%x %x ", seq, mod);
  int i = 0;
  /* for (int i = 0; mods[i] && !cmd; ++i) */
  {
  for (int j = 0; j < nelem(keys_mapped); j++) {
	print("%x %x %d", keys_mapped[j].r, j, (runecmp(seq, Kctl)));
	if (mod || runecmp(seq, keys_mapped[j].r)) {
	  print("mod ");
	  cur_map = global_map->val[j];
	  if (mod && Mctl) {
		print("ctl %d", (seq <= 26));
		if (seq <= 26)
		  seq += 96;
	  }
	  break;
	}
  }

  if (cur_map) {
	for (int j = 0; j < cur_map->n; ++j) {
	  print("curmap %d %d %d %d\n", cur_map->n,
			(map_mode == 0 && seq >= '0' && seq <= '9'),
			(runecmp(seq, cur_map->k[j])),
			(cur_map->iscmd));
	  if (map_mode == 0 && seq >= '0' && seq <= '9') {
		prefix = seq - '0';
		break;
	  } else if (runecmp(seq, cur_map->k[j])) {
		map_mode = 1;
		cur_map = cur_map->val[j];
	  }
	  if (cur_map->iscmd) {
		cmd = cur_map->cmd;
		break;
	  }
	}
  } else {
	print("insert ");
	self_insert(win, seq, prefix ? prefix : 1);
	keymap_reset();
  }
  }
  if (cmd != NULL) {
	print("cmd ");
	switch (cmd->argc) {
	case 0:
	  (*cmd->f0)();
	  break;
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

Keymap* keymap_new(int n) {
  Keymap* map;
  map = emalloc(sizeof(Keymap));
  map->n   = 0;
  map->len = n;
  map->k   = emalloc(n * sizeof(int));
  map->val = emalloc(n * sizeof(Keymap));
  return map;
}

void keymap_free(Keymap* map) {
  for(int i = 0; i < map->n; i++)
	keymap_free(map->val[i]);
	free(map->k);
	free(map);
}

void keymap_copy(Keymap* dest, Keymap* map) {
  if(dest->n < map->n) {
	keymap_free(dest);
	dest = keymap_new(map->n + 3);
  }
  for(int i = 0; i < map->n; i++) {
	dest->k[i]   = map->k[i];
	dest->val[i] = map->val[i];
  }
}

void keymap_load(Keydef key_map[]) {
  if(global_map == NULL)
    global_map = keymap_new(nelem(keys_mapped));

  for(int i=0; key_map[i].k; i++) {
	cur_map = NULL;
	print("load %d, ", i); 
	for(int j=0; key_map[i].k[j]; j++) {
	  print("load1 %d, ", j); 
	  char k = key_map[i].k[j];
	  char *k1 = key_map[i].k + j;
	  Rune r;
	  int n = -1;
	  if ((k == '-') || (k == ' '))
		continue;

	  for (int m = 0; m < nelem(keys_mapped); m++) {
		print("%s %x %x ", keys_mapped[m].s, keys_mapped[m].r, j);
		int l = strlen(keys_mapped[m].s);
		if (strncmp(k1, keys_mapped[m].s, l) == 0) {
		  print("matched %x %x ", l, m);
		  r = keys_mapped[m].r;
		  n = m;
		  j += l - 1;
		  break;
		}
	  }
	  if (n > -1) {
		if (cur_map == NULL && global_map->val[n] != NULL) {
		  cur_map = global_map->val[n];
		  print("existing %d", n);
		} else {
		  print("new %d", n);
		  cur_map = keymap_new(3);

		  global_map->n++;
		  global_map->k[n] = r;
		  global_map->val[n] = cur_map;
		}
	  } else
		r = k;
		
	  if (cur_map == NULL)
		cur_map = global_map;
	  keymap_set_key(cur_map, r, key_map[i].cmd);
	}
  }
  cur_map = NULL;
  print("loaded");
  /* exits("loaded"); */
}

static void
wkeyctl(Window *w, Rune r)
{
	keymap_exec(r);
}
