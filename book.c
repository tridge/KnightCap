#include "includes.h"
#include "knightcap.h"

extern int mulling;
extern struct state *state;

#define BOOKLEARN_SKIP 1000
#define BOOKLEARN_BATCH 100
#define BOOKLEARN_FACTOR (0.1)
#define BOOKLEARN_MINDEPTH 2
#define AVERAGE_LENGTH 100

#define MAX_BOOK 12000000

#ifndef SQR
#define SQR(x) ((x)*(x))
#endif

struct MoveP {
	Square from, to;
	uint16 count;
};

#define RESULT_UNKNOWN   0
#define RESULT_WHITE_WIN 1
#define RESULT_BLACK_WIN 2
#define RESULT_DRAW      3

static struct book {
	uint32 hash1;
	uint32 hash2;
	uint16 white_wins;
	uint16 black_wins;
} book[MAX_BOOK];

static int book_size;

/* load the opening book */
void book_open(char *fname)
{
	int fd;
	struct stat st;
	ssize_t ret;

	book_size = 0;

	fd = open(fname, O_RDONLY);
	if (fd == -1) {
		return;
	}

	fstat(fd, &st);

	book_size = st.st_size / sizeof(book[0]);
	if (book_size > MAX_BOOK) {
		book_size = MAX_BOOK;
	}

	ret = read(fd, book, sizeof(book[0]) * book_size);
	close(fd);

	if (ret != sizeof(book[0]) * book_size) {
		lprintf(0, "Failed to read book - got %d\n", (int)ret);
		book_size = 0;
		return;
	}

	lprintf(0,"loaded %d book positions\n", book_size);
}


static int book_find(uint32 hash1, uint32 hash2)
{
	int low,high;

	if (mulling || book_size <= 0) return -1;

	low = 0;
	high = book_size-1;
	while (low < high) {
		int t = (low+high)/2;
		if (book[t].hash1 == hash1) {
			low = t;
			break;
		}
		if (book[t].hash1 < hash1) {
			low = t+1;
		} else {
			high = t-1;
		}
	}

	while (low > 0 && book[low-1].hash1 == hash1) low--;

	while (low < book_size &&
	       book[low+1].hash1 == hash1 && book[low].hash2 != hash2) low++;

	if (low >= book_size || 
	    book[low].hash1 != hash1 || 
	    book[low].hash2 != hash2)
		return -1;

	return low;
}

static int move_comp(Move *m1, Move *m2)
{
	return m1->v - m2->v;
}

static void randomise_moves(Move *moves, int num_moves)
{
	int i;
	for (i=0;i<num_moves;i++) {
		moves[i].v = random();
	}
	qsort(moves, num_moves, sizeof(moves[0]), move_comp);
}

