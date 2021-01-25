#include "includes.h"
#include "knightcap.h"

extern struct state *state;

#define ADD_GEN(to) \
{ \
	b->topieces[to] |= mask; \
}

#if USE_SLIDING_CONTROL
#define XRAY_ADD_GEN(to) \
{ \
	b->xray_topieces[to] |= mask; \
}


/* returns a mask of all the pieces that have an xray attack on 
   sq2 through sq1 */
static inline uint32 xray_attack(Position *b, Square sq1, Square sq2)
{
	uint32 mask;
	unsigned dir = sliding_map[sq1][sq2];

	if (dir == 0)
		return 0;

	/* first look for all pieces that directly attack sq1 and 
	   xray attack sq2. to be sure in the case of queens, you need to 
	   check for xray attacks on 2 adjacent squares when sq1 and sq2 are 
	   not themselves adjacent */
	if (sq2 - dir != sq1) {
		mask = (b->topieces[sq1] & 
			b->xray_topieces[sq2] &
			b->xray_topieces[sq2-dir]);
	} else {
		mask = (b->topieces[sq1] & 
			b->xray_topieces[sq2]);
	}

	/* the next bit is a hack for R---Q---R(sq1)--sq2 type combinations.
	   The signature we use for this is sq2 being xrayed by a major piece
	   through sq1 (which has already been detected in mask). Then we just 
	   include any other major piece of the same colour that xray attacks
	   square 2. */
	if (mask & MAJOR_MASK) {
		if (is_white(b->board[sq1])) {
			mask |= b->xray_topieces[sq2] & WMAJOR_MASK; 
		} else {
			mask |= b->xray_topieces[sq2] & BMAJOR_MASK;
		}
	}
	
	return mask;
}

/* this tells us whether there is a piece at sq2 
   that would block a sliding xray coming from sq1.
   if either suqare doesn't have a piece we 
   just answer "no" */
static inline int xray_through(Position *b, Square sq1, Square sq2)
{
	Piece piece1 = b->board[sq1], piece2 = b->board[sq2];
	
	if (!piece1 || !piece2)
		return 1;
	return ((b->sliding_mask & (1<<b->pboard[sq2])) && 
		same_color(piece1, piece2) &&  
		capture_map[KING+piece2][sq2][sq1]);
}
#endif


__fastfunc(static void pawn_generate(Position *b, int pi))
{
	uint32 mask = (1<<pi);
	PieceStruct *p = &b->pieces[pi];
	Square from = p->pos;
	int x = XPOS(from);

	if (p->p > 0) {
		/* white pawns */
		if (x != 0) {
			ADD_GEN(from + NORTH_WEST);
		}
		if (x != 7) {
			ADD_GEN(from + NORTH_EAST);
		}
	} else {
		/* black panws */
		if (x != 0) {
			ADD_GEN(from + SOUTH_WEST);
		}
		if (x != 7) {
			ADD_GEN(from + SOUTH_EAST);
		}
	}
}


__fastfunc(static void bishop_generate(Position *b, int pi))
{
#if !USE_SLIDING_CONTROL
	old_bishop_generate(b, pi);
	return;
#else
	uint32 mask = (1<<pi);
	PieceStruct *p = &b->pieces[pi];
	Square from = p->pos;
	int x = XPOS(from);
	int y = YPOS(from);       
	int l;
	Square to;

	l = imin(7-x, 7-y);
	to = from + NORTH_EAST;

	while (l--) {
		ADD_GEN(to);
		if (b->board[to]) break;
		to += NORTH_EAST;
	}

	if (l>0) {
		while (l-- && xray_through(b,from,to)) {
			to += NORTH_EAST;
			XRAY_ADD_GEN(to);
		}
	}

	l = imin(x, 7-y);
	to = from + NORTH_WEST;

	while (l--) {
		ADD_GEN(to);
		if (b->board[to]) break;
		to += NORTH_WEST;
	}

	if (l>0) {
		while (l-- && xray_through(b,from,to)) {
			to += NORTH_WEST;
			XRAY_ADD_GEN(to);
		}
	}

	l = imin(x, y);
	to = from + SOUTH_WEST;

	while (l--) {
		ADD_GEN(to);
		if (b->board[to]) break;
		to += SOUTH_WEST;
	}

	if (l>0) {
		while (l-- && xray_through(b,from,to)) {
			to += SOUTH_WEST;
			XRAY_ADD_GEN(to);
		}
	}

	l = imin(7-x, y);
	to = from + SOUTH_EAST;

	while (l--) {
		ADD_GEN(to);
		if (b->board[to]) break;
		to += SOUTH_EAST;
	}

	if (l>0) {
		while (l-- && xray_through(b,from,to)) {
			to += SOUTH_EAST;
			XRAY_ADD_GEN(to);
		}
	}
#endif
}


