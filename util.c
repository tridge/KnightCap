#include "includes.h"
#include "knightcap.h"

extern struct state *state;

void beep(int n)
{
	while (n--) {
		putc(7,stdout);
		fflush(stdout);
		sleep(1);
	}
}

static inline void simple_merge(Move *m1,Move *m2,Move *d,int n1,int n2)
{
	while (n1 > 0 && n2 > 0) {
		if (m1->v >= m2->v) {
			*d++ = *m1++;
			--n1;
		} else {
			*d++ = *m2++;
			--n2;
		}
	}
	while (n1--) {
		*d++ = *m1++;
	}
	while (n2--) {
		*d++ = *m2++;
	}
}

void sort_moves(Move *m, int n)
{
	static Move tmp[MAX_MOVES];
	Move *p1, *p2, *p3;
	int l, i;

	p1 = m;
	p2 = tmp;

	for (l=1; l<n; l*=2) {
		for (i=0; i<=(n-2*l); i += 2*l) {
			simple_merge(p1+i,p1+(i+l),p2+i, l, l);
		}
		if (i < n) {
			simple_merge(p1+i,p1+(i+l),p2+i, 
				     i+l>=n?n-i:l,
				     i+l>=n?0:(n-i)-l);
		}
		p3 = p1;
		p1 = p2;
		p2 = p3;
	}

	if (p2 == m) {
		for (i=0;i<n;i++) {
			m[i] = tmp[i];
		}
	} 
}


char *posstr(Square s)
{
	static char ret[4][3];
	static int i;
	char *p;

	p = ret[i];
	i = (i+1)%4;

	sprintf(p, "%c%c", XPOS(s) + 'a', YPOS(s) + '1');
	return p;
}

char *piecestr(Piece p)
{
	static char *names[7] = {"NONE", "PAWN", "KNIGHT", "BISHOP", 
				 "ROOK", "QUEEN", "KING"};
	return names[abs(p)];
}


char piecechar(Piece p)
{
	static char names[2*KING+1] = "kqrbnp PNBRQK";
	return names[p + KING];
}


char *short_movestr(Position *p, Move *move)
{
	static char reti[4][10];
	static int nexti;
	char *ret = reti[nexti];

	nexti = (nexti+1) % 4;

	sprintf(ret, "%s%s", posstr(move->from), posstr(move->to));

	if (p && abs(p->board[move->from]) == PAWN &&
	    (YPOS(move->to) == 0 || YPOS(move->to) == 7)) {
		/* assume a queen */
		sprintf(ret+strlen(ret),"=q");
	}

	return ret;
}

char *movestr(Position *p, Move *move)
{
	static char ret[80];

	ret[0] = 0;

	if (next_to_play(&state->position) == state->computer) {
		sprintf(ret, "%d. ... %s", 
			1+(p->move_num/2), 
			short_movestr(p, move));
	} 
#if 0
	else {
		sprintf(ret, "%d. %s", 
			1+(p->move_num/2), 
			short_movestr(p, move));
	}
#endif
	return ret;
}

void print_move(Position *p, Move *move)
{
	printf("%s\n", movestr(p, move));
}

char *colorstr(Piece p)
{
	static char *b = "B";
	static char *w = "W";
	if (p < 0)
		return b;
	return w;
}

int is_sliding(Piece p)
{
	static const int sliding[KING+1] = {0, 0, 0, 1, 1, 1, 0};
	return sliding[abs(p)];
}

int charpiece(char c)
{
	switch (tolower(c)) {
	case 'p': return PAWN;
	case 'n': return KNIGHT;
	case 'r': return ROOK;
	case 'b': return BISHOP;
	case 'q': return QUEEN;
	case 'k': return KING;
	}
	return 0;
}

void *Realloc(void *p, int size)
{
	void *ret;

	if (p) {
		ret = (void *)realloc(p, size);
	} else {
		ret = (void *)malloc(size);
	}

	if (!ret) {
		lprintf(0, "Realloc %d failed\n", size);
		return NULL;
	}
	
	return ret;	
}


void ring_bell(void)
{
	putchar(7);
	fflush(stdout);
}



/* this assumes a color_xterm */
void print_board(Piece *board)
{
	extern int color_display;
	char *yellow = "[35m"; 
	char *blue = "[34m"; 
	char *normal = "[m";
	char piece[KING+1] = " pNBRQK";
	Piece p;
	int x,y;

	printf("---------------------------------\n");

	for (y=7;y>=0;y--) {
		for (x=0;x<8;x++) {
			p = board[POSN(x,y)];
			if (color_display) {
				printf("| %s%c%s ", 
				       p > 0 ? yellow : blue,
				       piece[abs(p)],
				       normal);
			} else {
				if (p >= 0) 
					printf("| %c ", piece[abs(p)]);
				else
					printf("|*%c ", piece[abs(p)]);
			}
		}
		if (y != 0) {
			printf("|\n|---+---+---+---+---+---+---+---|\n");
		}
	}

	printf("|\n---------------------------------\n");
}

