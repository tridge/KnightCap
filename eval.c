/* the evaluation functions */

#include "includes.h"
#include "knightcap.h"
#include "small_coeffs.h"

#define EVAL_ALL_DEPTH 4

#define EVAL_SHORTCUT_THRESHOLD (2.0*PAWN_VALUE)
#define EVAL_SHORTCUT_OFFSET (1.5*PAWN_VALUE)

/* coefficients we don't want changed by TD(lambda) (multiplicative factors, 
 the value of a pawn, and the value of a draw */
int dont_change[] = {0, 1, 34, 37, 549, 550, 551, 556, 557, -1};

extern int learning;
extern int mulling;
extern struct state *state;
int debug;

/* this is the value of pawns on open files as they move up the board
   - it is doubled for passed pawns */
static CONST_EVAL etype *pawn_advance = &coefficients[IPAWN_ADVANCE];

/* value of pawn configurations around the king */
static CONST_EVAL etype *pawn_defence = &coefficients[IPAWN_DEFENCE];
static unsigned char black_pawn_loc[8];
static unsigned char white_pawn_loc[8];

#define PAWN_LOC(player) (player>0?white_pawn_loc:black_pawn_loc)

int pop_count[256];


static CONST_EVAL etype *pawn_pos_value = &coefficients[IPAWN_POS];

#if USE_NULLMOVE_FIXUP
/* this corrects the unstoppable pawn evaluation on null moves */
void update_pawns(Position *b) 
{
	if (whites_move(b)) {
		b->eval_result += (pop_count32(b->null_stoppable_pawn & BPAWN_MASK) + 
				   pop_count32(b->null_unstoppable_pawn & WPAWN_MASK))*
			UNSTOPPABLE_PAWN;
	} else {
		b->eval_result -= (pop_count32(b->null_stoppable_pawn & WPAWN_MASK) + 
				   pop_count32(b->null_unstoppable_pawn & BPAWN_MASK))*
			UNSTOPPABLE_PAWN;
	}
}
#endif

/* this evaluates a pawn - its more valuable if its a passed
   pawn or on an open file. In those cases it gains value
   as it moves up the board */
static etype eval_white_pawn(Position *b, int pi)
{
	PieceStruct *piece = b->pieces + pi;
	etype ret=0;
	int x = XPOS(piece->pos);
	int y = YPOS(piece->pos);
	etype advance = 0;
	int weak = 0;

	ret = pawn_pos_value[piece->pos];

	/* a pawn is weak if it isn't supported by a pawn and can't
	   move to a place where it is supported by a pawn */

	if (b->topieces[piece->pos] & WPAWN_MASK) 
		goto not_weak;
	     
	if ((b->topieces[piece->pos+NORTH] & WPAWN_MASK) &&
	    abs(b->board[piece->pos+NORTH]) != PAWN)
		goto not_weak;

	if (y == 1 && !(b->topieces[piece->pos+NORTH] & BPAWN_MASK) &&
	    (b->topieces[piece->pos+2*NORTH] & WPAWN_MASK) &&
	    (abs(b->board[piece->pos+NORTH]) != PAWN) &&
	    (abs(b->board[piece->pos+2*NORTH]) != PAWN))
		goto not_weak;
	    
	if (debug)
		lprintf(0,"weak %s\n", posstr(piece->pos));
	weak = 1;

	ret -= WEAK_PAWN;

	/* if its on a half open file then its even weaker */
	if (black_pawn_loc[x] == 0)
		ret -= MEGA_WEAK_PAWN;

	/* attacks on weak pawns are worth something */
	if (((b->topieces[piece->pos] & ~b->pinned_mask)>>16) & 0xFF)
		ret -= pop_count[((b->topieces[piece->pos] &
				   ~b->pinned_mask)>>16) & 0xFF] * 
			WEAK_PAWN_ATTACK_VALUE;
	
not_weak:

	if (black_pawn_loc[x] < y) {
		int n = 7-y;
		if (!weak) {
			advance = shiftr2(pawn_advance[n]);
		}
		if ((x==0 || black_pawn_loc[x-1] <= y) &&
		    (x==7 || black_pawn_loc[x+1] <= y)) {
			Square wkpos = WHITEPIECES(b)[IKING].pos;
			Square bkpos = BLACKPIECES(b)[IKING].pos;

			b->wpassed_pawn_mask |= (1<<pi);

			advance += pawn_advance[n];

			if (YPOS(bkpos) >= y && abs(XPOS(bkpos) - x) <= 1)
				advance = shiftr1(advance);

			if (YPOS(wkpos) >= y && abs(XPOS(wkpos) - x) <= 1)
				advance += KING_PASSED_PAWN_SUPPORT;

			if (b->board[piece->pos+NORTH] < 0)
				advance -= BLOCKED_PASSED_PAWN;

			if (x != 0 && white_pawn_loc[x-1] &&
			    abs(y - white_pawn_loc[x-1]) <= 1) {
				advance += shiftr1(pawn_advance[n]);
			}

			if (x != 7 && white_pawn_loc[x+1] &&
			    abs(y - white_pawn_loc[x+1]) <= 1) {
				advance += shiftr1(pawn_advance[n]);
			}

			if ((b->topieces[piece->pos] & ~b->pinned_mask & BKROOK_MASK) &&
			    XPOS(BLACKPIECES(b)[IKROOK].pos) == x &&
			    YPOS(BLACKPIECES(b)[IKROOK].pos) < y) {
				/* there is a black king rook 
				   behind our passed pawn */
				advance -= PASSED_PAWN_ROOK_ATTACK;
			}

			if ((b->topieces[piece->pos] & ~b->pinned_mask & BQROOK_MASK) &&
			    XPOS(BLACKPIECES(b)[IQROOK].pos) == x &&
			    YPOS(BLACKPIECES(b)[IQROOK].pos) < y) {
				/* there is a black queen rook 
				   behind our passed pawn */
				advance -= PASSED_PAWN_ROOK_ATTACK;
			}

			if ((b->topieces[piece->pos] & ~b->pinned_mask & WKROOK_MASK) &&
			    XPOS(WHITEPIECES(b)[IKROOK].pos) == x &&
			    YPOS(WHITEPIECES(b)[IKROOK].pos) < y) {
				/* there is a white king rook 
				   behind our passed pawn */
				advance += PASSED_PAWN_ROOK_SUPPORT;
			}

			if ((b->topieces[piece->pos] & ~b->pinned_mask & WQROOK_MASK) &&
			    XPOS(WHITEPIECES(b)[IQROOK].pos) == x &&
			    YPOS(WHITEPIECES(b)[IQROOK].pos) < y) {
				/* there is a white queen rook 
				   behind our passed pawn */
				advance += PASSED_PAWN_ROOK_SUPPORT;
			}

			/* a special case - is it unstoppable? */
			if ((b->piece_mask & BLACK_MASK) == BKING_MASK) {
				int kpos = BLACKPIECES(b)[IKING].pos;
				int kmoves = imax(abs(XPOS(kpos) - x), 7-YPOS(kpos));
				int pmoves = 7-y;
				if (blacks_move(b)) kmoves--;
				if (kmoves > pmoves) {
					/* its a candidate for unstopppability, but we need to
					   count all pieces that have to move out of the way,
					   except those on the queening square. If we find a pawn
					   in front then we assume the current pawn is stoppable */

					Square pos = b->pieces[pi].pos;
					
					while (!off_board(pos+NORTH,NORTH)) {
						pos += NORTH;
						if (b->board[pos]) {
							if (b->board[pos] == PAWN)
								pmoves += 10;
							else 
								++pmoves;
							if (pmoves >= kmoves)
								break;
						}
					} 
					if (kmoves > pmoves) {
						ret += UNSTOPPABLE_PAWN;
						if (debug)
							lprintf(0,"unstoppable %s\n",
								posstr(piece->pos));
						/* would it become stoppable after a null move? */
						if (whites_move(b) && kmoves == pmoves+1) {
							b->null_stoppable_pawn |= (1<<pi);
							if (debug)
								lprintf(0,"null stoppable %s\n",
									posstr(piece->pos));
						}
						/* would it become unstoppable after a null move? */
					} else if (blacks_move(b) && kmoves == pmoves) {
						b->null_unstoppable_pawn |= (1<<pi);
						if (debug)
							lprintf(0,"null unstoppable %s\n",
								posstr(piece->pos));
					}
				}
			}

			if (n < 4 && (ret + advance < pawn_advance[5])) {
				/* passed pawns always have some value */
				advance = pawn_advance[5] - ret;
			}
		}
		ret += advance;
	}

	if (white_pawn_loc[x] != y)
		ret -= DOUBLED_PAWN;

	if (x==0) {
		if (white_pawn_loc[x+1] == 0) ret -= ISOLATED_PAWN;
	} else if (x==7) {
		if (white_pawn_loc[x-1] == 0) ret -= ISOLATED_PAWN;
	} else if (white_pawn_loc[x-1] == 0 && white_pawn_loc[x+1] == 0) {
		ret -= ISOLATED_PAWN;
	}

	/* get rid of this because its not calculated incrementally and not clear 
	   that its really useful */
#if 0
	if (b->stage == ENDING) {
		if ((b->piece_mask & WBISHOP_MASK) == WKBISHOP_MASK) {
			if (!white_square(piece->pos)) ret += ODD_BISHOPS_PAWN_POS;
		} else if ((b->piece_mask & WBISHOP_MASK) == WQBISHOP_MASK) {
			if (white_square(piece->pos)) ret += ODD_BISHOPS_PAWN_POS;
		}
	}
#endif
	return ret;
}