__fastfunc(static void rook_generate(Position *b, int pi))
{
#if !USE_SLIDING_CONTROL
	old_rook_generate(b, pi);
	return;
#else
	uint32 mask = (1<<pi);
	PieceStruct *p = &b->pieces[pi];
	Square from = p->pos;
	int x = XPOS(from);
	int y = YPOS(from);       
	int l;
	Square to;

	l = 7-y;
	to = from + NORTH;

	while (l--) {
		ADD_GEN(to);
		if (b->board[to]) break;
		to += NORTH;
	}

	if (l>0) {
		while (l-- && xray_through(b, from, to)) {
			to += NORTH;
			XRAY_ADD_GEN(to);
		}
	}

	l = x;
	to = from + WEST;

	while (l--) {
		ADD_GEN(to);
		if (b->board[to]) break;
		to += WEST;
	}

	if (l>0) {
		while (l-- && xray_through(b, from, to)) {
			to += WEST;
			XRAY_ADD_GEN(to);
		}
	}

	l = y;
	to = from + SOUTH;

	while (l--) {
		ADD_GEN(to);
		if (b->board[to]) break;
		to += SOUTH;
	}

	if (l>0) {
		while (l-- && xray_through(b, from, to)) {
			to += SOUTH;
			XRAY_ADD_GEN(to);
		}
	}

	l = 7-x;
	to = from + EAST;

	while (l--) {
		ADD_GEN(to);
		if (b->board[to]) break;
		to += EAST;
	}

	if (l>0) {
		while (l-- && xray_through(b, from, to)) {
			to += EAST;
			XRAY_ADD_GEN(to);
		}
	}
#endif
}


__fastfunc(static void knight_generate(Position *b, int pi))
{
	uint32 mask = (1<<pi);
	PieceStruct *p = &b->pieces[pi];
	Square from = p->pos;
	int x = XPOS(from);
	int y = YPOS(from);       
	Square to;

	if (x == 7) goto west;

	if (y<6) {
		to = from + 2*NORTH + EAST;
		ADD_GEN(to);
	}

	if (y>1) {
		to = from + EAST + 2*SOUTH;
		ADD_GEN(to);
	}

	if (x == 6) goto west;

	if (y<7) {
		to = from + NORTH + 2*EAST;
		ADD_GEN(to);
	}

	if (y>0) {
		to = from + SOUTH + 2*EAST;
		ADD_GEN(to);
	}

west:	
	if (x == 0) return;

	if (y>1) {
		to = from + WEST + 2*SOUTH;
		ADD_GEN(to);
	}

	if (y<6) {
		to = from + WEST + 2*NORTH;
		ADD_GEN(to);
	}

	if (x == 1) return;

	if (y>0) {
		to = from + 2*WEST + SOUTH;
		ADD_GEN(to);
	}
	
	if (y<7) {
		to = from + 2*WEST + NORTH;
		ADD_GEN(to);
	}
}


__fastfunc(static void king_generate(Position *b, int pi))
{
	uint32 mask = (1<<pi);
	PieceStruct *p = &b->pieces[pi];
	Square from = p->pos;
	int x = XPOS(from);
	int y = YPOS(from);       
	Square to;

	if (x != 0) {
		to = from + WEST;
		ADD_GEN(to);
	}

	if (x != 7) {
		to = from + EAST;
		ADD_GEN(to);
	}

	if (y == 7) goto south;
	
	if (x != 0) {
		to = from + NORTH_WEST;
		ADD_GEN(to);
	}

	if (x != 7) {
		to = from + NORTH_EAST;
		ADD_GEN(to);
	}

	to = from + NORTH;
	ADD_GEN(to);

south:
	if (y == 0) return;

	if (x != 0) {
		to = from + SOUTH_WEST;
		ADD_GEN(to);
	}

	if (x != 7) {
		to = from + SOUTH_EAST;
		ADD_GEN(to);
	}

	to = from + SOUTH;
	ADD_GEN(to);
}


__fastfunc(static void queen_generate(Position *b, int pi))
{
	rook_generate(b, pi);
	bishop_generate(b, pi);
}


