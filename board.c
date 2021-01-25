/* this file contains all the code that keeps the board position
   uptodate, calling eval and hash procedures to ensure that
   the hashes and evaluation are also kept uptodate */

#include "includes.h"
#include "knightcap.h"

extern struct state *state;

struct hashvalue_entry hash_values;

static inline void init_hash_values(void)
{
	static int initialised;
	int i, j;
	int oldrand = random();

	if (initialised) return;
	initialised = 1;

	memset(&hash_values, 0, sizeof(hash_values));

	srandom(1);

	for (i=0;i<(2*KING+1);i++) {
		if (i != KING) {
			for (j=0;j<NUM_SQUARES;j++) {
				hash_values.v1[i][j] = random() & ~1;
				hash_values.v2[i][j] = random();
			}
		}
	}

	srandom(oldrand);
}

static void eval_piece(Position *b, PieceStruct *p)
{
	if (p->p > 0) 
		b->w_material += mat_value(p->p);
	else
		b->b_material += mat_value(-p->p);
}

static void deval_piece(Position *b, PieceStruct *p)
{
	if (p->p > 0) 
		b->w_material -= mat_value(p->p);
	else
		b->b_material -= mat_value(-p->p);		
}


static inline void remove_piece(Position *b, PieceStruct *piece)
{
	int pi = (piece - b->pieces);
	uint32 mask = ~(1<<pi);

	deval_piece(b, piece);
	remove_hash(b, piece->pos, piece->p);
	set_pboard(b, piece->pos, NULL);
	b->board[piece->pos] = 0;
	piece->p = 0;
	b->pawns7th &= mask;
	b->piece_mask &= mask;
	b->material_mask &= mask;
}


static inline void move_piece(Position *b, PieceStruct *piece, Square to)
{
	int pi = (piece - b->pieces);
	if (b->pawns7th & (1<<pi))
		b->pawns7th &= ~(1<<pi);
	remove_hash(b, piece->pos, piece->p);
	set_pboard(b, piece->pos, NULL);
	set_pboard(b, to, piece);
	b->board[piece->pos] = 0;
	b->board[to] = piece->p;
	piece->pos = to;
	add_hash(b, piece->pos, piece->p);
	if ((piece->p == PAWN && YPOS(to) == 6) ||
	    (piece->p == -PAWN && YPOS(to) == 1)) {
		b->pawns7th |= (1<<pi);
	}
}

static inline void add_piece(Position *b, Piece p, Square to, 
			     PieceStruct *piece)
{
	int pi = (piece - b->pieces);
	b->board[to] = p;
	piece->pos = to;
	piece->p = p;

	set_pboard(b, to, piece);
	add_hash(b, piece->pos, piece->p);
	eval_piece(b, piece);

	if (is_sliding(p))
		b->sliding_mask |= (1<<pi);

	if ((p == PAWN && YPOS(to) == 6) || (p == -PAWN && YPOS(to) == 1)) {
		b->pawns7th |= (1<<pi);
	}

	if (abs(p) != PAWN)
		b->piece_mask |= (1<<pi);
	b->material_mask |= (1<<pi);
}


static inline void change_piece(Position *b, PieceStruct *piece, Piece to)
{
	int pi = (piece - b->pieces);
	Square pos = piece->pos;

	if (to == QUEEN && !(b->piece_mask & WQUEEN_MASK)) {
		remove_piece(b, piece);
		add_piece(b, QUEEN, pos, &(WHITEPIECES(b)[IQUEEN]));
	} else if (to == -QUEEN && !(b->piece_mask & BQUEEN_MASK)) {
		remove_piece(b, piece);
		add_piece(b, -QUEEN, pos, &(BLACKPIECES(b)[IQUEEN]));
	} else {
		deval_piece(b, piece);
		remove_hash(b, piece->pos, piece->p);
		b->board[piece->pos] = to;
		piece->p = to;
		add_hash(b, piece->pos, piece->p);
		eval_piece(b, piece);
		if (is_sliding(to)) {
			b->sliding_mask |= (1<<pi);
		}
		if (abs(to) != PAWN)
			b->piece_mask |= (1<<pi);
	}
}


int check_repitition(Position *b, int repeats)
{
	int i;

	/* check for exceeding the 50 move rule */
	if (b->fifty_count > REPITITION_LENGTH) return 1;

	/* and evaluate as a draw if the position has occurred before with
	   the same player to play */
	for (i=b->move_num-2;
	     repeats && i>=(b->move_num - b->fifty_count);
	     i-=2) {
		if (state->hash_list[i] == b->hash1)
			repeats--;
	}

	return repeats == 0;
}