static etype eval_black_pawn(Position *b, int pi)
{
	PieceStruct *piece = b->pieces + pi;
	etype ret=0;
	int x = XPOS(piece->pos);
	int y = YPOS(piece->pos);
	etype advance = 0;
	int weak = 0;

	ret = pawn_pos_value[mirror_square(piece->pos)];

	/* a pawn is weak if it isn't supported by a pawn and can't
	   move to a place where it is supported by a pawn */

	if (b->topieces[piece->pos] & BPAWN_MASK) 
		goto not_weak;

	if ((b->topieces[piece->pos+SOUTH] & BPAWN_MASK) &&
	    abs(b->board[piece->pos+SOUTH]) != PAWN)
		goto not_weak;


	if (y == 6 && !(b->topieces[piece->pos+SOUTH] & WPAWN_MASK) &&
	    (b->topieces[piece->pos+2*SOUTH] & BPAWN_MASK) &&
	    (abs(b->board[piece->pos+SOUTH]) != PAWN) &&
	    (abs(b->board[piece->pos+2*SOUTH]) != PAWN))
		goto not_weak;

	if (debug)
		lprintf(0,"weak %s\n", posstr(piece->pos));
	weak = 1;
	ret -= WEAK_PAWN;

	/* if its on a half open file then its even weaker */
	if (white_pawn_loc[x] == 0)
		ret -= MEGA_WEAK_PAWN;

	/* attacks on weak pawns are worth something */
	if (b->topieces[piece->pos] & ~b->pinned_mask & 0xFF)
		ret -= pop_count[b->topieces[piece->pos] &
				~b->pinned_mask & 0xFF] * WEAK_PAWN_ATTACK_VALUE;

 not_weak:

	if (white_pawn_loc[x] == 0 || white_pawn_loc[x] > y) {
		int n = y;
		if (!weak) {
			advance = pawn_advance[n] / 4;
		}
		if ((x==0 || white_pawn_loc[x-1]==0 ||
		     white_pawn_loc[x-1] >= y) &&
		    (x==7 || white_pawn_loc[x+1]==0 ||
		     white_pawn_loc[x+1] >= y)) {
			Square wkpos = WHITEPIECES(b)[IKING].pos;
			Square bkpos = BLACKPIECES(b)[IKING].pos;
#if 1
			b->bpassed_pawn_mask |= (1<<pi);
#endif
			advance += pawn_advance[n];

			if (YPOS(wkpos) <= y && abs(XPOS(wkpos) - x) <= 1)
				advance = shiftr1(advance);

			if (YPOS(bkpos) <= y && abs(XPOS(bkpos) - x) <= 1)
				advance += KING_PASSED_PAWN_SUPPORT;

			if (YPOS(wkpos) <= y && abs(XPOS(wkpos) - x) <= 1)
				advance = shiftr1(advance);

			if (b->board[piece->pos+SOUTH] > 0) {
				advance -= BLOCKED_PASSED_PAWN;
			}

			if (x != 0 && black_pawn_loc[x-1] &&
			    abs(y - black_pawn_loc[x-1]) <= 1) {
				advance += shiftr1(pawn_advance[n]);
			}

			if (x != 7 && black_pawn_loc[x+1] &&
			    abs(y - black_pawn_loc[x+1]) <= 1) {
				advance += shiftr1(pawn_advance[n]);
			}

			if ((b->topieces[piece->pos] & ~b->pinned_mask & WKROOK_MASK) &&
			    XPOS(WHITEPIECES(b)[IKROOK].pos) == x &&
			    YPOS(WHITEPIECES(b)[IKROOK].pos) > y) {
				/* there is a white king rook 
				   behind our passed pawn */
				advance -= PASSED_PAWN_ROOK_ATTACK;
			}

			if ((b->topieces[piece->pos] & ~b->pinned_mask & WQROOK_MASK) &&
			    XPOS(WHITEPIECES(b)[IQROOK].pos) == x &&
			    YPOS(WHITEPIECES(b)[IQROOK].pos) > y) {
				/* there is a white queen rook 
				   behind our passed pawn */
				advance -= PASSED_PAWN_ROOK_ATTACK;
			}


			if ((b->topieces[piece->pos] & ~b->pinned_mask & BKROOK_MASK) &&
			    XPOS(BLACKPIECES(b)[IKROOK].pos) == x &&
			    YPOS(BLACKPIECES(b)[IKROOK].pos) > y) {
				/* there is a black king rook 
				   behind our passed pawn */
				advance += PASSED_PAWN_ROOK_SUPPORT;
			}

			if ((b->topieces[piece->pos] & ~b->pinned_mask & BQROOK_MASK) &&
			    XPOS(BLACKPIECES(b)[IQROOK].pos) == x &&
			    YPOS(BLACKPIECES(b)[IQROOK].pos) > y) {
				/* there is a black queen rook 
				   behind our passed pawn */
				advance += PASSED_PAWN_ROOK_SUPPORT;
			}


			/* a special case - is it unstoppable? */
			if ((b->piece_mask & WHITE_MASK) == WKING_MASK) {
				int kpos = WHITEPIECES(b)[IKING].pos;
				int kmoves = imax(abs(XPOS(kpos) - x), YPOS(kpos));
				int pmoves = y;
				if (whites_move(b)) kmoves--;
				if (kmoves > pmoves) {
					/* its a candidate for unstopppability, but we need to
					   count all pieces that have to move out of the way,
					   except those on the queening square. */

					Square pos = piece->pos;

					while (!off_board(pos+SOUTH,SOUTH)) {
						pos += SOUTH;
						if (b->board[pos] != 0) {
							if (b->board[pos] == -PAWN)
								pmoves += 10;
							else 
								++pmoves;
							if (pmoves >= kmoves)
								break;
						}
					} 
					if (kmoves > pmoves) {
						ret += UNSTOPPABLE_PAWN;
						if (debug)
							lprintf(0,"unstoppable %s %d %d\n",
								posstr(piece->pos), 
								kmoves, pmoves);
						/* would it become stoppable after a null move? */
						if (blacks_move(b) && kmoves == pmoves+1) {
							b->null_stoppable_pawn |= (1<<pi);
							if (debug)
								lprintf(0,"null stoppable %s\n",
									posstr(piece->pos));
						}
						/* would it become unstoppable after a null move? */
					} else if (whites_move(b) && kmoves == pmoves) {
						b->null_unstoppable_pawn |= (1<<pi);
						if (debug)
							lprintf(0,"null unstoppable %s\n",
								posstr(piece->pos));
					}
				}
			}
			if (n < 4 && (ret + advance < pawn_advance[5])) {
				/* passed pawns always have some value */
				advance = pawn_advance[5] - ret;
			}
		}
		if (debug) 
			lprintf(0,"%s advance: %e\n", posstr(piece->pos), advance);
		ret += advance;		
	}

	if (black_pawn_loc[x] != y)
		ret -= DOUBLED_PAWN;

	if (x==0) {
		if (black_pawn_loc[x+1] == 0) ret -= ISOLATED_PAWN;
	} else if (x==7) {
		if (black_pawn_loc[x-1] == 0) ret -= ISOLATED_PAWN;
	} else if (black_pawn_loc[x-1] == 0 && black_pawn_loc[x+1] == 0) {
		ret -= ISOLATED_PAWN;
	}

#if 0
	if (b->stage == ENDING)
		if ((b->piece_mask & BBISHOP_MASK) == BKBISHOP_MASK) {
			if (white_square(piece->pos)) ret += ODD_BISHOPS_PAWN_POS;
		} else if ((b->piece_mask & BBISHOP_MASK) == BQBISHOP_MASK) {
			if (!white_square(piece->pos)) ret += ODD_BISHOPS_PAWN_POS;
		}
#endif	
	return ret;
}


/* these are adjustments for king position in the opening and
   ending - they try to encourage a central king in the ending */
static CONST_EVAL etype *ending_kpos = &coefficients[IENDING_KPOS];

/* the king likes to be near an edge at the start of the game and
   near the middle at the end */