__fastfunc(static void piece_generate(Position *b, int pi))
{
	b->piece_change |= (1<<pi);

	switch (abs(b->pieces[pi].p)) {
	case PAWN:
		pawn_generate(b, pi);
		break;
		
	case BISHOP:
		bishop_generate(b, pi);
		break;
		
	case KNIGHT:
		knight_generate(b, pi);
		break;
		
	case ROOK:
		rook_generate(b, pi);
		break;
		
	case QUEEN:
		queen_generate(b, pi);
		break;
		
	case KING:
		king_generate(b, pi);
		break;

	default:
		lprintf(0, "unknown pi %d p=%d pos=%s in piece_generate\n", 
			pi, b->pieces[pi].p, posstr(b->pieces[pi].pos));
		break;
	}
}

#if !USE_SLIDING_CONTROL
void old_update_moves(Position *b, Position *oldb, Move *move)
{
	unsigned pi, dir;
	uint32 mask_from, mask_to, mask;
	int pos, p2;

	/* wipe the piece that is moving */
	mask = (1<<oldb->pboard[move->from]);

	/* a piece being captured - always wipe */
	if (oldb->pboard[move->to] != -1) {
		mask |= (1<<oldb->pboard[move->to]); 
	}

	mask = ~mask;

	for (pos=A1;pos<=H8;pos+=4) {
		b->topieces[pos] = oldb->topieces[pos] & mask;
		b->topieces[pos+1] = oldb->topieces[pos+1] & mask;
		b->topieces[pos+2] = oldb->topieces[pos+2] & mask;
		b->topieces[pos+3] = oldb->topieces[pos+3] & mask;
	}

	mask_from = b->topieces[move->from] & b->sliding_mask; 
	mask_to = b->topieces[move->to] & b->sliding_mask; 

	/* sliding pieces that can move to the from square */
	mask = mask_from;
	while (mask) {
		pi = ff_one(mask);
		mask &= ~(1<<pi);

		pos = b->pieces[pi].pos;

		if (same_line(move->from, move->to, pos)) {
			/* if we are moving towards this piece then it
			   removes some topieces elements */
			dir = sliding_map[move->from][move->to];
			p2 = move->from;
			while (p2 != move->to) {
				b->topieces[p2] &= ~(1<<pi);
				p2 += dir;
			}
		} else if (same_line(pos, move->from, move->to)) {
			/* if we are moving away from this piece then
			   it adds some topieces elements */
			dir = sliding_map[move->from][move->to];
			p2 = move->from;
			while (p2 != move->to) {
				p2 += dir;
				b->topieces[p2] |= (1<<pi);
			}
		} else {
			/* we are moving off the line of this piece 
			   we add some topieces elements */
			dir = sliding_map[pos][move->from];
			p2 = move->from;
			while (!off_board(p2, dir) && !b->board[p2]) {
				p2 += dir;
				b->topieces[p2] |= (1<<pi);
			}
		}
	}


	/* sliding pieces that can move to the to square - their moves
	   along that line might get truncated */
	mask = mask_to;

	while (mask) {
		pi = ff_one(mask);
		mask &= ~(1<<pi);

		pos = b->pieces[pi].pos;

		dir = sliding_map[pos][move->to];
		p2 = move->to;
		while (!off_board(p2, dir)) {
			p2 += dir;
			b->topieces[p2] &= ~(1<<pi);
			if (b->board[p2]) break;
		}
	}

	/* finally regenerate the piece that is moving */
	piece_generate(b, b->pboard[move->to]);


        /* ugly. we need to recalc pawns because of changes in doubled,
           isolated or passed pawns */
        {
                int minx=0, maxx=0, i;
                if (abs(oldb->board[move->from]) == PAWN) {
                        minx = XPOS(move->from) - 1;
                        maxx = XPOS(move->from) + 1;
                        if (XPOS(move->to) != XPOS(move->from)) {
                                minx = imin(minx, XPOS(move->to) - 1);
                                maxx = imax(maxx, XPOS(move->to) + 1);
                        }
                } else if (abs(oldb->board[move->to]) == PAWN) {
                        minx = XPOS(move->to) - 1;
                        maxx = XPOS(move->to) + 1;
                }
		
                if (maxx != 0) {
                        for (i=8;i<16;i++) {
                                if (b->pieces[i].p == PAWN &&
                                    XPOS(b->pieces[i].pos) >= minx &&
                                    XPOS(b->pieces[i].pos) <= maxx)
                                        b->piece_change |= (1<<i);
                        }
			
                        for (i=24;i<32;i++) {
                                if (b->pieces[i].p == -PAWN &&
                                    XPOS(b->pieces[i].pos) >= minx &&
                                    XPOS(b->pieces[i].pos) <= maxx)
                                        b->piece_change |= (1<<i);
                        }
                }
        }
}
#endif