int book_lookup(Position *b0, Move *move, Eval *v)
{
	int i, best, n, best_wins=0, best_losses=0;
	Position b2;
	Move moves[MAX_MOVES];
	int num_moves, not_in_book=0;
	double r1, r2;

	num_moves = generate_moves(b0, moves);

	if (num_moves <= 0) {
		return 0;
	}

	srandom(time(NULL));

	randomise_moves(moves, num_moves);

	if (book_find(b0->hash1, b0->hash2) == -1) {
		not_in_book = 1;
	}

	best = -1;

	for (i=0;i<num_moves;i++) {
		int wins, losses;

		if (!do_move(&b2, b0, &moves[i])) {
			continue;
		}

		n = book_find(b2.hash1, b2.hash2);
		if (n == -1) continue;

		if (whites_move(b0)) {
			wins = book[n].white_wins;
			losses = book[n].black_wins;
		} else {
			wins = book[n].black_wins;
			losses = book[n].white_wins;
		}

#if 1
		lprintf(0, "book maybe %s wins=%d losses=%d r=%.2f\n", 
			short_movestr(b0, &moves[i]),
			wins, losses, wins / (1.0 + losses));
#endif

		if (wins == 0) continue;

		if (best == -1) {
			best = i;
			best_wins = wins;
			best_losses = losses;
			continue;
		}

		r1 = wins / (1.0 + losses);
		r2 = best_wins / (1.0 + best_losses);

#if 1
		if (r1 <= 0.7 * r2) {
			continue;
		}
		if (wins > best_wins * 100) {
			r1 *= 1.5;
		}
		if (wins > best_wins * 10) {
			r1 *= 1.3;
		}
		if (wins > best_wins * 5) {
			r1 *= 1.1;
		}
		if (r1 >= 1.7 * r2 || random_chance((r1/r2) - 0.7)) {
			best = i;
			best_wins = wins;
			best_losses = losses;
		}

// Lightning 1933     25.7    115     89     76    280   1965 (19-Jul-2004)

#else
		if (r1 > r2) {
			best = i;
			best_wins = wins;
			best_losses = losses;		  
		}
#endif
	}

	if (best == -1) {
		return 0;
	}

	r1 = best_wins / (1.0 + best_losses);

	if (not_in_book && r1 < 2.0) {
		return 0;
	}

	move->from = moves[best].from;
	move->to = moves[best].to;
	(*v) = makeeval(b0, 0);

	lprintf(0, "book: %s wins=%d losses=%d\n", 
		short_movestr(b0, move),
		best_wins, best_losses);

	return 1;
}


static int book_comp(struct book *b1, struct book *b2)
{
	if (b1->hash1 != b2->hash1) {
		return b1->hash1 - b2->hash1;
	}
	if (b1->hash2 != b2->hash2) {
		return b1->hash2 - b2->hash2;
	}
	if (b1->white_wins != b2->white_wins) {
		return b1->white_wins - b2->white_wins;
	}
	if (b1->black_wins != b2->black_wins) {
		return b1->black_wins - b2->black_wins;
	}
	return 0;
}

/* build the opening book */
void book_build(char *fname, char *outfname)
{
	char buf[1024], tok[100];
	FILE *f;
	int outfd;
	Position b;
	char *m;
	Move moves[MAX_MOVES];
	int i=0, j;
	int next_game = 0;
	int line=0;
	int result = RESULT_UNKNOWN;

	f = fopen(fname, "r");

	if (!f) return;

	book_size = 0;

	setup_board(&b);

	while (fgets(buf, sizeof(buf)-1, f)) {
		line++;
		if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = 0;
			
		if (buf[0] == '[') {
			if (i > 0 && result != RESULT_UNKNOWN) {
				lprintf(0,"game at line %d book_size=%d\n\n", 
					line, book_size);

				setup_board(&b);
				for (j=0;j<i;j++) {
					do_move(&b, &b, &moves[j]);
					book[book_size].hash1 = b.hash1;
					book[book_size].hash2 = b.hash2;
					switch (result) {
					case RESULT_UNKNOWN:
						book[book_size].white_wins = 0;
						book[book_size].black_wins = 0;
						break;
					case RESULT_WHITE_WIN:
						book[book_size].white_wins = 2;
						book[book_size].black_wins = 0;
						break;
					case RESULT_BLACK_WIN:
						book[book_size].white_wins = 0;
						book[book_size].black_wins = 2;
						break;
					case RESULT_DRAW:
						book[book_size].white_wins = 1;
						book[book_size].black_wins = 1;
						break;
					}
					book_size++;
					if (book_size == MAX_BOOK) {
						printf("Book overflow!!\n");
						goto done;
					}
				}
				setup_board(&b);
				result = RESULT_UNKNOWN;
			}
			lprintf(0,"%s\n", buf);
			next_game = 0;
			i = 0;
			if (strcmp(buf, "[Result \"0-1\"]") == 0) {
				result = RESULT_BLACK_WIN;
			}
			if (strcmp(buf, "[Result \"1-0\"]") == 0) {
				result = RESULT_WHITE_WIN;
			}
			if (strcmp(buf, "[Result \"1/2-1/2\"]") == 0) {
				result = RESULT_DRAW;
			}
			continue;
		}

		if (next_game) continue;

		m = buf;

		while (next_token(&m, tok, " \t\n\r")) {
			if (isdigit(tok[0]) || tok[0] == '$') continue;
			if (i==0) {
			  setup_board(&b);
			}

			if (!parse_move(tok, &b, &moves[i]) || 
			    !legal_move(&b, &moves[i])) {
				lprintf(0,"bad move %d at line %d %s [%s]\n", 
					i, line, tok, buf);
				print_board(b.board);
				next_game = 1;
				setup_board(&b);
				i = 0;
				break;
			} 
			do_move(&b, &b, &moves[i]);
			i++;
		}
	}

	fclose(f);

 done:
	if (book_size == 0) return;

	qsort(book, book_size, sizeof(book[0]), book_comp);

	for (i=0;i<book_size-1;i++) {
		if (book[i].hash1 == book[i+1].hash1 &&
		    book[i].hash2 == book[i+1].hash2) {
			book[i+1].white_wins += book[i].white_wins;
			book[i+1].black_wins += book[i].black_wins;
			book[i].hash1 = book[i].hash2 = 0;
		}
	}

	qsort(book, book_size, sizeof(book[0]), book_comp);

	for (i=0;i<book_size;i++) {
		if (book[i].hash1 != 0 || book[i].hash2 != 0) break;
	}
	if (i > 0) {
		memmove(book, &book[i], (book_size-i)*sizeof(book[0]));
		book_size -= i;
	}

	outfd = open(outfname, O_WRONLY|O_CREAT|O_TRUNC, 0644);

	if (outfd == -1) {
		perror(outfname);
		return;
	}

	write(outfd, book, sizeof(book[0]) * book_size);

	close(outfd);

	lprintf(0,"Wrote book of size %d\n", book_size);
}



