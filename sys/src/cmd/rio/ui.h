#define Kctl 29
#define Kshift 42
#define Kalt 56
#define Kmod4 125

enum {FALSE, TRUE};
typedef int bool;
typedef unsigned int uint;
typedef char Rune;
typedef struct {
  int q0, q1, qh, nr, org;
  char *r;
} Window;

typedef struct {} Frame;
typedef struct {} Point;
typedef struct {} Pt;

void wshow(Window *w, uint n){}
void wsetorigin(Window *w, uint n, int e){}
uint wbacknl(Window *w, uint p, uint n){}
int wbswidth(Window *w, Rune c){}
void wsetselect(Window *w, uint q0, uint q1){}
void wdelete(Window *w, uint q0, uint q1){}
uint winsert(Window *w, Rune *r, int n, uint q0){}
void wcut(Window *w){}
void wsnarf(Window *w){}
void wpaste(Window *w){}

ulong	frcharofpt(Frame*, Point){}