/* this routine is the core of the move generator. It is incremental,
   the topieces[] array always contains a bitmap of what pieces can move
   to that square. update_moves() is called to update the topieces[]
   array after each move. It is only called for "simple moves", ie not
   enpassent captures or castling */
__fastfunc(void update_moves(Position *b, Position *oldb, Move *move))
{
#if !USE_SLIDING_CONTROL
	old_update_moves(b,oldb,move);
	return;
#else
	unsigned pi, dir;
	uint32 mask_from, mask_to, mask, xray_mask;
	int pos, p2;
	int sc, case2;
	Piece piece1, piece2;


	/* the piece that is moving */
	piece1 = oldb->board[move->from];

	/* wipe the piece that is moving */
	mask = (1<<oldb->pboard[move->from]);

	/* a piece being captured - always wipe */
	if (oldb->pboard[move->to] != -1) {
		mask |= (1<<oldb->pboard[move->to]); 
	}

	mask = ~mask;

	for (pos=A1;pos<=H8;pos+=4) {
		b->topieces[pos] = oldb->topieces[pos] & mask;
		b->topieces[pos+1] = oldb->topieces[pos+1] & mask;
		b->topieces[pos+2] = oldb->topieces[pos+2] & mask;
		b->topieces[pos+3] = oldb->topieces[pos+3] & mask;
		b->xray_topieces[pos] = oldb->xray_topieces[pos] & mask;
		b->xray_topieces[pos+1] = oldb->xray_topieces[pos+1] & mask;
		b->xray_topieces[pos+2] = oldb->xray_topieces[pos+2] & mask;
		b->xray_topieces[pos+3] = oldb->xray_topieces[pos+3] & mask;
	}

	mask_from = b->topieces[move->from] & b->sliding_mask; 
	mask_to = b->topieces[move->to] & b->sliding_mask; 

	/* sliding pieces that can move to the from square */
	mask = mask_from;
	while (mask) {
		pi = ff_one(mask);
		mask &= ~(1<<pi);

		pos = b->pieces[pi].pos;
		piece2 = b->pieces[pi].p;
		
		sc = same_color(piece1,piece2); 

		if (same_line(move->from, move->to, pos)) {
			/* if we are moving towards this piece then it
			   removes some topieces elements */
			dir = sliding_map[move->from][move->to];
			p2 = move->from;

			xray_mask = ~xray_attack(oldb, pos, move->to);

			while (p2 != move->to) {
				b->topieces[p2] &= ~(1<<pi);
				/* if the pieces are the same colour
				   then the same squares become
				   xray attacked by piece2. If not, we must
				   remove any xray attack on
				   those squares by pieces through
				   piece2.  */
				if (sc) {
					b->xray_topieces[p2] |=	(1<<pi);
				} else {
					b->xray_topieces[p2] &= xray_mask;
				}
				p2 += dir;
			}
		} else if (same_line(pos, move->from, move->to)) {
			/* if we are moving away from this piece then
			   it adds some topieces elements */
			dir = sliding_map[move->from][move->to];
			p2 = move->from;

			xray_mask = xray_attack(oldb, pos, move->from);

			while (p2 != move->to) {
				p2 += dir;
				b->topieces[p2] |= (1<<pi);
				/* the same squares are no  longer
				   xray attacked by piece2. 
				   we must include any xray attack on
				   those squares by pieces through
				   piece2.  */
				b->xray_topieces[p2] &= ~(1<<pi);
				b->xray_topieces[p2] |= xray_mask;
			}
			
			/* If a piece has been captured it may open 
			   up some more xray_attacks on the other side 
			   of the to square */
			if (oldb->pboard[move->to] != -1) {
				xray_mask |= (1<<pi);
				while (!off_board(p2, dir) && xray_through(b,pos,p2)) {
					p2 += dir;
					b->xray_topieces[p2] |= xray_mask; 
				}
			}
		} else {
			/* we are moving off the line of this piece. we
			   add some topieces elements. */

			/* This is complicated for xray attacks. Here is
			   the logic: 
			     1) any xray attacks through piece2 and
			         the from square get extended along
			         that line.  
			     2) any xray attacks originating at piece2
			        and propagating through a piece on
			        the other side of the from square get
			        included.
			     3) any xray attacks on the other side of
                                the from square by piece2 are removed
                                (up to the next piece). */


			dir = sliding_map[pos][move->from];
			p2 = move->from;

			xray_mask = xray_attack(oldb, pos, move->from);
			while (!off_board(p2, dir)) {
				p2 += dir;
				b->topieces[p2] |= (1<<pi);
				b->xray_topieces[p2] |= xray_mask; /* 1 */
				b->xray_topieces[p2] &= ~(1<<pi);  /* 3 */
				if (b->board[p2])
					break;
			}

			/* if we stopped because of a piece, we might have to 
			   add xray attacks to the squares on the other side of
			   that piece. */

			xray_mask |= (1<<pi);
			while (!off_board(p2, dir) && xray_through(b,pos,p2)) {
				p2 += dir;
				b->xray_topieces[p2] |= xray_mask; /* 2 */
			}
		}
	}


	/* sliding pieces that can move to the to square - their moves
	   along that line might get truncated. */

	/* This is complicated for xray attacks. Here is the 
	   logic: 
	   1) if the pieces are different colours, or they are the same colour but 
	      piece1 does not attack piece2 or piece1 is not a sliding piece:
	      1a) any xray attacks through piece2 and the to square get truncated 
	          at the to square.
	      1b) any xray attacks originating at piece2 and propagating through a 
	          piece on the other side of the to square get truncated at that piece
		  (i.e. removed altogether).
	   2) if not 1) then the pieces are the same colour and they mutually attack:
	      2a) piece2 and any xray attacks through piece2 
	          now xray-attack the squares on the other side of the to 
	          square.
	  */

	mask = mask_to;

	while (mask) {
		pi = ff_one(mask);
		mask &= ~(1<<pi);

		pos = b->pieces[pi].pos;
		piece2 = b->pieces[pi].p;
		
		sc = same_color(piece1,piece2); 
		case2 = sc && capture_map[KING + piece1][move->to][pos] && 
			(b->sliding_mask & (1<<oldb->pboard[move->from]));

		dir = sliding_map[pos][move->to];

		xray_mask = xray_attack(oldb, pos, move->to) | (1<<pi);
		p2 = move->to;
		while (!off_board(p2, dir)) {
			p2 += dir;
			b->topieces[p2] &= ~(1<<pi);
			if (case2) {
				b->xray_topieces[p2] |= xray_mask; /* 2a */
			} else {
				b->xray_topieces[p2] &= ~xray_mask; /* 1a */
			}
			if (b->board[p2])
				break;
		}

		/* if we stopped becuase of a piece we need to check that 
		   xray attacks don't go through that piece */

		while (!off_board(p2, dir) && xray_through(b, pos, p2)) {
			p2 += dir;
			if (case2) {
				b->xray_topieces[p2] |= xray_mask; /* 2a */
			} else {
				b->xray_topieces[p2] &= ~xray_mask; /* 1b */
			}
		}
	}

	/* finally regenerate the piece that is moving */
	piece_generate(b, b->pboard[move->to]);


        /* ugly. we need to recalc pawns because of changes in doubled,
           isolated or passed pawns */
        {
                int minx=0, maxx=0, i;
                if (abs(oldb->board[move->from]) == PAWN) {
                        minx = XPOS(move->from) - 1;
                        maxx = XPOS(move->from) + 1;
                        if (XPOS(move->to) != XPOS(move->from)) {
                                minx = imin(minx, XPOS(move->to) - 1);
                                maxx = imax(maxx, XPOS(move->to) + 1);
                        }
                } else if (abs(oldb->board[move->to]) == PAWN) {
                        minx = XPOS(move->to) - 1;
                        maxx = XPOS(move->to) + 1;
                }
		
                if (maxx != 0) {
                        for (i=8;i<16;i++) {
                                if (b->pieces[i].p == PAWN &&
                                    XPOS(b->pieces[i].pos) >= minx &&
                                    XPOS(b->pieces[i].pos) <= maxx)
                                        b->piece_change |= (1<<i);
                        }
			
                        for (i=24;i<32;i++) {
                                if (b->pieces[i].p == -PAWN &&
                                    XPOS(b->pieces[i].pos) >= minx &&
                                    XPOS(b->pieces[i].pos) <= maxx)
                                        b->piece_change |= (1<<i);
                        }
                }
        }
#endif
}