static etype eval_white_king(Position *b, int pi)
{
	PieceStruct *piece = b->pieces + pi;
	etype ret=0;
	
	if (b->piece_mask & BQUEEN_MASK) {
		if (b->stage == OPENING) {
			if (YPOS(piece->pos) != 0)
				ret -= OPENING_KING_ADVANCE*YPOS(piece->pos);
		} if (b->stage == MIDDLE) {
			if (YPOS(piece->pos) != 0)
				ret -= MID_KING_ADVANCE*YPOS(piece->pos);
		}
	} else {
		ret += ending_kpos[XPOS(piece->pos)] +
			ending_kpos[YPOS(piece->pos)];
		if (b->stage == MATING) {
			if (b->w_material < b->b_material - PAWN_VALUE) {
				ret *= 10;
			} 
		}
	}

	if (b->stage == ENDING)
		return ret;

	if (b->flags & WHITE_CASTLED) {
		ret += CASTLE_BONUS;
	} else if (!(b->flags & (WHITE_CASTLE_SHORT|WHITE_CASTLE_LONG))) {
		ret -= CASTLE_BONUS;
	}

	/* forget about pawn defence once the queens are off! */
	if (!(b->material_mask & BQUEEN_MASK))
		return ret;

	if (YPOS(piece->pos) == 0 && 
	    (XPOS(piece->pos) > 4 || (b->flags & WHITE_CASTLE_SHORT))) {
		if (b->board[F2] == PAWN)
			ret += pawn_defence[FPAWN];
		if (b->board[G2] == PAWN)
			ret += pawn_defence[GPAWN];
		if (b->board[H2] == PAWN)
			ret += pawn_defence[HPAWN];
		if (b->board[H3] == PAWN && b->board[G2] == PAWN)
			ret += pawn_defence[HGPAWN];
		if (b->board[G3] == PAWN && b->board[H2] == PAWN &&
		    !(b->material_mask & BQBISHOP_MASK))
			ret += pawn_defence[GHPAWN];
		if (b->board[F3] == PAWN && b->board[G2] == PAWN &&
		    !(b->material_mask & BKBISHOP_MASK))
			ret += pawn_defence[FGPAWN];
	}

	if (YPOS(piece->pos) == 0 && XPOS(piece->pos) < 3) {
		if (b->board[A2] == PAWN)
			ret += pawn_defence[APAWN];
		if (b->board[B2] == PAWN)
			ret += pawn_defence[BPAWN];
		if (b->board[C2] == PAWN)
			ret += pawn_defence[CPAWN];
		if (b->board[A3] == PAWN && b->board[B2] == PAWN)
			ret += pawn_defence[ABPAWN];
		if (b->board[B3] == PAWN && b->board[A2] == PAWN &&
		    !(b->material_mask & BKBISHOP_MASK))
			ret += pawn_defence[BAPAWN];
		if (b->board[C3] == PAWN && b->board[B2] == PAWN &&
		    !(b->material_mask & BQBISHOP_MASK))
			ret += pawn_defence[CBPAWN];
	}

	return ret;
}


static etype eval_black_king(Position *b, int pi)
{
	PieceStruct *piece = b->pieces + pi;
	etype ret=0;
	
	if (b->piece_mask & WQUEEN_MASK) {
		if (b->stage == OPENING) {
			if (YPOS(piece->pos) != 7)
				ret -= OPENING_KING_ADVANCE*(7-YPOS(piece->pos));
		} if (b->stage == MIDDLE) {
			if (YPOS(piece->pos) != 7)
				ret -= MID_KING_ADVANCE*(7-YPOS(piece->pos));
		}
	} else {
		ret += ending_kpos[XPOS(piece->pos)] +
			ending_kpos[YPOS(piece->pos)];
		if (b->stage == MATING) {
			if (b->b_material < b->w_material - PAWN_VALUE) {
				ret *= 10;
			} 
		}
	}

	if (b->stage == ENDING)
		return ret;

	if (b->flags & BLACK_CASTLED) {
		ret += CASTLE_BONUS;
	} else if (!(b->flags & (BLACK_CASTLE_SHORT|BLACK_CASTLE_LONG))) {
		ret -= CASTLE_BONUS;
	} 

	/* forget about pawn defence once the queens are off! */
	if (!(b->material_mask & WQUEEN_MASK))
		return ret;

	if (YPOS(piece->pos) == 7 && 
	    (XPOS(piece->pos) > 4 || (b->flags & BLACK_CASTLE_SHORT))) {
		if (b->board[F7] == -PAWN)
			ret += pawn_defence[FPAWN];
		if (b->board[G7] == -PAWN)
			ret += pawn_defence[GPAWN];
		if (b->board[H7] == -PAWN)
			ret += pawn_defence[HPAWN];
		if (b->board[H6] == -PAWN && b->board[G7] == -PAWN)
			ret += pawn_defence[HGPAWN];
		if (b->board[G6] == -PAWN && b->board[H7] == -PAWN &&
		    !(b->material_mask & WQBISHOP_MASK))
			ret += pawn_defence[GHPAWN];
		if (b->board[F6] == -PAWN && b->board[G7] == -PAWN &&
		    !(b->material_mask & WKBISHOP_MASK))
			ret += pawn_defence[FGPAWN];
	}

	if (YPOS(piece->pos) == 7 && XPOS(piece->pos) < 3) {
		if (b->board[A7] == -PAWN)
			ret += pawn_defence[APAWN];
		if (b->board[B7] == -PAWN)
			ret += pawn_defence[BPAWN];
		if (b->board[C7] == -PAWN)
			ret += pawn_defence[CPAWN];
		if (b->board[A6] == -PAWN && b->board[B7] == -PAWN)
			ret += pawn_defence[ABPAWN];
		if (b->board[B6] == -PAWN && b->board[A7] == -PAWN &&
		    !(b->material_mask & WKBISHOP_MASK))
			ret += pawn_defence[BAPAWN];
		if (b->board[C6] == -PAWN && b->board[B7] == -PAWN &&
		    !(b->material_mask & WQBISHOP_MASK))
			ret += pawn_defence[CBPAWN];
	}

	return ret;
}



static etype eval_queen(Position *b, int pi)
{
	PieceStruct *p = b->pieces + pi;
	etype ret=0;
	
	/* penalize early queen movement */
	if (b->stage == OPENING) {
		if ((p->p > 0 && YPOS(p->pos) > 2) || 
		    (p->p < 0 && YPOS(p->pos) < 5))
			ret -= EARLY_QUEEN_MOVEMENT;
	}
	
	return ret;
}

static etype eval_rook(Position *b, int pi)
{
	static CONST_EVAL etype *pos_value = &coefficients[IROOK_POS];
	PieceStruct *p = b->pieces + pi;
	etype ret=0;

	if (p->p > 0)
		ret += pos_value[p->pos];
	else
		ret += pos_value[mirror_square(p->pos)];

	if (p->p > 0) {
		if (b->topieces[p->pos] & ~b->pinned_mask & WROOK_MASK)
			ret += CONNECTED_ROOKS;
	} else {
		if (b->topieces[p->pos] & ~b->pinned_mask & BROOK_MASK)
			ret += CONNECTED_ROOKS;
	}

	return ret;
}


static etype eval_bishop_xray(Position *b, int pi)
{
	PieceStruct *p = b->pieces + pi;
	etype ret=0;
	int xray=0;
	PieceStruct *p2;
	static CONST_EVAL etype *xray_bonus = &coefficients[IBISHOP_XRAY];

	/* bishops get a bonus for Xray attacks on kings, queens or rooks */
	p2 = &PIECES(b, -p->p)[IKING];
	if (capture_map[p->p+KING][p->pos][p2->pos])
		xray++;

	p2 = &PIECES(b, -p->p)[IQUEEN];
	if (p2->p && capture_map[p->p+KING][p->pos][p2->pos])
		xray++;

	p2 = &PIECES(b, -p->p)[IQROOK];
	if (p2->p && capture_map[p->p+KING][p->pos][p2->pos])
		xray++;

	p2 = &PIECES(b, -p->p)[IKROOK];
	if (p2->p && capture_map[p->p+KING][p->pos][p2->pos])
		xray++;

	ret += xray_bonus[xray];

	return ret;
}

static etype eval_bishop(Position *b, int pi)
{
	etype ret=0;
	PieceStruct *p = b->pieces + pi;
	int x = XPOS(p->pos);
	int y = YPOS(p->pos);

	/* bishops are good on outposts */
	if (p->p > 0) {
		if (y < 6 && y > 3 &&
		    (x == 0 || black_pawn_loc[x-1] <= y) &&
		    (x == 7 || black_pawn_loc[x+1] <= y) && 
		    ((x>0 && b->board[p->pos+SOUTH_WEST] == PAWN) ||
		     (x<7 && b->board[p->pos+SOUTH_EAST] == PAWN))) {
			ret += BISHOP_OUTPOST;
		}
	} else {
		if (y > 1 && y < 4 &&
		    (x == 0 || white_pawn_loc[x-1] == 0 ||
		     white_pawn_loc[x-1] >= y) &&
		    (x == 7 || white_pawn_loc[x+1] == 0 ||
		     white_pawn_loc[x+1] >= y) &&
		    ((x>0 && b->board[p->pos+NORTH_WEST] == -PAWN) ||
		     (x<7 && b->board[p->pos+NORTH_EAST] == -PAWN))) {
			ret += BISHOP_OUTPOST;
		}
	}

#if 1
	ret += eval_bishop_xray(b, pi);
#endif
	return ret;
}


static etype eval_knight(Position *b, int pi)
{
	PieceStruct *p = b->pieces + pi;
	etype ret=0;
	int x = XPOS(p->pos);
	int y = YPOS(p->pos);

	static CONST_EVAL etype *pos_value = &coefficients[IKNIGHT_POS];

	if (p->p > 0)
		ret += pos_value[p->pos];
	else
		ret += pos_value[mirror_square(p->pos)];

	/* knights are great on outposts */
	if (p->p > 0) {
		if (y < 6 && y > 3 &&
		    (x == 0 || black_pawn_loc[x-1] <= y) &&
		    (x == 7 || black_pawn_loc[x+1] <= y) &&
		    ((x>0 && b->board[p->pos+SOUTH_WEST] == PAWN) ||
		     (x<7 && b->board[p->pos+SOUTH_EAST] == PAWN))) {
			ret += KNIGHT_OUTPOST;
		}
	} else {
		if (y > 1 && y < 4 &&
		    (x == 0 || white_pawn_loc[x-1] == 0 ||
		     white_pawn_loc[x-1] >= y) &&
		    (x == 7 || white_pawn_loc[x+1] == 0 ||
		     white_pawn_loc[x+1] >= y) && 
		    ((x>0 && b->board[p->pos+NORTH_WEST] == -PAWN) ||
		     (x<7 && b->board[p->pos+NORTH_EAST] == -PAWN))) {
			ret += KNIGHT_OUTPOST;
		}
	}

	return ret;
}