void print_bitboard(uint64 bitboard)
{
	int x,y;
	uint64 mask;

	printf("---------------------------------\n");
		
	for (y=7;y>=0;y--) {
		for (x=0;x<8;x++) {
			mask =  ((uint64)1<<POSN(x,y));
			printf("| %c ", (bitboard & mask)?'*':' ');
		}
		if (y != 0)
			printf("|\n|---+---+---+---+---+---+---+---|\n");
	}
	
	printf("|\n---------------------------------\n");
}

int process_exists(int pid)
{
	return (pid == getpid() || kill(pid,0) == 0 || errno != ESRCH);
}


/* an alternative to strtok. This one can be used recursively */
int next_token(char **ptr,char *buff,char *sep)
{
	static char *last_ptr;
	char *s;
	int quoted;

	if (!ptr) ptr = &last_ptr;
	if (!ptr) return 0;

	s = *ptr;

	/* default to simple separators */
	if (!sep) sep = " \t\n\r";
	
	/* find the first non sep char */
	while(*s && strchr(sep,*s)) s++;
	
	/* nothing left? */
	if (! *s) return 0;
	
	/* copy over the token */
	for (quoted = 0; *s && (quoted || !strchr(sep,*s)); s++)
		{
			if (*s == '\"') 
				quoted = !quoted;
			else
				*buff++ = *s;
		}
	
	*ptr = (*s) ? s+1 : s;  
	*buff = 0;
	last_ptr = *ptr;
	
	return 1;
}

void null_func(Position *b)
{
	
}


char *position_to_ppn(Position *b)
{
	int x, y, count;
	char *p;
	static char ppn[200];

	p = ppn;

	memset(ppn, 0, sizeof(ppn));

	for (y=7;y>=0;y--) {
		count = 0;

		for (x=0;x<8;x++) {
			if (b->board[POSN(x,y)] == 0) {
				count++;
				continue;
			}
			if (count) {
				*p++ = '0' + count;
				count = 0;
			}
			*p++ = piecechar(b->board[POSN(x,y)]);
		}
		if (count)
			*p++ = '0' + count;
		if (y > 0)
			*p++ = '/';
	}

	*p++ = ' ';

	if (whites_move(b))
		*p++ = 'w';
	else
		*p++ = 'b';

	*p++ = ' ';

	if (!(b->flags & FLAG_CAN_CASTLE)) {
		*p++ = '-';
	} else {
		if (b->flags & WHITE_CASTLE_SHORT)
			*p++ = 'K';
		if (b->flags & WHITE_CASTLE_LONG)
			*p++ = 'Q';
		if (b->flags & BLACK_CASTLE_SHORT)
			*p++ = 'k';
		if (b->flags & BLACK_CASTLE_LONG)
			*p++ = 'q';
	}

	*p++ = ' ';
	
	if (b->enpassent) {
		strcat(p, posstr(b->enpassent));
	} else {
		*p++ = '-';
	}

	return ppn;
}



/* convert a ppn to a position, returning a pointer to the char after
   the ppn ends (which often has "best move" etc in it) */
char *ppn_to_position(char *ppn, Position *b)
{
	int x, y;
	char *p = ppn;

	memset(b, 0, sizeof(*b));

	for (y=7;y>=0;y--) {
		for (x=0;x<8;) {
			if (isdigit(*p)) {
				x += (*p) - '0';
				p++;
				continue;
			}

			b->board[POSN(x,y)] = charpiece(*p);
			if (islower(*p)) 
				b->board[POSN(x,y)] = -b->board[POSN(x,y)];
			p++;
			x++;
		}
		p++;
	}

	b->move_num = 30; /* arbitrary */
	if ((*p) == 'b')
		b->move_num++;

	p += 2;

	b->flags = 0;
	if ((*p) == '-') {
		p++;
	} else {
		while ((*p) && (*p) != ' ') {
			if ((*p) == 'k')
				b->flags |= BLACK_CASTLE_SHORT;
			if ((*p) == 'q')
				b->flags |= BLACK_CASTLE_LONG;
			if ((*p) == 'K')
				b->flags |= WHITE_CASTLE_SHORT;
			if ((*p) == 'Q')
				b->flags |= WHITE_CASTLE_LONG;
			p++;
		}
	}

	p++;

	if ((*p) == '-') {
		p++;
		b->enpassent = 0;
	} else {
		x = p[0] - 'a';
		y = p[1] - '1';
		b->enpassent = POSN(x,y);
		p += 2;
	}

	p++;

	create_pboard(b);

	return p;
}