/* this is test code to see that update moves is working absolutely 
   correctly */
void update_moves1(Position *b, Position *oldb, Move *move)
{
	int i;
	Position b1;
	int bad=0, xray_bad=0;

	b1 = (*b);

	update_moves1(b, oldb, move);
	regen_moves(&b1);
	
	for (i=A1;i<=H8;i++) {
		if (b->topieces[i] != b1.topieces[i]) {
			lprintf(0,"%s %08x %08x\n",
				posstr(i), b->topieces[i],
				b1.topieces[i]);
			bad++;
		}
#if USE_SLIDING_CONTROL
		if (b->xray_topieces[i] != b1.xray_topieces[i]) {
			lprintf(0,"%s %08x %08x %08x\n",
				posstr(i), b->xray_topieces[i],
				b1.xray_topieces[i], oldb->xray_topieces[i]);
			xray_bad++;
		}
#endif
	}

	if (bad) {
		lprintf(0,"%s %d bad\n", short_movestr(oldb, move), bad);
		print_board(oldb->board);
	}
#if USE_SLIDING_CONTROL
	if (xray_bad) {
		lprintf(0,"%s %d xray bad\n", short_movestr(oldb, move), xray_bad);
		print_board(oldb->board);
	}
#endif
}


/* this is called when the topieces[] array needs to be completely redone */
void regen_moves(Position *b)
{
	int pi;
	memset(b->topieces, 0, sizeof(b->topieces));
#if USE_SLIDING_CONTROL
	memset(b->xray_topieces, 0, sizeof(b->xray_topieces));
#endif
	b->oldb = NULL;

	for (pi=0;pi<32;pi++) {
		if (!b->pieces[pi].p) continue;
		piece_generate(b, pi);
	}

	b->flags &= ~FLAG_EVAL_DONE;
	b->flags &= ~FLAG_DONE_TACTICS;
	b->piece_change = b->material_mask;

	eval(b, INFINITY, MAX_DEPTH);
}