/* build a table of the most backward pawns in each file for each color */
static void build_pawn_loc(Position *b)
{
	int i;
	PieceStruct *p;
	
	memset(white_pawn_loc, 0, sizeof(white_pawn_loc));
	memset(black_pawn_loc, 0, sizeof(black_pawn_loc));

	p = &WHITEPIECES(b)[8];
	for (i=0;i<8;i++, p++) 
		if (p->p == PAWN) {
			int x = XPOS(p->pos);
			int y = YPOS(p->pos);
			if (white_pawn_loc[x] == 0 ||
			    white_pawn_loc[x] > y)
				white_pawn_loc[x] = y;
		}

	p = &BLACKPIECES(b)[8];
	for (i=0;i<8;i++, p++) 
		if (p->p == -PAWN) {
			int x = XPOS(p->pos);
			int y = YPOS(p->pos);
			black_pawn_loc[x] = imax(black_pawn_loc[x], y);
		}
}


void estimate_game_stage(Position *b, int root_node)
{
	int count;
	int last_stage = b->stage;

	count = pop_count[b->piece_mask & 0xFF] + 
		pop_count[(b->piece_mask >> 16) & 0xFF];

	if ((b->piece_mask & QUEEN_MASK) == QUEEN_MASK)
		count += 2;

	if ((root_node || last_stage == MATING) &&
	    b->piece_mask == b->material_mask) {
		b->stage = MATING;
	} else if (count <= 6) {
		b->stage = ENDING;
	} else if (count >= 16 && (b->flags & FLAG_CAN_CASTLE)) {
		b->stage = OPENING;
	} else {
		b->stage = MIDDLE;
	}

	if (last_stage != b->stage) {
		b->piece_change |= KING_MASK;
		b->piece_change |= (QUEEN_MASK & b->piece_mask);
	}

#if 0
	if (root_node && b->stage == MATING && last_stage != MATING) {
		hash_reset();
	}
#endif
}


/* try to discourage specific positional features - particularly in
   the opening */
static etype specifics(Position *b)
{
	etype ret = 0;

	/* blocking the e or d pawn is a bad idea */
	if (b->board[D3] && b->board[D2] == PAWN)
		ret -= BLOCKED_DPAWN;

	if (b->board[E3] && b->board[E2] == PAWN)
		ret -= BLOCKED_EPAWN;

	if (b->board[D6] && b->board[D7] == -PAWN)
		ret -= -BLOCKED_DPAWN;

	if (b->board[E6] && b->board[E7] == -PAWN)
		ret -= -BLOCKED_EPAWN;


	/* blocking in knights is a bad idea */
	if (b->board[C3] && b->board[B1] == KNIGHT)
		ret -= BLOCKED_KNIGHT;

	if (b->board[F3] && b->board[G1] == KNIGHT)
		ret -= BLOCKED_KNIGHT;

	if (b->board[C6] && b->board[B8] == -KNIGHT)
		ret -= -BLOCKED_KNIGHT;

	if (b->board[F6] && b->board[G8] == -KNIGHT)
		ret -= -BLOCKED_KNIGHT;

	/* pairs of bishops are good */
	if ((b->piece_mask & WBISHOP_MASK) == WBISHOP_MASK)
		ret += BISHOP_PAIR;

	if ((b->piece_mask & BBISHOP_MASK) == BBISHOP_MASK)
		ret -= BISHOP_PAIR;

	/* opposite bishops is drawish */
	if ((b->piece_mask & BISHOP_MASK) == (WKBISHOP_MASK | BKBISHOP_MASK) ||
	    (b->piece_mask & BISHOP_MASK) == (WQBISHOP_MASK | BQBISHOP_MASK)) {
		if (b->b_material > b->w_material + PAWN_VALUE/2)
			ret += OPPOSITE_BISHOPS;
		else if (b->w_material > b->b_material + PAWN_VALUE/2)
			ret -= OPPOSITE_BISHOPS;		
	}

	/* these are some specialist endgame things */
	if (b->w_material < b->b_material - PAWN_VALUE &&
	    b->stage == MATING) {
		int kpos = WHITEPIECES(b)[IKING].pos;

		ret -= NO_MATERIAL;

		if ((b->piece_mask & BBISHOP_MASK) == BKBISHOP_MASK) {
			ret -= (7-corner_distance(kpos)) * MATING_POSITION;
		}
		if ((b->piece_mask & BBISHOP_MASK) == BQBISHOP_MASK) {
			ret -= (7-corner_distance(mirror_square(kpos))) * MATING_POSITION;
		}
		ret += distance(kpos, BLACKPIECES(b)[IKING].pos) * MATING_POSITION;		
	}

	if (b->b_material < b->w_material - PAWN_VALUE &&
	    b->stage == MATING) {
		int kpos = BLACKPIECES(b)[IKING].pos;

		ret += NO_MATERIAL;

		if ((b->piece_mask & WBISHOP_MASK) == WQBISHOP_MASK) {
			ret += (7-corner_distance(kpos)) * MATING_POSITION;
		}
		if ((b->piece_mask & WBISHOP_MASK) == WKBISHOP_MASK) {
			ret += (7-corner_distance(mirror_square(kpos))) * MATING_POSITION;
		}
		ret -= distance(kpos, WHITEPIECES(b)[IKING].pos) * MATING_POSITION;		
	}
	

	return ret;
}


static CONST_EVAL etype *base_pos_value = &coefficients[IPOS_BASE];

static etype null_pos_value[NUM_SQUARES];


static CONST_EVAL etype *white_king_side = &coefficients[IPOS_KINGSIDE];
static CONST_EVAL etype *white_queen_side = &coefficients[IPOS_QUEENSIDE];

static etype black_king_side[NUM_SQUARES];
static etype black_queen_side[NUM_SQUARES];

static int new_pos_values;
static CONST_EVAL etype *sq_p1, *sq_p2;

static void recalc_pos_values(Position *b)
{
	Square wkpos, bkpos;
	int flags = 0;

	wkpos = WHITEPIECES(b)[IKING].pos;
	bkpos = BLACKPIECES(b)[IKING].pos;

	sq_p1 = null_pos_value;
	sq_p2 = null_pos_value;

	if (b->material_mask & QUEEN_MASK) {
		if (XPOS(wkpos) > 3 && YPOS(wkpos) < 3) {
			sq_p1 = white_king_side;
			flags |= FLAG_WHITE_KINGSIDE;
		} else if (XPOS(wkpos) < 3 && YPOS(wkpos) < 3) {
			sq_p1 = white_queen_side;
			flags |= FLAG_WHITE_QUEENSIDE;
		}

		if (XPOS(bkpos) > 3 && YPOS(bkpos) > 4) {
			sq_p2 = black_king_side;
			flags |= FLAG_BLACK_KINGSIDE;
		} else if (XPOS(bkpos) < 3 && YPOS(bkpos) > 4) {
			sq_p2 = black_queen_side;
			flags |= FLAG_BLACK_QUEENSIDE;
		}
	}

	if ((b->flags & FLAG_KQ_SIDE) == flags)
		return;

	invert(black_king_side, white_king_side);
	invert(black_queen_side, white_queen_side);

	new_pos_values = 1;

	b->flags &= ~FLAG_KQ_SIDE;
	b->flags |= flags;
}


/* this basically tries to answer the question "can one of the players
   profitably capture on that square, assuming there was something there
   to capture */
static int get_control(Position *b, uint32 topieces,
		       int piece, Square sq) 
{
	int nw, nb;
	/* the following masks select pieces that are of lower than
	   value than a piece */
	static CONST_EVAL uint32 masks[2*KING+1] = 
	{0xFFFE, 0xFFFC, 0xFFF0, 0xFF00, 0xFF00, 0, 
	 0, 
	 0, 0xFF000000, 0xFF000000, 0xFFF00000, 0xFFFC0000, 0xFFFE0000};

	if (!topieces) return 0;

	if (topieces & masks[piece+KING]) 
		return -piece;

	/* compare pawn counts */
	nw = pop_count[(topieces >> 8) & 0xFF];
	nb = pop_count[(topieces >> 24) & 0xFF];
	if (nw - nb) return nw - nb;

	/* compare piece counts */
	nw = pop_count[topieces & 0xFF];
	nb = pop_count[(topieces >> 16) & 0xFF];

#if USE_SLIDING_CONTROL
	nw += pop_count[b->xray_topieces[sq] & 0xFF];
	nb += pop_count[(b->xray_topieces[sq] >> 16) & 0xFF];
#endif
	return (nw - nb);
}

