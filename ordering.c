/* move ordering code */
#include "includes.h"
#include "knightcap.h"

#define NUM_CUTOFFS 1

static unsigned history[NUM_SQUARES][NUM_SQUARES];
static Move refutation[NUM_SQUARES][NUM_SQUARES];

struct {
	Move k1, k2;
} killers[MAX_DEPTH+10];

void order_reset(void)
{
	memset(history, 0, sizeof(history));
	memset(killers, 0, sizeof(killers));
	memset(refutation, 0, sizeof(refutation));
	lprintf(0,"reset ordering info\n");
}

void order_clear(int move_num)
{
	static int last_num;
	int x,y, delta;

	delta = imax(move_num - last_num, 3);
	last_num = move_num;

	memset(killers, 0, sizeof(killers));
	memset(refutation, 0, sizeof(refutation));

	if (delta <= 0) return;

	for (x=A1;x<=H8;x++)
		for (y=A1;y<=H8;y++)
			history[x][y] >>= delta;
}


void cutoff_hint(Position *b, int m, int depth, int ply)
{
	Move *move = &b->moves[m];
	history[move->from][move->to] += (1 << depth);

	refutation[b->last_move.from][b->last_move.to] = (*move);

	if (ply < MAX_DEPTH) {
		if (!same_move(move, &killers[ply].k1) &&
		    !same_move(move, &killers[ply].k2)) {
			if (b->board[move->to]) {
				killers[ply].k1 = (*move);
			} else {
				killers[ply].k2 = (*move);
			}
		}
	}
}


static Move hash_move;

static int order_fn(Position *b, Move *m, int ply, Eval testv)
{
	int ret = 0;

#if RANDOM_ORDERING
	return random() % 1000;
#endif

	if (same_move(m, &hash_move)) {
		return 100000000;
	}

	if (ply < MAX_DEPTH &&
	    (same_move(m, &killers[ply].k1) || 
	     same_move(m, &killers[ply].k2))) {
		ret += 10000;
	}

	if (same_move(m, &b->best_capture)) {
		ret += 15000;
	}

	if (same_move(m, &refutation[b->last_move.from][b->last_move.to])) {
		ret += 10000;
	}

	ret += hash_ordering(b, m, testv) * 10000;

	return ret + history[m->from][m->to];
}

void order_moves(Position *b, Move *moves, int n, int ply, Move *m1,Eval testv)
{
	int m;

	eval_tactics(b);

	if (m1)
		hash_move = (*m1);
	else
		zero_move(&hash_move);

#if 0
	print_board(b->board);
#endif

	for (m=0;m<n;m++)
		moves[m].v = order_fn(b, &moves[m], ply, testv);

	sort_moves(moves, n);
}