/* return the number of moves, and put all the moves in moves[].
   This assumes that moves[] is big enough to hold all the moves */
int generate_moves(Position *b, Move *moves)
{
	int n, i;
	PieceStruct *p;
	uint32 pieces;
	int to;

	n = 0;

	if (whites_move(b)) {
		p = WHITEPIECES(b);

		for (to=A1;to<=H8;to++) {
			if (b->board[to]>0) continue;
			pieces = b->topieces[to] & WHITE_MASK;
			if (b->board[to]==0)
				pieces &= b->piece_mask;
			while (pieces) {
				i = ff_one(pieces);
				moves->from = p[i].pos;
				moves->to = to;
				moves++;
				n++;
				pieces &= ~(1<<i);
			}
		}

		if ((to=b->enpassent) && (b->topieces[to] & WPAWN_MASK)) {
			if (XPOS(to) != 0 && b->board[to+SOUTH_WEST] == PAWN) {
				moves->from = to+SOUTH_WEST;
				moves->to = to;
				moves++;
				n++;
			}
			if (XPOS(to) != 7 && b->board[to+SOUTH_EAST] == PAWN) {
				moves->from = to+SOUTH_EAST;
				moves->to = to;
				moves++;
				n++;
			}
		}


		if (b->flags & WHITE_CASTLE_SHORT) {
			if (b->board[F1] == 0 && b->board[G1] == 0 &&
			    !(b->topieces[E1] & BLACK_MASK) &&
			    !(b->topieces[F1] & BLACK_MASK) &&
			    !(b->topieces[G1] & BLACK_MASK)) {
				moves->from = E1;
				moves->to = G1;
				moves++;
				n++;
			}
		}
		if (b->flags & WHITE_CASTLE_LONG) {
			if (b->board[D1] == 0 && b->board[C1] == 0 &&
			    b->board[B1] == 0 &&
			    !(b->topieces[E1] & BLACK_MASK) &&
			    !(b->topieces[D1] & BLACK_MASK) &&
			    !(b->topieces[C1] & BLACK_MASK)) {
				moves->from = E1;
				moves->to = C1;
				moves++;
				n++;
			}
		}

		p = &WHITEPIECES(b)[8];
		for (i=0;i<8;i++, p++) {
			if (p->p != PAWN) continue;
			if (b->board[p->pos+NORTH]) continue;
			moves->from = p->pos;
			moves->to = p->pos+NORTH;
			moves++;
			n++;
			if (YPOS(p->pos) != 1 || b->board[p->pos+2*NORTH]) continue;
			moves->from = p->pos;
			moves->to = p->pos+2*NORTH;
			moves++;
			n++;			
		}
	} else {
		p = WHITEPIECES(b);

		for (to=A1;to<=H8;to++) {
			if (b->board[to]<0) continue;
			pieces = b->topieces[to] & BLACK_MASK;
			if (b->board[to]==0)
				pieces &= b->piece_mask;
			while (pieces) {
				i = ff_one(pieces);
				moves->from = p[i].pos;
				moves->to = to;
				moves++;
				n++;
				pieces &= ~(1<<i);
			}
		}

		if ((to=b->enpassent) && (b->topieces[to] & BPAWN_MASK)) {
			if (XPOS(to) != 0 && b->board[to+NORTH_WEST] == -PAWN) {
				moves->from = to+NORTH_WEST;
				moves->to = to;
				moves++;
				n++;
			}
			if (XPOS(to) != 7 && b->board[to+NORTH_EAST] == -PAWN) {
				moves->from = to+NORTH_EAST;
				moves->to = to;
				moves++;
				n++;
			}
		}


		if (b->flags & BLACK_CASTLE_SHORT) {
			if (b->board[F8] == 0 && b->board[G8] == 0 &&
			    !(b->topieces[E8] & WHITE_MASK) &&
			    !(b->topieces[F8] & WHITE_MASK) &&
			    !(b->topieces[G8] & WHITE_MASK)) {
				moves->from = E8;
				moves->to = G8;
				moves++;
				n++;
			}
		}
		if (b->flags & BLACK_CASTLE_LONG) {
			if (b->board[D8] == 0 && b->board[C8] == 0 &&
			    b->board[B8] == 0 &&
			    !(b->topieces[E8] & WHITE_MASK) &&
			    !(b->topieces[D8] & WHITE_MASK) &&
			    !(b->topieces[C8] & WHITE_MASK)) {
				moves->from = E8;
				moves->to = C8;
				moves++;
				n++;
			}
		}

		p = &BLACKPIECES(b)[8];
		for (i=0;i<8;i++, p++) {
			if (p->p != -PAWN) continue;
			if (b->board[p->pos+SOUTH]) continue;
			moves->from = p->pos;
			moves->to = p->pos+SOUTH;
			moves++;
			n++;
			if (YPOS(p->pos) != 6 || b->board[p->pos+2*SOUTH]) continue;
			moves->from = p->pos;
			moves->to = p->pos+2*SOUTH;
			moves++;
			n++;			
		}
	}

	return n;
}