static etype board_control1(Position *b)
{
	int i, j;
	int control;
	etype total, v;
	uint32 topieces, oldflight, fchange, mchange, old_pin_mask, pin_mask;
	uint32 oldtopieces, xray_topieces, oldxray_topieces;
	uint32 *top, *oldtop, *xray_top, *oldxray_top, *flight;
	Piece *oldboard;

	/* this is a mask which governs what piece can fly to a square
	   given the smallest attacker on the square, so for example, the 
	   0x00FC0000 entry means that any black piece except a queen
	   or king can fly to a square that is controlled by black if
	   the square is covered by a white rook */
	static CONST_EVAL uint32 flight_mask[32] = {
		0x00FE0000, 0x00FE0000, 0x00FC0000, 0x00FC0000, 
		0x00F00000, 0x00F00000, 0x00F00000, 0x00F00000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,

		0x00FE, 0x00FE, 0x00FC, 0x00FC, 
		0x00F0, 0x00F0, 0x00F0, 0x00F0,
		0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000
	};

#if 0
	if (b->stage == MATING)
		return 0;
#endif
	top = b->topieces;
#if USE_SLIDING_CONTROL
	xray_top = b->xray_topieces;
#endif
	flight = b->flight;

	recalc_pos_values(b);

	if (b->oldb && (b->oldb->flags & FLAG_EVAL_DONE) && !new_pos_values) {
		oldtop = b->oldb->topieces;
#if USE_SLIDING_CONTROL
		oldxray_top = b->oldb->xray_topieces;
#endif
		oldboard = b->oldb->board;
		old_pin_mask = ~b->oldb->pinned_mask;
		memcpy(b->mobility, b->oldb->mobility, sizeof(b->mobility));
	} else {
		static uint32 dummy[64];
		static Piece dummy2[64];
#if USE_SLIDING_CONTROL
		static uint32 xray_dummy[64];
		oldxray_top = xray_dummy;
#endif
		oldtop = dummy;
		oldboard = dummy2;
		b->board_control = 0;
		b->attacking_mask = 0;
		old_pin_mask = ~0;
		memset(b->control, 0, sizeof(b->control));
		memset(b->flight, 0, sizeof(b->flight));
		memset(b->mobility, 0, sizeof(b->mobility));
		memset(b->safe_mobility, 0, sizeof(b->safe_mobility));
		b->white_moves = b->black_moves = 0;
		new_pos_values = 0;
	}

	pin_mask = ~b->pinned_mask;

	total = b->board_control;

	for (i=A1;i<=H8;i++) {
		topieces = top[i] & pin_mask;
		oldtopieces = oldtop[i] & old_pin_mask;
#if USE_SLIDING_CONTROL
		xray_topieces = xray_top[i];
		oldxray_topieces = oldxray_top[i];
#endif
		if (topieces == oldtopieces && b->board[i] == oldboard[i]
#if USE_SLIDING_CONTROL
		    && xray_topieces == oldxray_topieces
#endif
		    ) {
			continue;
		}

		/* if there is something on the square then recalc that piece
		   as well */
		if (b->board[i])
			b->piece_change |= (1<<b->pboard[i]);

		/* something has changed on the square - we need to recalculate
		   everything */

		oldflight = flight[i];

		/* we define two control values, one taking into 
		   account whites pins,
		   and the other taking into account blacks pins */
		control = get_control(b, topieces, b->board[i], i);
#if 0
		if (debug)
			lprintf(0,"control %s %d\n", posstr(i), control);
#endif
		/* now the trapped pieces stuff. we need to work out what 
		   pieces can flee to this square 

		   a square can be used as a flight square only if the 
		   piece would not be hung when it moves there. 

		   we use the flight mask to shortcut this calculation
		   */
		if (control > 0 && b->board[i] <= 0) {
			if (topieces & BLACK_MASK) {
				flight[i] = flight_mask[fl_one(topieces&BLACK_MASK)] & topieces & pin_mask;
			} else {
				flight[i] = WHITE_MASK & topieces & pin_mask;
			}
		} else if (control < 0 && b->board[i] >= 0) {
			if (topieces & WHITE_MASK) {
				flight[i] = flight_mask[fl_one(topieces&WHITE_MASK)] & topieces & pin_mask;
			} else {
				flight[i] = BLACK_MASK & topieces & pin_mask;
			}
		} else {
			flight[i] = 0;
		}

		flight[i] &= b->piece_mask;

		/* this xor tells us what pieces have changed in their
		   safe mobility */
		fchange = (oldflight ^ flight[i]) & b->piece_mask;

		while (fchange & WHITE_MASK) {
			j = ff_one(fchange & WHITE_MASK);
			fchange &= ~(1<<j);
			if (flight[i] & (1<<j)) {
				b->safe_mobility[j]++;
				b->white_moves++;
			} else {
				b->safe_mobility[j]--;
				b->white_moves--;
			}
		}

		while (fchange) {
			j = ff_one(fchange);
			fchange &= ~(1<<j);
			if (flight[i] & (1<<j)) {
				b->safe_mobility[j]++;
				b->black_moves++;
			} else {
				b->safe_mobility[j]--;
				b->black_moves--;
			}
		}

		/* this xor tells us what pieces have changed in their
		   mobility */
		mchange = (oldtopieces ^ topieces) & b->piece_mask;

		while (mchange & WHITE_MASK) {
			j = ff_one(mchange & WHITE_MASK);
			mchange &= ~(1<<j);
			if (topieces & (1<<j)) {
				b->mobility[j]++;
			} else {
				b->mobility[j]--;
			}
		}

		while (mchange) {
			j = ff_one(mchange);
			mchange &= ~(1<<j);
			if (topieces & (1<<j)) {
				b->mobility[j]++;
			} else {
				b->mobility[j]--;
			}
		}

		/* the board control itself */
		if (control > 0) {
			v = base_pos_value[i] + sq_p1[i];
		} else if (control < 0) {
			v = -(base_pos_value[mirror_square(i)] + sq_p2[i]);
		} else {
			v = 0;
		}

		total += v - b->control[i];
		b->control[i] = v;
	}

	b->board_control = total;

	return total;
}


static etype board_control2(Position *b)
{
	Square i, j;
	etype total;
	uint32 hung, attacking;

	total = 0;
	hung = 0;
	attacking = 0;

	if (b->stage == MATING)
		return 0;

	for (i=1;i<16;i++) {
		if (!b->pieces[i].p) continue;
		j = b->pieces[i].pos;

		if (b->topieces[j] & BLACK_MASK) {
			attacking |= b->topieces[j] & BLACK_MASK;
			total -= pop_count[(b->topieces[j]>>16) & 0xFF];
			if (b->control[j] <= 0) {
				if (b->control[j] < 0) {
					hung |= (1<<i);
				} else {
				/* if it is even then the white pieces
                                   are stuck protecing the piece,
                                   their mobility is lowered */
					total -= pop_count[b->topieces[j] & 0xFF] * OVERPROTECTION;
				}
			}
		}
	}

	for (i=17;i<32;i++) {
		if (!b->pieces[i].p) continue;
		j = b->pieces[i].pos;

		if (b->topieces[j] & WHITE_MASK) {
			attacking |= b->topieces[j] & WHITE_MASK;
			total += pop_count[b->topieces[j] & 0xFF];
			if (b->control[j] >= 0) {
				if (b->control[j] > 0) {
					hung |= (1<<i);
				} else {
				/* if it is even then the black pieces
                                   are stuck protecing the piece,
                                   their mobility is lowered */
					total += pop_count[(b->topieces[j]>>16) & 0xFF] * OVERPROTECTION;
				}
			}
		}
	}

	total *= ATTACK_VALUE;

	attacking &= b->piece_mask;
	b->piece_change |= b->attacking_mask ^ attacking; 
	b->hung_mask = hung;
	b->attacking_mask = attacking;

	if (debug) {
		lprintf(0,"attacking_mask=%08x\n", b->attacking_mask);
		lprintf(0,"hung_mask=%08x\n", b->hung_mask);
	}

	return total;
}


static int empty_line(Position *b, int sq1, int sq2)
{
	int dir, dx, dy;

	dx = XPOS(sq1) - XPOS(sq2);
	dy = YPOS(sq1) - YPOS(sq2);

	dir = 0;
	if (dy > 0) 
		dir += SOUTH;
	else if (dy < 0) 
		dir += NORTH;

	if (dx > 0) 
		dir += WEST;
	else if (dx < 0) 
		dir += EAST;

	sq1 += dir;

	while (sq1 != sq2) {
		if (b->board[sq1]) return 0;
		sq1 += dir;
	}

	return 1;
}

static etype mobility_fn(Position *b, int pi)
{
	static CONST_EVAL int mobility_threshold[KING+1] = {0, 0, 6, 5, 4, 6, 0};
	int mobility;
	etype ret=0;
	Piece p = abs(b->pieces[pi].p);

	etype CONST_EVAL *mobility_table[KING+1] = {NULL, NULL, NULL, 
						 &coefficients[IBISHOP_MOBILITY],
						 &coefficients[IROOK_MOBILITY],
						 &coefficients[IQUEEN_MOBILITY],
						 &coefficients[IKING_MOBILITY]};

	CONST_EVAL etype *safe_mobility_table[KING+1] = 
	{NULL, NULL, &coefficients[IKNIGHT_SMOBILITY],
		 &coefficients[IBISHOP_SMOBILITY],
		 &coefficients[IROOK_SMOBILITY],
		 &coefficients[IQUEEN_SMOBILITY],
		 &coefficients[IKING_SMOBILITY]};

	mobility = b->mobility[pi];
	if (mobility >= 10) {
		mobility = 9;
	}

	if (mobility_table[p]) {
		ret = mobility_table[p][mobility];
	}

	if (b->mobility[pi] < mobility_threshold[p] &&
	    !(b->attacking_mask & (1<<pi))) {
		if (debug)
			lprintf(0,"useless %s\n", 
				posstr(b->pieces[pi].pos));
		ret -= USELESS_PIECE;
	}

	mobility = b->safe_mobility[pi];
	if (mobility >= 10) {
		mobility = 9;
	}

	if (safe_mobility_table[p])
		ret += safe_mobility_table[p][mobility];

	if (b->safe_mobility[pi] == 0) {
		if (b->pieces[pi].p > 0) {
			ret -= TRAPPED_STEP * YPOS(b->pieces[pi].pos);
		} else {
			ret -= TRAPPED_STEP * (7-YPOS(b->pieces[pi].pos));
		}
	}

	return ret;
}