uint16 swapu16(uint16 x)
{
	char *s = (char *)&x;
	char t;
	t = s[0];
	s[0] = s[1];
	s[1] = t;
	return x;
}

int16 swap16(int16 x)
{
	char *s = (char *)&x;
	char t;
	t = s[0];
	s[0] = s[1];
	s[1] = t;
	return x;
}

uint32 swapu32(uint32 x)
{
	char *s = (char *)&x;
	char t;
	t = s[0];
	s[0] = s[3];
	s[3] = t;
	t = s[1];
	s[1] = s[2];
	s[2] = t;
	return x;
}

int swap32(int x)
{
	char *s = (char *)&x;
	char t;
	t = s[0];
	s[0] = s[3];
	s[3] = t;
	t = s[1];
	s[1] = s[2];
	s[2] = t;
	return x;
}

int Write(int fd, char *buf, size_t count)
{
	int r,total;

	total = 0;

	while (count) {
		r = write(fd, buf, count);
		if (r == -1) {
			if (errno == EINTR) continue;
			if (total) return total;
			return -1;
		}
		total += r;
		count -= r;
		buf += r;
	}

	return total;
}

int Read(int fd, char *buf, size_t count)
{
	int r,total;

	total = 0;

	while (count) {
		r = read(fd, buf, count);
		if (r == -1) {
			if (errno == EINTR) continue;
			if (total) return total;
			return -1;
		}
		total += r;
		count -= r;
		buf += r;
	}

	return total;
}

int result()
{
	if (!state->ics_robot) {
		return -1;
	}

	if (state->won == UNKNOWN) {
		/* haven't heard from ICS who won so determine it from
		   board and colour. Need to fix this */
		if (state->position.winner == STALEMATE) {
			state->won = STALEMATE;
		} else if (state->position.winner == state->colour) {
			state->won = 1;
		} else if (state->position.winner == -state->colour) {
			state->won = 0;
		} else {
			state->won = TIME_FORFEIT;
		}
	}

	return (state->won);
}

void p_coeff_vboard(etype *p)
{
	int x, y;

	for (y=0; y<8; y++) {
		for (x=0; x<8; x++) {
			lprintf(0,"%9.3f", *(p + x + y*8));
		}
		printf("\n");
	}
}

char *evalstr(etype v)
{
	static char ret[50];
	if (v >= INFINITY-1) {
		sprintf(ret, "Inf");
	} else 	if (v <= -INFINITY+1) {
		sprintf(ret, "-Inf");
	} else 	if (v > FORCED_WIN) {
		sprintf(ret,"Mate%02d", (int)((WIN-v)/2 + 1));
	} else if (v < -FORCED_WIN) {
		sprintf(ret,"-Mate%02d", (int)((v + WIN)/2 + 1));
	} else {
		sprintf(ret,"%3.2f", ((double)v)/PAWN_VALUE);
	}
	return ret;
}


void init_eval_tables(void)
{
        static int initialised;
        int i;
	extern int pop_count[256];

        if (initialised) return;
        initialised = 1;

        state->rating_change = -2;

        for (i=0;i<256;i++) {
                pop_count[i] = bit_count(i);
        }
}
 
#ifdef __INSURE__
#include <dlfcn.h>

/*******************************************************************
This routine is a trick to immediately catch errors when debugging
with insure. A xterm with a gdb is popped up when insure catches
a error. It is Linux specific.
********************************************************************/
int _Insure_trap_error(int a1, int a2, int a3, int a4, int a5, int a6)
{
	static int (*fn)();
	int ret;
	char cmd[1024];

	sprintf(cmd, "/usr/X11R6/bin/xterm -display :0 -T Panic -n Panic -e /bin/sh -c 'cat /tmp/ierrs.*.%d ; gdb /proc/%d/exe %d'", 
		getpid(), getpid(), getpid());

	if (!fn) {
		static void *h;
		h = dlopen("/usr/local/parasoft/insure++lite/lib.linux2/libinsure.so", RTLD_LAZY);
		fn = dlsym(h, "_Insure_trap_error");
	}

	ret = fn(a1, a2, a3, a4, a5, a6);

	system(cmd);

	return ret;
}
#endif


int random_chance(double prob)
{
	const unsigned maxx = 1000000;
	uint32 x = random() % maxx;
	if (x < prob * maxx) {
		return 1;
	}
	return 0;
}