int generate_check_avoidance(Position *b, Move *moves)
{
	int n=0;
	int kpos, pos, pi, pi2, dx, dy, to, dir;
	int x1, x2, y1, y2, x, y;
	uint32 giving_check, pieces;

	if (whites_move(b)) {
		kpos = WHITEPIECES(b)[IKING].pos;
		giving_check = b->topieces[kpos] & BLACK_MASK;
	} else {
		kpos = BLACKPIECES(b)[IKING].pos;
		giving_check = b->topieces[kpos] & WHITE_MASK;
	}

	pos = kpos;

	/* which piece is giving check? */
	pi = ff_one(giving_check);
	if (giving_check & ~(1<<pi))
		goto king_moves; /* its double check */

	pos = b->pieces[pi].pos;

	/* try to capture it */
	pieces = b->topieces[pos];

	/* a special case - if its a pawn and enpassent is possible then
	   take enpassent */
	if (b->enpassent && !(giving_check & b->piece_mask)) {
		pieces |= b->topieces[b->enpassent] & ~b->piece_mask;
	}

	if (whites_move(b)) {
		pieces &= WHITE_MASK;
	} else {
		pieces &= BLACK_MASK;
	}

	while (pieces) {
		pi2 = ff_one(pieces);
		moves->from = b->pieces[pi2].pos;
		if (abs(b->board[moves->from]) == PAWN &&
		    YPOS(pos) == YPOS(moves->from))
			moves->to = b->enpassent;
		else
			moves->to = pos;
		moves++;
		n++;
		pieces &= ~(1<<pi2);
	}


	/* now try to block it if its a sliding piece */
	if (!(giving_check & b->sliding_mask))
		goto king_moves;

	dx = XPOS(kpos) - XPOS(pos);
	dy = YPOS(kpos) - YPOS(pos);

	dir = 0;
	if (dy > 0) 
		dir += SOUTH;
	else if (dy < 0) 
		dir += NORTH;

	if (dx > 0) 
		dir += WEST;
	else if (dx < 0) 
		dir += EAST;
	
	to = kpos + dir;
	while (to != pos) {
		if (whites_move(b)) {
			pieces = (b->topieces[to] & WHITE_MASK) & ~WKING_MASK;
			pieces &= b->piece_mask;
			if (b->board[to+SOUTH] == PAWN) {
				pieces |= (1<<b->pboard[to+SOUTH]);
			} else if (YPOS(to) == 3 && b->board[to+SOUTH] == 0 &&
				   b->board[to+2*SOUTH] == PAWN) {
				pieces |= (1<<b->pboard[to+2*SOUTH]);
			}
		} else {
			pieces = (b->topieces[to] & BLACK_MASK) & ~BKING_MASK;
			pieces &= b->piece_mask;
			if (b->board[to+NORTH] == -PAWN) {
				pieces |= (1<<b->pboard[to+NORTH]);
			} else if (YPOS(to) == 4 && b->board[to+NORTH] == 0 &&
				   b->board[to+2*NORTH] == -PAWN) {
				pieces |= (1<<b->pboard[to+2*NORTH]);
			}
		}
		while (pieces) {
			pi2 = ff_one(pieces);
			moves->from = b->pieces[pi2].pos;
			moves->to = to;
			moves++;
			n++;
			pieces &= ~(1<<pi2);
		}
		to += dir;
	}


king_moves:
	/* try to move the king */
	x1 = XPOS(kpos) - 1;
	x2 = x1 + 2;
	y1 = YPOS(kpos) - 1;
	y2 = y1 + 2;

	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;
	if (x2 == 8) x2 = 7;
	if (y2 == 8) y2 = 7;

	for (x=x1;x<=x2;x++)
		for (y=y1;y<=y2;y++) {
			int pos2 = POSN(x,y);
			if (pos2 == kpos || pos2 == pos) continue;
			pieces = b->topieces[pos2];
			if (whites_move(b)) {
				if (pieces & BLACK_MASK) continue;
				if (b->board[pos2] > 0) continue;
			} else {
				if (pieces & WHITE_MASK) continue;
				if (b->board[pos2] < 0) continue;
			}

			/* the king can't move to any square that is in the 
			   capture map of the pieces giving check */
			pieces = giving_check & b->sliding_mask;
			while (pieces) {
				Square ppos;
				pi = ff_one(pieces);
				ppos = b->pieces[pi].pos;
				if (same_line(ppos, kpos, pos2))
					break;
				pieces &= ~(1<<pi);
			}

			moves->from = kpos;
			moves->to = pos2;

			if (pieces) {
#if 0
				Position b1;
				if (do_move(&b1, b, moves)) {
					lprintf(0, "legal move %s\n", 
						short_movestr(b, moves));
					print_board(b->board);
				}
#endif
				continue;
			}

#if 0
			{
				Position b1;
				if (!do_move(&b1, b, moves)) {
					lprintf(0, "illegal move %s\n", 
						short_movestr(b, moves));
					print_board(b->board);
				}
			}
#endif

			moves++;
			n++;
		}

	return n;
}