static etype piece_values(Position *b)
{
	etype v;
	etype total;
	int i;
	uint32 piece_change;

	piece_change = b->piece_change;

	total = 0;

	for (i=0;i<32;i++) {
		if (!b->pieces[i].p) continue;
		if (!(piece_change & (1<<i)) && b->oldb) {
			total += b->piece_values[i];
		} else {
			switch (b->pieces[i].p) {
			case PAWN:
				v = eval_white_pawn(b, i);
				break;
			case KNIGHT:
				v = eval_knight(b, i);
				break;
			case BISHOP:
				v = eval_bishop(b, i);
				break;
			case ROOK:
				v = eval_rook(b, i);
				break;
			case QUEEN:
				v = eval_queen(b, i);
				break;
			case KING:
				v = eval_white_king(b, i);
				break;
			case -PAWN:
				v = -eval_black_pawn(b, i);
				break;
			case -KNIGHT:
				v = -eval_knight(b, i);
				break;
			case -BISHOP:
				v = -eval_bishop(b, i);
				break;
			case -ROOK:
				v = -eval_rook(b, i);
				break;
			case -QUEEN:
				v = -eval_queen(b, i);
				break;
			case -KING:
				v = -eval_black_king(b, i);
				break;
			default:
				v = 0;
				lprintf(0,"no piece? i=%d pos=%s p=%d\n",
					i, posstr(b->pieces[i].pos),
					b->pieces[i].p);
#if 0
				beep(20);
				sleep(120);
#endif
				break;
			}

			total += v;
			b->piece_values[i] = v;
		}
	}

	if (debug) {
		lprintf(0,"white pvalues:  \n");
		for (i=0;i<16;i++)
			if (b->pieces[i].p)
				lprintf(0,"%+3d ", b->piece_values[i]);
			else
				lprintf(0,"*** ");
		lprintf(0,"\n");
		lprintf(0,"black pvalues: \n");
		for (i=16;i<32;i++)
			if (b->pieces[i].p)
				lprintf(0,"%+3d ", -b->piece_values[i]);
			else
				lprintf(0,"*** ");
		lprintf(0,"\n");
	}
			
	b->piece_change = 0;

	return total;
}


static etype eval_mobility(Position *b)
{
	int i;
	etype total;

	total = 0;

	if (b->stage == MATING)
		return 0;

	for (i=0;i<16;i++) {
		if (!(b->piece_mask & (1<<i))) continue;
		total += mobility_fn(b, i);
	}

	for (i=16;i<32;i++) {
		if (!(b->piece_mask & (1<<i))) continue;
		total -= mobility_fn(b, i);
	}

	if (debug) {
		lprintf(0,"white safe mobility:  \n");
		for (i=0;i<16;i++)
			if (b->pieces[i].p)
				lprintf(0,"%+3d ", b->safe_mobility[i]);
			else
				lprintf(0,"*** ");
		lprintf(0,"\n");
		lprintf(0,"black safe mobility: \n");
		for (i=16;i<32;i++)
			if (b->pieces[i].p)
				lprintf(0,"%+3d ", b->safe_mobility[i]);
			else
				lprintf(0,"*** ");
		lprintf(0,"\n");
		lprintf(0,"white mobility:  \n");
		for (i=0;i<16;i++)
			if (b->pieces[i].p)
				lprintf(0,"%+3d ", b->mobility[i]);
			else
				lprintf(0,"*** ");
		lprintf(0,"\n");
		lprintf(0,"black mobility: \n");
		for (i=16;i<32;i++)
			if (b->pieces[i].p)
				lprintf(0,"%+3d ", b->mobility[i]);
			else
				lprintf(0,"*** ");
		lprintf(0,"\n");
	}

	return total;
}


/* this is called only when there are no pawns on the board.
   eval_tactics must already have been called */
static etype check_material(Position *b, etype ret0)
{
	etype ret = ret0;

	if (b->material_mask == b->piece_mask && b->tactics == 0) {
		/* queen and king vs queen and king is probably a draw */
		if (b->w_material == KING_VALUE+QUEEN_VALUE && 
		    b->b_material == KING_VALUE+QUEEN_VALUE) {
			ret = draw_value(b) * 2;
			return ret - ret0;
		}

		/* king and rook vs king and rook is almost always a draw */
		if (b->w_material == KING_VALUE+ROOK_VALUE && 
		    b->b_material == KING_VALUE+ROOK_VALUE) {
			ret = draw_value(b) * 2;
			return ret - ret0;
		}
		/* rook vs knight is almost always a draw */
		if ((b->w_material==KING_VALUE+ROOK_VALUE && 
		     b->b_material==KING_VALUE+KNIGHT_VALUE) ||
		    (b->b_material==KING_VALUE+ROOK_VALUE && 
		     b->w_material==KING_VALUE+KNIGHT_VALUE)) {
			ret = draw_value(b) * 2;
			return ret - ret0;
		}
		
		/* rook vs bishop is almost always a draw */
		if ((b->w_material==KING_VALUE+ROOK_VALUE && 
		     b->b_material==KING_VALUE+BISHOP_VALUE) ||
		    (b->b_material==KING_VALUE+ROOK_VALUE && 
		     b->w_material==KING_VALUE+BISHOP_VALUE)) {
			ret = draw_value(b) * 2;
			return ret - ret0;
		}
	}

	if ((b->material_mask & WHITE_MASK) == (b->piece_mask & WHITE_MASK) && 
	    (b->w_material <= emax(KING_VALUE+BISHOP_VALUE, KING_VALUE+KNIGHT_VALUE) ||
	     b->w_material == KING_VALUE+2*KNIGHT_VALUE)) {
		/* white has insufficient material to mate */
		ret = emin(ret, 0);
	}

	if ((b->material_mask & BLACK_MASK) == (b->piece_mask & BLACK_MASK) && 
	    (b->b_material <= emax(KING_VALUE+BISHOP_VALUE, KING_VALUE+KNIGHT_VALUE) ||
	     b->b_material == KING_VALUE+2*KNIGHT_VALUE)) {
		/* black has insufficient material to mate */
		ret = emax(ret, 0);
	}

	if (ret == 0 && ret0 != 0) {
		b->flags |= FLAG_ACCEPT_DRAW;
		if (debug)
			lprintf(0,"will accept draw\n");
	} else {
		b->flags &= ~FLAG_ACCEPT_DRAW;
	}

#if 0
	if (ret == 0)
		ret = draw_value(b);
#endif
	/* push it nearer a draw as the fifty move mark approaches */
	if (b->stage != MATING && ret != 0 && b->fifty_count > 20) {
		ret = ((128 - (b->fifty_count-20)) * ret) / 128;
	}

	if (ret == 0 && ret0 != 0) {
		ret = draw_value(b);
	}

	if (b->w_material >= b->b_material + (0.8*PAWN_VALUE) &&
	    pop_count16(b->material_mask & WPAWN_MASK) > 
	    pop_count16((b->material_mask & BPAWN_MASK)>>16)) {
		/* encourage piece trading but not pawn trading */
		ret += (16-pop_count32(b->piece_mask)) * TRADE_BONUS;
		ret -= (16-pop_count32(b->material_mask & ~b->piece_mask)) * TRADE_BONUS;
	}

	if (b->b_material >= b->w_material + (0.8*PAWN_VALUE) &&
	    pop_count16(b->material_mask & WPAWN_MASK) < 
	    pop_count16((b->material_mask & BPAWN_MASK)>>16)) {
		/* encourage piece trading but not pawn trading */
		ret -= (16-pop_count32(b->piece_mask)) * TRADE_BONUS;
		ret += (16-pop_count32(b->material_mask & ~b->piece_mask)) * TRADE_BONUS;
	}

	return ret - ret0;
}

static etype white_king_safety(Position *b)
{
	int ret = 0;
	Square x1, x2, y1, y2, kpos, x, y, pos;
	/* if the opponents queen is off the board then totally ignore
	   king safety. This is not too bad because with the queens
	   off king side attacks tend to be slower in execution, so they
	   can be diverted with search alone */
	if (!(b->piece_mask & BQUEEN_MASK)) return 0;

	/* the opponents queen is on the board, we need to worry about
	   king safety. The major components is "king attack", which
	   is proportional to the number of attacks made by enemy
	   pieces on the king. To find this we need to loop over the squares
	   around the king */
	
	kpos = WHITEPIECES(b)[IKING].pos;

	if (XPOS(kpos) == 0) {
		x1 = 0; x2 = 1;
	} else if (XPOS(kpos) == 7) {
		x1 = 6; x2 = 7;
	} else {
		x1 = XPOS(kpos)-1; x2 = x1+2;		
	}

	if (YPOS(kpos) == 0) {
		y1 = 0; y2 = 1;
	} else if (YPOS(kpos) == 7) {
		y1 = 6; y2 = 7;
	} else {
		y1 = YPOS(kpos)-1; y2 = y1+2;		
	}

	for (x=x1;x<=x2;x++)
		for (y=y1;y<=y2;y++) {
			pos = POSN(x, y);
			ret -= pop_count16((b->topieces[pos] >> 16) & 0xFFFF);
#if USE_SLIDING_CONTROL
			ret -= pop_count[(b->xray_topieces[pos] >> 16) & 0xFF];
#endif
		}

	if (b->flags & FLAG_COMPUTER_WHITE)
		ret *= KING_ATTACK_COMPUTER;
	else
		ret *= KING_ATTACK_OPPONENT;

	return ret;
}

