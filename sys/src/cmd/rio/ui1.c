/**
Support file for handling keybindings.

Copyright (C) 2025  Anand Tamariya

Author: Anand Tamariya <anand@gmail.com>
Keywords: plan9, keybinding

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

Commentary:
 - move char word line screen buffer
 - del  char word line
 - window obj n
 - n is repeat - derive from prefix
*/

/* Code: */
#include <u.h>
#include <libc.h>
#include <ctype.h>
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

void interruptproc(void *v);
void namecomplete(Window *w);

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
struct keymap {
  Rune *k;
  Keymap **val; // Array of Keymap pointer
  int n, len;

  bool iscmd;
  Command *cmd;
};

static Keymap *global_map, *cur_map;
static int prefix, map_mode;

int runecmp(Rune r1, Rune r2) {
  char str[4]; // UTFmax + 1
  if (runetochar(str, &r1) == Runeerror)
	return 0;
  return utfrune(str, r2) != nil;
}

void bol(Window *w) {
  int nb;
  if(w->q0 == 0 || w->q0 == w->qh || w->r[w->q0-1] == '\n')
	return;
  nb = wbswidth(w, 0x15 /* ^U */);
  wsetselect(w, w->q0-nb, shiftdown ? w->q1 : w->q0-nb);
  wshow(w, w->q0);
}

void eol(Window *w) {
  int q1 = w->q1;
  while(q1 < w->nr && w->r[q1] != '\n')
	q1++;
  wsetselect(w, shiftdown ? w->q0 : q1, q1);
  wshow(w, w->q1);
}

void bob(Window *w) {
  if (w->popup && w->popup->i) {
	w = w->popup;
  }
  wsetselect(w, 0, shiftdown ? w->q1 : 0);
  wshow(w, w->q0);
}

void eob(Window *w) {
  if (w->popup && w->popup->i) {
	w = w->popup;
  }
  wsetselect(w, shiftdown ? w->q0 : w->nr, w->nr);
  wshow(w, w->q1);
}

void scrollup(Window *w, int n) {
  int q0, l;
  if (w->popup && w->popup->i) {
	w = w->popup;
  }
  l = (w->Frame.r.max.y - w->Frame.r.min.y) / w->font->height;
  q0 = wbacknl(w, w->org, n * l);
  wsetorigin(w, q0, TRUE);
}  

void scrolldown(Window *w, int n) {
  int q0, l;
  if (w->popup && w->popup->i) {
	w = w->popup;
  }
  l = (w->Frame.r.max.y - w->Frame.r.min.y) / w->font->height;
  q0 = w->org + frcharofpt(w, Pt(w->Frame.r.min.x, w->Frame.r.min.y + n*l*w->font->height));
  if (q0 < w->nr)
	wsetorigin(w, q0, TRUE);
}

void lineup(Window *w, int n) {
  int q0, org, h;

  if (w->popup && w->popup->i) {
	w = w->popup;
	if (w->q0 > w->org)
	  w->q1 = w->q0;
	else
	  w->q1 = w->org;
	shiftdown = 1;
  } else {
	/* If we are scrolling page, go to cursor. */
	wshow(w, w->q0);
  }

  org = w->org;
  q0 = w->q0 - org;
  h = w->font->height;
  Point p = frptofchar(w, q0);
  p.y -= n * h;
  if (p.y < w->Frame.r.min.y) {
	org = wbacknl(w, w->org, n);
	wsetorigin(w, org, TRUE);
	p.y += n * h;
  }
  q0 = w->org + frcharofpt(w, p);
  wsetselect(w, q0, shiftdown ? w->q1 : q0);
  wshow(w, q0);
}

void linedown(Window *w, int n) {
  int q0, org, h;

  if (w->popup && w->popup->i) {
	w = w->popup;
	w->q0 = w->q1;
	shiftdown = 1;
  } else {
	/* If we are scrolling page, go to cursor. */
	wshow(w, w->q0);
  }

  org = w->org;
  q0 = w->q1 - org;
  h = w->font->height;
  Point p = frptofchar(w, q0);
  p.y += n * h;
  if (p.y >= w->Frame.r.max.y) {
	p.y -= n * h;
	org = w->org + frcharofpt(w, Pt(w->Frame.r.min.x, w->Frame.r.min.y + w->font->height));
	wsetorigin(w, org, TRUE);
  }
  q0 = w->org + frcharofpt(w, p);
  wsetselect(w, shiftdown ? w->q0 : q0, q0);
  wshow(w, q0);
}