static inline void copy_position1(Position *d, Position *s)
{
	int n = (((char *)&d->part1_marker) - ((char *)d));
	memcpy((char *)d, (char *)s, n);
}

static inline void copy_position2(Position *d, Position *s)
{
	int n = (((char *)&d->dont_copy) - ((char *)&d->part1_marker));
	memcpy((char *)&d->part1_marker, (char *)&s->part1_marker, n);
}

/* execute the move on the board and update the flags and all the
   state information
 */
__fastfunc(int do_move_part1(Position *b, Position *oldb, Move *move))
{
	Piece p1;
	PieceStruct *piece, *oldpiece;
	int complex_move = 0;
	static Position b1;

	init_hash_values();

	if (oldb == b) {
		b1 = (*b);
		oldb = &b1;
	} else {
		copy_position1(b, oldb);
	}

	p1 = b->board[move->from];

	piece = get_pboard(b, move->from);

	if (!piece || !p1) {
		lprintf(0,"no piece! %s pid=%d\n", 
			short_movestr(b, move), getpid());
		print_board(b->board);
		save_game("bug.game");
		return 0;
	}

#if 0
	if ((b->board[move->from] > 0 && b->board[move->to] > 0) ||
	    (b->board[move->from] < 0 && b->board[move->to] < 0)) {
		lprintf(0,"own capture! %s\n", short_movestr(b, move));
		print_board(b->board);
		save_game("bug.game");
		return 0;
	}

	if (abs(b->board[move->to]) == KING) {
		lprintf(0,"king capture! %s\n", short_movestr(b, move));
		print_board(b->board);
		save_game("bug.game");
		return 0;
	}
#endif

	oldpiece = get_pboard(b, move->to);

	b->last_move = (*move);

	zero_move(&b->best_capture);

	b->flags = oldb->flags & ~(FLAG_CHECK|FLAG_PREV_CHECK|FLAG_EVAL_DONE|FLAG_EXTENDED|FLAG_DONE_TACTICS);
	if (oldb->flags & FLAG_CHECK) {
		b->flags |= FLAG_PREV_CHECK;
	}

	if (oldpiece) {
		remove_piece(b, oldpiece);
	}

	move_piece(b, piece, move->to);

	/* that was the easy bit - now all the messy bits */

	/* did they take enpassent? */
	if (oldb->enpassent && move->to == oldb->enpassent && abs(p1) == PAWN) {
		if (p1 == PAWN) {
			oldpiece = get_pboard(b, move->to + SOUTH);
		} else {
			oldpiece = get_pboard(b, move->to + NORTH);
		}
		remove_piece(b, oldpiece);
		complex_move = 1;
	}

	/* did they promote? */
	if (oldb->pawns7th) {
		if (p1 == PAWN && YPOS(piece->pos) == 7) {
			if (oldb->promotion) 
				change_piece(b, piece, oldb->promotion);
			else {
				change_piece(b, piece, QUEEN);
				b->flags |= FLAG_WHITE_PROMOTE;
			}
			complex_move = 1;
		} else if (p1 == -PAWN && YPOS(piece->pos) == 0) {
			if (oldb->promotion)
				change_piece(b, piece, -oldb->promotion);
			else {
				change_piece(b, piece, -QUEEN);
				b->flags |= FLAG_BLACK_PROMOTE;
			}
			complex_move = 1;
		}
	}

	b->promotion = 0;

	b->hash1 ^= (oldb->enpassent<<8);

	/* can the next player move enpassent? */
	if (p1 == PAWN && (move->to == move->from + 2*NORTH) &&
	    (oldb->topieces[move->from+NORTH] & BPAWN_MASK)) {
		b->enpassent = move->from + NORTH;
	} else if (p1 == -PAWN && (move->to == move->from + 2*SOUTH) &&
		   (oldb->topieces[move->from+SOUTH] & WPAWN_MASK)) {
		b->enpassent = move->from + SOUTH;
	} else {
		b->enpassent = 0;
	}

	b->hash1 ^= (b->enpassent<<8);


	if (!(b->flags & FLAG_CAN_CASTLE))
		goto cant_castle;

	b->hash1 ^= (b->flags & (FLAG_CAN_CASTLE)) << 16;

	/* have they castled? */
	if (p1 == KING || p1 == -KING) {
		if (move->to == move->from + 2*EAST) {
			move_piece(b, get_pboard(b, move->from + 3*EAST),
				   move->from + EAST);
			complex_move = 1;
		} else if (move->to == move->from + 2*WEST) {
			move_piece(b, get_pboard(b, move->from + 4*WEST),
				   move->from + WEST);
			complex_move = 1;
		}
	}

	/* have they wrecked their castling chances? There are lots of
           ways to do it! */
	if (p1 == KING && 
	    (b->flags & (WHITE_CASTLE_LONG | WHITE_CASTLE_SHORT))) {
		if (move->from == E1 && (move->to == G1 || move->to == C1))
			b->flags |= WHITE_CASTLED;
		b->flags &= ~(WHITE_CASTLE_LONG | WHITE_CASTLE_SHORT);
	} else if (p1 == -KING &&
		   (b->flags & (BLACK_CASTLE_LONG | BLACK_CASTLE_SHORT))) {
		if (move->from == E8 && (move->to == G8 || move->to == C8))
			b->flags |= BLACK_CASTLED;
		b->flags &= ~(BLACK_CASTLE_LONG | BLACK_CASTLE_SHORT);
	}

	if (p1 == ROOK) {
		if (move->from == A1 && (b->flags & WHITE_CASTLE_LONG)) {
			b->flags &= ~WHITE_CASTLE_LONG;
		} else if (move->from == H1 && 
			   (b->flags & WHITE_CASTLE_SHORT)) {
			b->flags &= ~WHITE_CASTLE_SHORT;
		}
	} else if (p1 == -ROOK) {
		if (move->from == A8 && (b->flags & BLACK_CASTLE_LONG)) {
			b->flags &= ~BLACK_CASTLE_LONG;
		} else if (move->from == H8 && 
			   (b->flags & BLACK_CASTLE_SHORT)) {
			b->flags &= ~BLACK_CASTLE_SHORT;
		}
	}

	/* this covers rook captures which wreck castling chances */
	if (oldpiece) {
		if (move->to == A1 && (b->flags & WHITE_CASTLE_LONG)) {
			b->flags &= ~WHITE_CASTLE_LONG;
		} else if (move->to == H1 && (b->flags & WHITE_CASTLE_SHORT)) {
			b->flags &= ~WHITE_CASTLE_SHORT;
		} else if (move->to == A8 && (b->flags & BLACK_CASTLE_LONG)) {
			b->flags &= ~BLACK_CASTLE_LONG;
		} else if (move->to == H8 && (b->flags & BLACK_CASTLE_SHORT)) {
			b->flags &= ~BLACK_CASTLE_SHORT;
		}
	}

	b->hash1 ^= (b->flags & (FLAG_CAN_CASTLE)) << 16;

	if ((b->flags & FLAG_CAN_CASTLE) != (oldb->flags & FLAG_CAN_CASTLE))
		complex_move = 1;

 cant_castle:

	/* update the fifty move counter */
	if (oldpiece || abs(p1) == PAWN)
		b->fifty_count = 1;
	else
		b->fifty_count = oldb->fifty_count+1;

	state->hash_list[oldb->move_num] = oldb->hash1;
	state->hash_list[oldb->move_num+1] = b->hash1;

	b->move_num = oldb->move_num+1;
	b->winner = oldb->winner;
	b->num_moves = 0;
	b->moves = NULL;
	zero_move(&b->best_capture);

	if (oldb != &b1) {
		b->oldb = oldb;
	} else {
		b->oldb = NULL;
		complex_move = 1;
	}

	/* we use the bottom bit of hash1 to indicate who is to move 
	   in hash entries */
	b->hash1 ^= 1;

	if (!complex_move) {
		b->flags |= FLAG_NEED_PART2;
		return 1;
	}

	regen_moves(b);
	b->flags &= ~FLAG_NEED_PART2;

	if (p1 > 0) {
		int bkpos = BLACKPIECES(b)[IKING].pos;
		int wkpos = WHITEPIECES(b)[IKING].pos;
		if (b->topieces[wkpos] & BLACK_MASK)
			return 0;
		if (b->topieces[bkpos] & WHITE_MASK)
			b->flags |= FLAG_CHECK;
	} else {
		int bkpos = BLACKPIECES(b)[IKING].pos;
		int wkpos = WHITEPIECES(b)[IKING].pos;
		if (b->topieces[bkpos] & WHITE_MASK)
			return 0;
		if (b->topieces[wkpos] & BLACK_MASK)
			b->flags |= FLAG_CHECK;
	}

	return 1;
}