#if STORE_LEAF_POS

static int average(int v)
{
	static int history[AVERAGE_LENGTH];
	static int next, total;
	int i, sum;

	history[next] = v;
	if (total < AVERAGE_LENGTH) {
		next++; total++;
	} else {
		next = (next+1) % AVERAGE_LENGTH;
	}

	for (i=sum=0;i<total;i++)
		sum += history[i];
	return (100*sum)/total;
}

static float *eval_gradient(Position *b)
{
	etype v, v2;
	int i, j;
	etype delta = 0.01;
	static float ret[__TOTAL_COEFFS__];
	int n = __TOTAL_COEFFS__;

	b->flags &= ~FLAG_EVAL_DONE;
	b->flags &= ~FLAG_DONE_TACTICS;
	
	v = eval_etype(b, INFINITY, MAX_DEPTH);

	j = 0;
	for (i=0;i<n;i++) {
		coefficients[i] += delta;

		/* material only affects the eval indirectly 
		   via the board, so update the board */
		if (i > IPIECE_VALUES && i < IPIECE_VALUES+KING)
			create_pboard(b);

		b->flags &= ~FLAG_EVAL_DONE;
		b->flags &= ~FLAG_DONE_TACTICS;
		v2 = eval_etype(b, INFINITY, MAX_DEPTH);

		ret[i] = (v2 - v) / delta;
		coefficients[i] -= delta;

		if (i > IPIECE_VALUES && i < IPIECE_VALUES+KING)
			create_pboard(b);
	}
	return ret;
}


static int no_change(int i)
{
	extern int dont_change[];
	int j;
	for (j=0;dont_change[j] != -1;j++)
		if (i == dont_change[j]) return 1;
	return 0;
}