static etype black_king_safety(Position *b)
{
	int ret = 0;
	Square x1, x2, y1, y2, kpos, x, y, pos;
	/* if the opponents queen is off the board then totally ignore
	   king safety. This is not too bad because with the queens
	   off king side attacks tend to be slower in execution, so they
	   can be diverted with search alone */
	if (!(b->piece_mask & WQUEEN_MASK)) return 0;

	/* the opponents queen is on the board, we need to worry about
	   king safety. The major components is "king attack", which
	   is proportional to the number of attacks made by enemy
	   pieces on the king. To find this we need to loop over the squares
	   around the king */
	
	kpos = BLACKPIECES(b)[IKING].pos;

	if (XPOS(kpos) == 0) {
		x1 = 0; x2 = 1;
	} else if (XPOS(kpos) == 7) {
		x1 = 6; x2 = 7;
	} else {
		x1 = XPOS(kpos)-1; x2 = x1+2;		
	}

	if (YPOS(kpos) == 0) {
		y1 = 0; y2 = 1;
	} else if (YPOS(kpos) == 7) {
		y1 = 6; y2 = 7;
	} else {
		y1 = YPOS(kpos)-1; y2 = y1+2;		
	}

	for (x=x1;x<=x2;x++)
		for (y=y1;y<=y2;y++) {
			pos = POSN(x, y);
			ret -= pop_count16(b->topieces[pos] & 0xFFFF);
#if USE_SLIDING_CONTROL
			ret -= pop_count[b->xray_topieces[pos] & 0xFF];
#endif
		}

	if (b->flags & FLAG_COMPUTER_WHITE)
		ret *= KING_ATTACK_OPPONENT;
	else
		ret *= KING_ATTACK_COMPUTER;

	return ret;
}

static etype king_safety(Position *b)
{
	return white_king_safety(b) - black_king_safety(b);
}

etype draw_value(Position *b)
{
	etype ret = 0;

	if (b->stage != ENDING) {
		if (state->computer > 0)
			ret += 0.05*PAWN_VALUE*state->rating_change;
		else
			ret -= 0.05*PAWN_VALUE*state->rating_change;
	} else {
		if (state->computer > 0)
			ret += DRAW_VALUE;
		else 
			ret -= DRAW_VALUE;
	}
	
	if (ABS(ret) > PAWN_VALUE)
		ret = SIGN(ret)*PAWN_VALUE;

	return ret;
}


static etype find_pins(Position *b)
{
	int i, j, k;
	uint32 pinner_mask;

	/* to find pins we loop over all pieces, looking for pieces
	   that are under attack by sliding pieces (but not if they
	   themselves are sliding pieces of the same type!). Then we
	   check to see if the sliding piece would attack another
	   piece, making it hung, if moved.

	   This is a expensive procedure!  */

	b->pinned_mask = 0;

	if (b->stage == MATING)
		return 0;

	for (i=2;i<16;i++) {
		if (!b->pieces[i].p) continue;

		pinner_mask = b->topieces[b->pieces[i].pos] & 
			b->sliding_mask & BLACK_MASK;

		while (pinner_mask) {
			j = ff_one(pinner_mask);
			pinner_mask &= ~(1<<j);

			/* If the pinned piece is attacking the pinner then 
			   it isn't a pin! It might be a skewer tho.
			   */
			if (b->topieces[b->pieces[j].pos] & (1<<i))
				continue;

			/* look for a piece that this bit is pinned against */
			for (k=0;k<8;k++) {
				if (!b->pieces[k].p) continue;
				if (k == i) continue;
				if (!same_line(b->pieces[j].pos, b->pieces[i].pos, 
					       b->pieces[k].pos)) continue;
				if (!empty_line(b, b->pieces[i].pos,
						b->pieces[k].pos))
				    continue;
				
				/* we have a likely pin. Now we need
				   to confirm that if the pinner could attack
				   the pinnedto piece then that piece
				   would be hung */
				if (get_control(b, 
						b->topieces[b->pieces[k].pos] | (1<<j), 
						b->pieces[k].p,
					        b->pieces[k].pos) < 0) {
					b->pinned_mask |= (1<<i);
					if (debug)
						lprintf(0,"w pinned %s -> %s -> %s\n",
							posstr(b->pieces[j].pos),
							posstr(b->pieces[i].pos),
							posstr(b->pieces[k].pos)
							);
				}
			}
		}
	}


	for (i=18;i<32;i++) {
		if (!b->pieces[i].p) continue;

		pinner_mask = b->topieces[b->pieces[i].pos] & 
			b->sliding_mask & WHITE_MASK;

		while (pinner_mask) {
			j = ff_one(pinner_mask);
			pinner_mask &= ~(1<<j);

			/* If the pinned piece is attacking the pinner then 
			   it isn't a pin! It might be a skewer tho.
			   */
			if (b->topieces[b->pieces[j].pos] & (1<<i))
				continue;

			/* look for a piece that this bit is pinned against */
			for (k=16;k<24;k++) {
				if (!b->pieces[k].p) continue;
				if (k == i) continue;
				if (!same_line(b->pieces[j].pos, b->pieces[i].pos, 
					       b->pieces[k].pos)) continue;
				if (!empty_line(b, b->pieces[i].pos,
						b->pieces[k].pos))
				    continue;
				
				/* we have a likely pin. Now we need
				   to confirm that if the pinner could attack
				   the pinnedto piece then that piece
				   would be hung */
				if (get_control(b, 
						b->topieces[b->pieces[k].pos] | (1<<j), 
						b->pieces[k].p,
						b->pieces[k].pos) > 0) {
					b->pinned_mask |= (1<<i);
					if (debug)
						lprintf(0,"b pinned %s -> %s -> %s\n",
							posstr(b->pieces[j].pos),
							posstr(b->pieces[i].pos),
							posstr(b->pieces[k].pos)
							);
				}
			}
		}
	}
	
	return 0;
}