/* execute the move on the board and update the flags and all the
   state information
 */
__fastfunc(int do_move_part2(Position *b))
{
	if (!(b->flags & FLAG_NEED_PART2)) return 1;

	b->flags &= ~FLAG_NEED_PART2;

	if (b->oldb)
	  copy_position2(b, b->oldb);

	update_moves(b, b->oldb, &b->last_move);

	if (blacks_move(b)) {
		int bkpos = BLACKPIECES(b)[IKING].pos;
		int wkpos = WHITEPIECES(b)[IKING].pos;
		if (b->topieces[wkpos] & BLACK_MASK)
			return 0;
		if (b->topieces[bkpos] & WHITE_MASK)
			b->flags |= FLAG_CHECK;
	} else {
		int bkpos = BLACKPIECES(b)[IKING].pos;
		int wkpos = WHITEPIECES(b)[IKING].pos;
		if (b->topieces[bkpos] & WHITE_MASK)
			return 0;
		if (b->topieces[wkpos] & BLACK_MASK)
			b->flags |= FLAG_CHECK;
	}

	return 1;
}

int do_move(Position *b, Position *oldb, Move *move)
{
	if (!do_move_part1(b, oldb, move)) return 0;

	return do_move_part2(b);
}


/* setup the usual initial position */
void setup_board(Position *b)
{
	timer_reset();

	memset(b, 0, sizeof(*b));

	/* all castling options are enabled */
	b->flags = WHITE_CASTLE_LONG | WHITE_CASTLE_SHORT | 
		BLACK_CASTLE_LONG | BLACK_CASTLE_SHORT; 
	
	b->board[A1] = b->board[H1] = ROOK;
	b->board[B1] = b->board[G1] = KNIGHT;
	b->board[C1] = b->board[F1] = BISHOP;
	b->board[D1] = QUEEN;
	b->board[E1] = KING;
	b->board[A2] = b->board[B2] = b->board[C2] = b->board[D2] = PAWN;
	b->board[E2] = b->board[F2] = b->board[G2] = b->board[H2] = PAWN;

	b->board[A8] = b->board[H8] = -ROOK;
	b->board[B8] = b->board[G8] = -KNIGHT;
	b->board[C8] = b->board[F8] = -BISHOP;
	b->board[D8] = -QUEEN;
	b->board[E8] = -KING;
	b->board[A7] = b->board[B7] = b->board[C7] = b->board[D7] = -PAWN;
	b->board[E7] = b->board[F7] = b->board[G7] = b->board[H7] = -PAWN;

	create_pboard(b);
}