void charleft(Window *w, int n) {
  /* print("left\n"); */
  int q0 = w->q0;
  if(q0 > 0) {
	q0 = q0 - n;
  }
  /* wsetselect(w, q0, q0); */
  wsetselect(w, q0, shiftdown ? w->q1 : q0);
  wshow(w, q0);
}

void charright(Window *w, int n) {
  /* print("right\n"); */
  int q1 = w->q1;
  if(q1 < w->nr) {
	q1 = q1 + n;
  }
  /* wsetselect(w, q1, q1); */
  wsetselect(w, shiftdown ? w->q0 : q1, q1);
  wshow(w, q1);
}

void cut(Window *w) {
  /* print("cut\n"); */
  wsnarf(w);
  wcut(w);
}

void copy(Window *w) {
  /* print("copy\n"); */
  wsnarf(w);
}

void paste(Window *w) {
  /* print("paste\n"); */
  wpaste(w);
}

/* void exit() { */
/* 	int f; */

/* 	wsupdate(); */
/* 	if(wcur == nil || (f = wwctl(wcur->id, OWRITE)) < 0) */
/* 		return; */
/* 	fprint(f, "delete"); */
/* 	close(f); */
/* } */

void delete_fn(Window *w, Rune r) {
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

void delcharl(Window *w, int i) {
  int q0, q1;
  if (i > 0) {
	q0 = max(w->q0 - i, 0);
	q1 = w->q1;
	wdelete(w, q0, q1);
	wsetselect(w, q0, q0);
	if (w->popup && w->popup->i) {
	  wclosepopup(w);
	  namecomplete(w);
	}
  }
}

void delcharr(Window *w, int i) {
  int q0, q1;
  if (i > 0) {
	q0 = w->q0;
	q1 = min(w->q1 + i, w->nr);
	wdelete(w, q0, q1);
	wsetselect(w, q0, q0);
  }
}

void delwordl(Window *w, int i) {
  while (i-- > 0)
	delete_fn(w, Ketb);
}

void wordleft(Window *w, int i) {
  uint q0, q1;

  q0 = w->q0;
  q1 = w->q1;
  /* print("%d %d, ", q0, q1); */
  while (i-- > 0 && q0 > 0) {
	/* print("i%d %d, ", i, shiftdown); */
	while(q0 > 0 && isspace(w->r[q0 - 1])) {
	  q0--;
	}
	while(q0 > 0 && (isalnum(w->r[q0 - 1]) || (w->r[q0 - 1] == '.') || (w->r[q0 - 1] == '_'))) {
	  q0--;
	}
  }
  /* print("%d %d, ", q0, q1); */
  w->q0 = q0;
  w->q1 = q1;
  wsetselect(w, q0, shiftdown ? w->q1 : q0);
  wshow(w, q1);
}

void wordright(Window *w, int i) {
  uint q0, q1;

  q0 = w->q0;
  q1 = w->q1;
  /* print("%d %d, ", q0, q1); */
  while (i-- > 0) {
	/* print("i%d %d, ", i, shiftdown); */
	while(q1 < w->nr && isspace(w->r[q1])) {
	  q1++;
	}
	while(q1 < w->nr && (isalnum(w->r[q1]) || (w->r[q1] == '.') || (w->r[q1] == '_'))) {
	  q1++;
	}
  }
  /* print("%d %d, ", q0, q1); */
  w->q0 = q0;
  w->q1 = q1;
  wsetselect(w, shiftdown ? w->q0 : q1, q1);
  wshow(w, q1);
}

void delwordr(Window *w, int i) {
  shiftdown = 1;
  wordright(w, i);
  cut(w);
  shiftdown = 0;
}

void interrupt(Window *w) {
  w->qh = w->nr;
  wshow(w, w->qh);
  if(w->notefd < 0)
	return;
  int *notefd = emalloc(sizeof(int));
  *notefd = dup(w->notefd, -1);
  proccreate(interruptproc, notefd, 4096);
}

void selectall(Window *w) {
  wsetselect(w, 0, w->nr);
}

void clear(Window *w) {
  wdelete(w, 0, w->nr);
}

void self_insert(Window *w, Rune s, int n) {
  /* if (s == '\n') print("insert %c %d\n", s, w->qh); */
  uint q0 = w->q0;
  if (s == '\t' && w->popup && w->popup->i) {
	Window *p = w->popup;
	int i1, i2;
	if (p->q0 < p->nr) {
	  i2 = w->q0;
	  wordleft(w, 1);
	  i1 = w->q0;
	  wsetselect(w, i1, i2);
	  cut(w);
	
	  i1 = p->q0;
	  /* eol(p); */
	  i2 = p->q1 - 1;
	  wsetselect(p, i1, i2);
	  cut(p);
	  paste(w);
	  q0 = w->q0 - 1;
	  if (w->r[q0] == '/')
		s = '/';
	}
  } else {
	/* Replace existing selection */
	cut(w);
	q0 = winsert(w, &s, n, q0);
  }
  wshow(w, q0 + 1);
  
  wclosepopup(w);
  if (!isspace(s))
	namecomplete(w);
}

void confirmexit(void);

struct { const char *s; int argc; void (*f); } prim[] = {
  {"bol", 1, bol}, {"eol", 1, eol}, {"bob", 1, bob}, {"eob", 1, eob},
  {"left", 2, charleft}, {"right", 2, charright}, {"up", 2, lineup}, {"down", 2, linedown},  
  {"wordleft", 2, wordleft}, {"wordright", 2, wordright},
  {"cut", 1, cut}, {"copy", 1, copy}, {"paste", 1, paste},
  {"find", 1, wlook}, {"rfind", 1, wrlook}, {"plumb", 1, wplumb},
  {"delcharl", 2, delcharl}, {"delcharr", 2, delcharr},
  {"delwordl", 2, delwordl}, {"delwordr", 2, delwordr},
  {"interrupt", 1, interrupt}, {"autosuggest", 1, namecomplete},
  {"scrollup", 2, scrollup}, {"scrolldown", 2, scrolldown},
  {"selectall", 1, selectall}, {"clear", 2, clear},
  {"exit", 0, confirmexit},
};

/* Special keys mapped for handling. */
struct {char *s; Rune r;} keys_mapped[] = {
  {"C", Kctl}, {"M", Kalt}, {"S", Kshift}, {"mod4", Kmod4},
  {"left", Kleft}, {"right", Kright}, {"up", Kup}, {"down", Kdown},
  {"backspace", Kbs}, {"tab", 0x09}, {"ret", 0x0a}, {"del", Kdel},
  {"spc", ' '}, {"pgup", Kpgup}, {"pgdown", Kpgdown},
  {"home", Khome}, {"end", Kend},
};

int keymap_find(Keymap* cur_map, Rune k) {
  int found = -1;
  for (int j = 0; j < cur_map->n; ++j) {
	if (runecmp(k, cur_map->k[j])) {
	  /* cur_map = cur_map->val[j]; */
	  found = j;
	  break;
	}
  }
  return found;
}

void keymap_set_key(Keymap *map, Rune k, char *cmd) {
  int n = map->n, match = 0;
  for (int i = 0; prim[i].s && !match; ++i) {
	/* print("%d %s\n", i, prim[i].s); */
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

void keymap_exec(Window *win, Rune seq) {
  Command *cmd;
  int k = -1, found = 0, modifier = mod;
  /* print("%x %x ", seq, modifier); */
  if (cur_map == NULL) {
	cur_map = global_map;
	found = 1;
  }
  while (found && !cur_map->iscmd) {
	found = 0;
	for (int j = 0; j < cur_map->n; j++) {
	  /* print("%d %x %x %x,", j, cur_map->k[j], keys_mapped[j].r, seq); */
	  if (modifier) {
		/* Don't handle SHIFT here */
		if (modifier & Mctl) {
		  k = keymap_find(cur_map, Kctl);
		  modifier ^= Mctl;
		  /* Exclude TAB and RET */
		  if (seq <= 26 && seq != 0x09 && seq != 0x0a)
			seq += 96;
		} else if (modifier & Malt) {
		  k = keymap_find(cur_map, Kalt);
		  modifier ^= Malt;
		} else if (modifier & Mmod4) {
		  k = keymap_find(cur_map, Kmod4);
		  modifier ^= Mmod4;
		}
		if (k > -1) {
		  cur_map = cur_map->val[k];
		  found = 1;
		  break;
		}
	  } else if (shiftdown && isalpha(cur_map->k[j])) {
		/* compare after switching case */
		char c = cur_map->k[j];
		if (c == seq
			|| ((c >= 'a' && c <= 'z') && (c - 32 == seq))
			|| ((c >= 'A' && c <= 'Z') && (c + 32 == seq))) {
			  cur_map = cur_map->val[j];
			  found = 1;
			  break;
			}
	  } else if (runecmp(seq, cur_map->k[j])) {
		/* No leading modifier key - typically a command */
		cur_map = cur_map->val[j];
		found = 1;
		break;
	  }
	}
  }

  if (0 && cur_map) {
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
  }
  if (found) {
	cmd = cur_map->cmd;
	if (cmd != NULL) {
	  if (!(strcmp(cmd->s, "up") == 0 || strcmp(cmd->s, "down") == 0
			|| strcmp(cmd->s, "scrollup") == 0 || strcmp(cmd->s, "scrolldown") == 0
			|| strcmp(cmd->s, "delcharl") == 0
			|| strcmp(cmd->s, "bob") == 0 || strcmp(cmd->s, "eob") == 0))
		wclosepopup(win);
	  /* print("cmd %s", cmd->s); */
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
  } else {
	/* print("insert "); */
	self_insert(win, seq, prefix ? prefix : 1);
	keymap_reset();
  }
}

Keymap* keymap_new(int n) {
  Keymap* map;
  map = emalloc(sizeof(Keymap));
  map->n   = 0;
  map->len = n;
  map->iscmd = 0;
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

Keymap* keymap_grow(Keymap* map) {
  Keymap* dest;
  dest = keymap_new(map->n + 3);
  dest->n = map->n;
  for(int i = 0; i < map->n; i++) {
	dest->k[i]   = map->k[i];
	dest->val[i] = map->val[i];
  }
  free(map);
  return dest;
}

void keymap_load(Keydef key_map[]) {
  Keymap* prev_map;
  Rune r;

  if(global_map == NULL)
    global_map = keymap_new(nelem(keys_mapped));

  for(int i=0; key_map[i].k; i++) {
	cur_map = NULL;
	r = 0;
	prev_map = global_map;
	/* print("\n load %d, ", i);  */
	for(int j=0; key_map[i].k[j]; j++) {
	  /* print("load1 %d, ", j);  */
	  char k = key_map[i].k[j];
	  char *k1 = key_map[i].k + j;
	  r = k;
	  int n;
	  if ((k == '-') || (k == ' '))
		continue;

	  for (int m = 0; m < nelem(keys_mapped); m++) {
		/* print("%s %x %x ", keys_mapped[m].s, keys_mapped[m].r, j); */
		int l = strlen(keys_mapped[m].s);
		if (strncmp(k1, keys_mapped[m].s, l) == 0) {
		  /* print("matched %x %x ", l, m); */
		  r = keys_mapped[m].r;
		  j += l - 1;
		  break;
		}
	  }

	  if ((n = keymap_find((cur_map == NULL) ? prev_map : cur_map, r)) == -1) {
		cur_map = keymap_new(1);
		n = prev_map->n++;
		prev_map->k[n] = r;
		prev_map->val[n] = cur_map;
		/* } else if (cur_map->iscmd) { */
		/* 	print("Key %x mapped to a command", r); */
		/* 	exits("Mapping conflict"); */
	  } else {
		cur_map = prev_map->val[n];
		if (cur_map->n >= cur_map->len) {
		  /* print("grow %d >= %d ", cur_map->n, cur_map->len); */
		  cur_map = keymap_grow(cur_map);
		  prev_map->val[n] = cur_map;
		}
	  }
	  prev_map = cur_map;
	}
	/* print("Map %x to %s\n", r, key_map[i].cmd); */
	keymap_set_key(cur_map, r, key_map[i].cmd);
  }
  cur_map = NULL;
  print("loaded");
  /* exits("loaded"); */
}
 
void wkeyctl(Window *w, Rune r) {
  if(!w->mouseopen) keymap_exec(w, r);
}