static etype eval_etype(Position *b, etype testv, int depth)
{
	etype ret, v, pv, pinv;
	int player = next_to_play(b);
	etype material;
	uint32 pieces_done;

#if PERFECT_EVAL
	testv = INFINITY;
	depth = MAX_DEPTH;
#endif

#if TEST_EVAL_SHORTCUT
	etype shortcut_ret = INFINITY;
	static int shortcut_total, shortcut_ok, eval_total;
#endif

	if ((b->flags & FLAG_EVAL_DONE) && (testv != INFINITY)) {
		return b->eval_result * player;
	}

	if (b->flags & FLAG_NEED_PART2) {
		lprintf(0,"WARNING: NEED_PART2 in eval\n");
	}

	if (learning && !mulling) {
		if ((b->flags & FLAG_FORCED_DRAW) && !debug) {
			b->flags |= FLAG_EVAL_DONE;
			b->eval_result = draw_value(b);
			return b->eval_result * player;
		}
	}

#if 0
	if (b->hash1 == 0x231672de) {
		debug = 1;
		print_board(b->board);
	} else {
		debug = 0;
	}
#endif

	init_eval_tables();

	/* passed pawns always get reevaluated, so stopppability does also */
	b->null_stoppable_pawn = 0;
	b->null_unstoppable_pawn = 0;

	if (b->oldb && !(b->oldb->flags & FLAG_EVAL_DONE)) {
		eval(b->oldb, testv, 1);
	}

	material = b->w_material - b->b_material;

	if (debug)
		lprintf(0,"w_material=%e b_material=%e\n",
			b->w_material, b->b_material);
	
	if (material > MAX_MATERIAL)
		material = MAX_MATERIAL;

	if (material < -MAX_MATERIAL)
		material = -MAX_MATERIAL;

	if (testv == INFINITY) {
		b->piece_change = b->material_mask;
		b->oldb = NULL;
	} else if (depth >= EVAL_ALL_DEPTH) {
		b->piece_change = b->material_mask;  
	} else if (b->oldb) {
		Square last_from = b->last_move.from;
		Square last_to = b->last_move.to;
		int fromy = YPOS(last_from);
		int toy = YPOS(last_to);
		
		/* we need to recalc a king if a pawn moves, as pawn
		   defence is critical */
		if ((b->oldb->board[last_from] == PAWN) ||
		    (b->oldb->board[last_to] == PAWN))
			b->piece_change |= WKING_MASK;

		if ((b->oldb->board[last_from] == -PAWN) ||
		    (b->oldb->board[last_to] == -PAWN))
			b->piece_change |= BKING_MASK;

		if (b->oldb->board[last_from] == KING && 
		    (b->piece_mask & WHITE_MASK) == WKING_MASK)
			b->piece_change |= (b->material_mask & ~b->piece_mask);

		if (b->oldb->board[last_from] == -KING &&
		    (b->piece_mask & BLACK_MASK) == BKING_MASK)
			b->piece_change |= (b->material_mask & ~b->piece_mask);

		if (b->oldb->board[last_to] == QUEEN)
			b->piece_change |= BKING_MASK;

		if (b->oldb->board[last_to] == -QUEEN)
			b->piece_change |= WKING_MASK;

		/* need to recalc king if bishops are captured */
		if (b->oldb->board[last_to] == BISHOP)
			b->piece_change |= BKING_MASK;

		if (b->oldb->board[last_to] == -BISHOP)
			b->piece_change |= WKING_MASK;

#if 0
		/* Changes in any pawn on the first rank may create
                   bishop or knight outosts on the two squares a
                   knight's move in front of the pawn */

		if (b->oldb->board[last_from] == PAWN && fromy == 1 &&
		    !(toy == 2 && tox == fromx)) {
			if (tox > 0) {
				b->piece_change |= (BMINOR_MASK & 
						    (1<<b->pboard[last_from+WEST+
								 2*NORTH]));
			}
			if (tox < 7) {
				b->piece_change |= (BMINOR_MASK & 
						    (1<<b->pboard[last_from+EAST+
						     2*NORTH]));
			}			
		} else if (b->oldb->board[last_to] == PAWN && toy == 1) {
			if (tox > 0) {
				b->piece_change |= (BMINOR_MASK & 
						    (1<<b->pboard[last_to+WEST+
								 2*NORTH]));
			}
			if (tox < 7) {
				b->piece_change |= (BMINOR_MASK & 
						    (1<<b->pboard[last_to+EAST+
								 2*NORTH]));
			}
		}

		if (b->oldb->board[last_from] == -PAWN && 
		    fromy == 6 && !(toy == 5 && tox == fromx)) {
			if (tox > 0) {
				b->piece_change |= (WMINOR_MASK & 
						    (1<<b->pboard[last_from+WEST+
						     2*SOUTH]));
			}
			if (tox < 7) {
				b->piece_change |= (WMINOR_MASK & 
						    (1<<b->pboard[last_from+EAST+
						     2*SOUTH]));
			}			
		} else if (b->oldb->board[last_to] == -PAWN && toy == 6) {
			if (tox > 0) {
				b->piece_change |= (WMINOR_MASK & 
						    (1<<b->pboard[last_to+WEST+
						     2*SOUTH]));
			}
			if (tox < 7) {
				b->piece_change |= (WMINOR_MASK & 
						    (1<<b->pboard[last_to+EAST+
						     2*SOUTH]));
			}			

		}
#endif
		/* b->piece_change |= MINOR_MASK; */

		/* blocking and unblocking passed pawns */
		if (toy>1 && 
		    (b->wpassed_pawn_mask & (1<<b->pboard[last_to+SOUTH])) &&
		    ((b->board[last_to] < 0 && b->oldb->board[last_to]==0) ||
		     b->oldb->board[last_to])) {
			b->piece_change |= (1<<b->pboard[last_to+SOUTH]);
		}
		if (toy<6 && 
		    (b->bpassed_pawn_mask & (1<<b->pboard[last_to+NORTH])) &&
		    ((b->board[last_to] > 0 && b->oldb->board[last_to]==0) ||
		     b->oldb->board[last_to])) {
			b->piece_change |= (1<<b->pboard[last_to+NORTH]);
		}

		if (fromy>1 && 
		    (b->wpassed_pawn_mask & (1<<b->pboard[last_from+SOUTH])) &&
		    b->board[last_to] < 0) {
			b->piece_change |= (1<<b->pboard[last_from+SOUTH]);
		}
		if (fromy<6 && 
		    (b->bpassed_pawn_mask & (1<<b->pboard[last_from+NORTH])) &&
		    b->board[last_to] > 0) {
			b->piece_change |= (1<<b->pboard[last_from+NORTH]);
		}
		
		/* unstoppable pawns can easily become stoppable, so 
		   we should recalculate all unstoppable pawns. 
		   Do this by recalculating all passed pawns */
		if ((b->piece_mask & WHITE_MASK) == WKING_MASK) {
			b->piece_change |= b->bpassed_pawn_mask;
		}
		if ((b->piece_mask & BLACK_MASK) == BKING_MASK) {
			b->piece_change |= b->wpassed_pawn_mask;
		}
		
	}

	ret = 0;

	/* need pins to compute various attack values (e.g weak pawn
	   attack) correctly */
	pinv = find_pins(b);

	estimate_game_stage(b, 0);

	if (debug)
		lprintf(0,"game stage=%d\n", b->stage);

	build_pawn_loc(b);

	v = specifics(b);
	ret += v;
	if (debug)
		lprintf(0,"specifics = %e\n", v);

	pieces_done = b->piece_change;
	pv = v = piece_values(b) * PIECE_VALUE_FACTOR;
	ret += v;
	if (debug)
		lprintf(0,"piece values = %e\n", v);

	v = king_safety(b);
	ret += v;
	if (debug)
		lprintf(0,"king safety = %e\n", v);

	v = eval_tactics(b);
	ret += v;
	if (debug)
		lprintf(0,"tactics = %e\n", v);

#if USE_EVAL_SHORTCUT
	if (depth <= 0 && 
	    b->oldb &&
	    abs(b->expensive) < EVAL_SHORTCUT_THRESHOLD &&
	    (ret + material + b->expensive) * player > 
	    testv + EVAL_SHORTCUT_THRESHOLD) {
		etype ret2 = (ret + material + b->expensive) * player;
		
		ret2 -= EVAL_SHORTCUT_OFFSET;
		ret2 *= player;
		
		ret2 += check_material(b, ret2);
		
		ret2 *= player;
		
		if (ret2 >= testv) {
#if TEST_EVAL_SHORTCUT
			shortcut_ret = ret2;
#else
			return ret2;
#endif
			}
	}
#endif

	b->expensive = 0;

	b->expensive += pinv;
	ret += pinv;

	v = board_control1(b) * BOARD_CONTROL_FACTOR;
	b->expensive += v;
	ret += v;
	if (debug)
		lprintf(0,"board control1 = %e\n", v);

	v = board_control2(b) * BOARD_CONTROL_FACTOR;
	b->expensive += v;
	ret += v;
	if (debug)
		lprintf(0,"board control2 = %e\n", v);
	
	b->piece_change &= ~pieces_done;
	v = piece_values(b) * PIECE_VALUE_FACTOR;
	ret += (v - pv);
	if (debug)
		lprintf(0,"piece values = %e\n", v);

	v = eval_mobility(b);
	b->expensive += v;
	ret += v;
	if (debug)
		lprintf(0,"mobility = %e\n", v);

	ret *= POSITIONAL_FACTOR;

	if (debug)
		lprintf(0,"pos value = %e\n", ret);

	b->flags |= FLAG_EVAL_DONE;

	ret += material;
	ret += check_material(b, ret);

	if (debug)
		lprintf(0,"eval = %e\n", ret);

#if TEST_EVAL_SHORTCUT
	if (shortcut_ret != INFINITY) {
		etype ret2 = ret * player;

		shortcut_total++;

		if ((ret2 < testv && shortcut_ret > testv) ||
		    (ret2 > testv && shortcut_ret < testv)) {
			lprintf(0,"shortcut: %d/%d/%d ret=%e shortcut_ret=%e testv=%d move=%s\n",
				shortcut_ok, shortcut_total, eval_total,
				ret2, shortcut_ret, testv,
				short_movestr(b, &b->last_move));
			print_board(b->board);
		} else {
			shortcut_ok++;
		}
	}
	eval_total++;
#endif

	b->eval_result = ret;
	return b->eval_result * player;
}


void eval_reset(void)
{
	int i, n = sizeof(coefficients) / sizeof(coefficients[0]);
	static etype *orig_coefficients;

	if (!orig_coefficients) {
		orig_coefficients = malloc(sizeof(coefficients));
		memcpy(orig_coefficients, coefficients, sizeof(coefficients));
		srandom(time(NULL));
	}

	memcpy(coefficients, orig_coefficients, sizeof(coefficients));
	
	for (i=0;i<n;i++) {
		coefficients[i] += coefficients[i] * (random()%100) * 
			0.01 * state->eval_weakness;
		if (orig_coefficients[i] < 0 && coefficients[i] > 0) {
			coefficients[i] = -1;
		}
		if (orig_coefficients[i] > 0 && coefficients[i] < 0) {
			coefficients[i] = 1;
		}
		if (coefficients[i] == 0) coefficients[i] = 1;
	}
}


Eval eval(Position *b, etype testv, int depth)
{
	Eval ret;

	ret = makeeval(b, eval_etype(b, testv, depth));

	if (EV(ret) > MAX_EVAL)
		EV(ret) = MAX_EVAL;
	if (EV(ret) < -MAX_EVAL)
		EV(ret) = -MAX_EVAL;
	return ret;
}

static Eval eval1(Position *b, etype testv, int depth)
{
	Eval ret1, ret2;
	Position b1, b2;

	b1 = (*b);
	b2 = (*b);
	
	ret1 = eval1(b, testv, depth);
	ret2 = eval1(&b1, INFINITY, depth);
	
	if (!(b->flags & FLAG_PROMOTE) &&
	    EV(ret1) != EV(ret2)) {
		lprintf(0,"****%d %d\n", EV(ret1), EV(ret2));
		
		if (b->oldb)
			print_board(b->oldb->board);
		print_board(b->board);
		
		debug = 1;
		lprintf(0, "*** INCREMENTAL b ***\n");
		ret1 = eval1(&b2, testv, depth);
		lprintf(0, "*** NON-INCREMENTAL b ***\n");
		b2.flags &= ~FLAG_EVAL_DONE;
		ret2 = eval1(b, INFINITY, depth);
		debug = 0;
		exit(1);

	}

	return ret1;
}


void eval_debug(Position *b)
{
	regen_moves(b);

	debug = 1;

	lprintf(0,"white_moves=%d black_moves=%d\n",
		b->white_moves, b->black_moves);

	b->flags |= FLAG_COMPUTER_WHITE;
	b->flags &= ~FLAG_EVAL_DONE;
	eval(b, INFINITY, MAX_DEPTH);
	b->flags &= ~FLAG_COMPUTER_WHITE;
	b->flags &= ~FLAG_EVAL_DONE;
	eval(b, INFINITY, MAX_DEPTH);
	
	debug = 0;
}