static int piece_comp(PieceStruct *p1, PieceStruct *p2)
{
	int ret = abs(p2->p) - abs(p1->p);
	if (ret == 0)
		return p2->pos - p1->pos;
	return ret;
}


static void shiftup(PieceStruct *p, int loc, int *n)
{
	memmove(&p[loc+1], &p[loc], ((*n)-loc)*sizeof(*p));
	p[loc].p = 0;
	(*n)++;
}


/* this assumes the board is correct and tries to form
   a pboard and piece list. Its done in this ugly way so that
   KnightCap can setup any position using the same code, and
   get the pieces in a sensible order for its bitmasks */
int create_pboard(Position *b)
{
	int i;
	PieceStruct *pw = WHITEPIECES(b);
	PieceStruct *pb = BLACKPIECES(b);
	PieceStruct t;
	int w_pieces=0;
	int b_pieces=0;
	int w_pawns=0;
	int b_pawns=0;

	b->winner = 0;

	init_hash_values();

	memset(b->pboard, 0, sizeof(b->pboard));
	memset(b->pieces, 0, sizeof(b->pieces));
	b->sliding_mask = 0;
	b->pawns7th = 0;
	b->hash1 = b->move_num & 1;
	b->hash2 = 0;
	b->piece_mask = 0;
	b->material_mask = 0;
	b->w_material = 0;
	b->b_material = 0;
	b->promotion = 0;
#if 1
	b->wpassed_pawn_mask = 0;
	b->bpassed_pawn_mask = 0;
#endif
	for (i=A1;i<=H8;i++) {
		Piece p = b->board[i];
		if (p == PAWN) {
			w_pawns++;
		} else if (p == -PAWN) {
			b_pawns++;
		} else if (p > PAWN) {
			pw->pos = i;
			pw->p = p;
			pw++;
			w_pieces++;
		} else if (p < -PAWN) {
			pb->pos = i;
			pb->p = p;
			pb++;
			b_pieces++;
		}
	}

	pw = &WHITEPIECES(b)[16 - w_pawns];
	pb = &BLACKPIECES(b)[16 - b_pawns];

	for (i=A1;i<=H8;i++) {
		Piece p = b->board[i];
		if (p == PAWN) {
			pw->pos = i;
			pw->p = p;
			pw++;
		} else if (p == -PAWN) {
			pb->pos = i;
			pb->p = p;
			pb++;
		}
	}

	pw = WHITEPIECES(b);
	pb = BLACKPIECES(b);

	qsort(pw, w_pieces, sizeof(*pw), piece_comp);
	qsort(pb, b_pieces, sizeof(*pb), piece_comp);
		
	if (pw[0].p != KING || pb[0].p != -KING) {
		lprintf(0,"no king!\n");
		return 0;
	}

	if (pw[IQUEEN].p != QUEEN) shiftup(pw, IQUEEN, &w_pieces);
	if (pw[IKROOK].p != ROOK) shiftup(pw, IKROOK, &w_pieces);
	if (pw[IQROOK].p != ROOK) shiftup(pw, IQROOK, &w_pieces);
	if (pw[IKBISHOP].p != BISHOP) shiftup(pw, IKBISHOP, &w_pieces);
	if (pw[IQBISHOP].p != BISHOP) shiftup(pw, IQBISHOP, &w_pieces);
	if (pw[IKKNIGHT].p != KNIGHT) shiftup(pw, IKKNIGHT, &w_pieces);
	if (pw[IQKNIGHT].p != KNIGHT) shiftup(pw, IQKNIGHT, &w_pieces);

	if (pb[IQUEEN].p != -QUEEN) shiftup(pb, IQUEEN, &b_pieces);
	if (pb[IKROOK].p != -ROOK) shiftup(pb, IKROOK, &b_pieces);
	if (pb[IQROOK].p != -ROOK) shiftup(pb, IQROOK, &b_pieces);
	if (pb[IKBISHOP].p != -BISHOP) shiftup(pb, IKBISHOP, &b_pieces);
	if (pb[IQBISHOP].p != -BISHOP) shiftup(pb, IQBISHOP, &b_pieces);
	if (pb[IKKNIGHT].p != -KNIGHT) shiftup(pb, IKKNIGHT, &b_pieces);
	if (pb[IQKNIGHT].p != -KNIGHT) shiftup(pb, IQKNIGHT, &b_pieces);

	if (pw[IKBISHOP].p && !white_square(pw[IKBISHOP].pos)) {
		t = pw[IKBISHOP];
		pw[IKBISHOP] = pw[IQBISHOP];
		pw[IQBISHOP] = t;
	}

	if (pb[IKBISHOP].p && white_square(pb[IKBISHOP].pos)) {
		t = pb[IKBISHOP];
		pb[IKBISHOP] = pb[IQBISHOP];
		pb[IQBISHOP] = t;
	}

	/* now the pboard */
	memset(b->board, 0, sizeof(b->board));
	memset(b->pboard, -1, sizeof(b->pboard));

	pw = WHITEPIECES(b);
	for (i=0;i<32;i++, pw++) {
		if (pw->p)
			add_piece(b, pw->p, pw->pos, pw);
	}

	if (b->board[E1] != KING) {
		b->flags &= ~(WHITE_CASTLE_SHORT | WHITE_CASTLE_LONG);
	}

	if (b->board[E8] != -KING) {
		b->flags &= ~(BLACK_CASTLE_SHORT | BLACK_CASTLE_LONG);
	}

	if (b->board[H1] != ROOK) {
		b->flags &= ~WHITE_CASTLE_SHORT;
	}

	if (b->board[A1] != ROOK) {
		b->flags &= ~WHITE_CASTLE_LONG;
	}

	if (b->board[H8] != -ROOK) {
		b->flags &= ~BLACK_CASTLE_SHORT;
	}

	if (b->board[A8] != -ROOK) {
		b->flags &= ~BLACK_CASTLE_LONG;
	}

	b->hash1 ^= (b->enpassent<<8);
	b->hash1 ^= (b->flags & (FLAG_CAN_CASTLE)) << 16;  

	regen_moves(b);

	return 1;
}