static void change_eval(Position *b1, Move *move, float factor)
{
	struct hash_entry *t;
	Position b;
	Eval e;
	float *grad;
	static float cum_change[__TOTAL_COEFFS__];
	static int count;
	int n = __TOTAL_COEFFS__;
	int i, change_count;
	double total_change=0;

	do_move(&b, b1, move);
	
	t = hash_ptr(b.hash1);
	if (!t || t->hash2 != b.hash2) return;
	
	if (t->depth_high < BOOKLEARN_MINDEPTH) return;

	e = t->low;

	lprintf(0,"low=%d/%e high=%d/%e low2=%d/%e high2=%d/%e\n",
		t->depth_low, EV(t->low),
		t->depth_high, EV(t->high),
		t->depth_low2, EV(t->low2),
		t->depth_high2, EV(t->high2));

	memset(&b, 0, sizeof(b));
	memcpy(&b.board, e.pos.board, sizeof(b.board));
	b.move_num = e.pos.move_num;
	b.flags = e.pos.flags;
	b.enpassent = e.pos.enpassent;
	b.fifty_count = 0;
	b.hash1 = 0;
	b.hash2 = 0;	
	if (!create_pboard(&b)) {
		return;
	}

	if (next_to_play(&b) != next_to_play(b1)) 
		factor = -factor;
	
	grad = eval_gradient(&b);

	change_count = 0;

	for (i=0;i<n;i++) {
		if (no_change(i)) continue;
		
		if (fabs(coefficients[i]) <= 0.0001 ||
		    fabs(grad[i]) <= 0.01)
			continue;

		cum_change[i] += grad[i]/factor; 
	}

	count++;

	if (count == BOOKLEARN_BATCH) {
		count=0;

		total_change = 0;
		for (i=0;i<n;i++) {
			if (fabs(cum_change[i]) < 0.001) {
				continue;
			}
			if (fabs(cum_change[i]) < 1) {
				if (cum_change[i] < 0)
					cum_change[i] = -1;
				else
					cum_change[i] = 1;
			}
			total_change += 1.0/cum_change[i];
			coefficients[i] += 1.0/cum_change[i];
		}

		for (i=0;i<n;i++) {
			cum_change[i] = 0;
		}
		lprintf(0,"total_change=%g\n", total_change);
	}
}


static void learn_one(Position *b, Move *move)
{
	int n = book_find(b);
	int i;
	Move m1;
	Eval v;
	static int book_hits, book_misses;
	int pct=0;

	if (n == -1) {
		lprintf(0,"Not in book %s\n", short_movestr(b, move));
		return;
	}

	book_lookup(b, &m1, &v);

	for (i=0;i<book[n].n;i++) {
		if (same_move(move, &book[n].m[i])) {
			book_hits++;
			pct = average(1);
			break;
		}
	}

	if (i==book[n].n) {
		book_misses++;
		pct = average(0);
		change_eval(b, move, -BOOKLEARN_FACTOR);
		for (i=0;i<book[n].n;i++)
			change_eval(b, &book[n].m[i], BOOKLEARN_FACTOR);
	}

	dump_coeffs("temp.h", 1);

	lprintf(0,"book_hits=%d/%d %d%%\n",
		book_hits, book_hits+book_misses, pct);
}


/* learn via the opening book */
void book_learn(char *fname)
{
	char buf[1024];
	FILE *f;
	Position b;
	char *p;
	Move move;

	f = fopen(fname, "r");

	if (!f) return;

	while (!state->quit && state->computer == 0 && 
	       state->use_mulling) {
		int count=(((unsigned)random()) % BOOKLEARN_SKIP)+1;
		while (count--) {
			while (!fgets(buf, sizeof(buf)-1, f)) {
				rewind(f);
			}
		}

		if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = 0;
		
		p = ppn_to_position(buf, &b);
		if (!p) continue;

		zero_move(&move);

		timer_reset();
		hash_reset();
		order_reset();

		timer_start(next_to_play(&b));
		
		mulling = 1;

		make_move(&b, &move);
		
		mulling = 0;

		learn_one(&b, &move);

		if (state->computer) continue;
		
		timer_off();
	}
	
	fclose(f);
}



#endif