/* this is used to make sure the player didn't make an illegal move -
   it is not speed critical */
int legal_move(Position *b, Move *move)
{
	Move moves[MAX_MOVES];
	int num_moves;
	int m;
	Position b1;

	num_moves = generate_moves(b, moves);

	for (m=0;m<num_moves;m++)
		if (moves[m].from == move->from &&
		    moves[m].to == move->to) break;

	if (m == num_moves) return 0;

	if (!do_move(&b1, b, move))
		return 0;

	return 1;
}


static Move *get_move_list(int ply)
{
	static int num_move_lists;
	static Move **move_lists;
	int j;

	if (num_move_lists > ply) {
		return move_lists[ply];
	}

	move_lists = (Move **)Realloc(move_lists,
				      (num_move_lists+30)*sizeof(Move *));
	for (j=num_move_lists;j<num_move_lists+30;j++) {
		move_lists[j] = (Move *)Realloc(NULL, MAX_MOVES*sizeof(Move));
	}

	num_move_lists += 30;
	
	return move_lists[ply];
}


void gen_move_list(Position *b, int ply, Move *m1, Eval testv)
{
	int n;
	Move *moves;
	
	if (b->moves) {
		lprintf(0,"already has a moves list\n");
	}

	b->moves = get_move_list(ply);
	moves = b->moves;

	if (b->flags & FLAG_CHECK) {
		n = generate_check_avoidance(b, moves);
	} else {
		n = generate_moves(b, moves);
	}

	b->num_moves = n;
	order_moves(b, moves, n, ply, m1, testv);


	if (state->dementia != 0) {
		int i;
		for (i=0;i<b->num_moves-1;i++) {
			int dist = distance(moves[i].from, moves[i].to);
			int probs[8] = {0, 10, 15, 20, 20, 25, 25, 30};
			int p = probs[dist] * state->dementia;
			if (b->board[moves[i].to]) p *= 2;
			if ((random()%100) <= p) {
				memmove(moves+i, moves+i+1, 
					sizeof(moves[0])*(b->num_moves-(i+1)));
				b->num_moves--;
				i--;
			}
		}
	}
}


int possible_move(Position *b, int ply, Move *m)
{
	int n, i;
	Move *moves;

	moves = get_move_list(ply+1);

	n = generate_moves(b, moves);

	for (i=0;i<n;i++)
		if (same_move(m, &moves[i])) return 1;

	return 0;
}


