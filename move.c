#include "includes.h"
#include "knightcap.h"

static int nodes, quiesce_nodes, extended_nodes, truncated_nodes, confirm_nodes;
static int maxply, maxdepth;
static int move_order_hits, move_order_misses;
static int pondering, ponder_stop;
static int search_depth;
extern int mulling;
extern int demo_mode;
extern int bad_eval_fd;
extern int learning;
extern int pop_count[256];

static volatile Position *real_position;
static volatile Position *ponder_position;

#if TREE_PRINT_PLY
static int tree_print_ply = TREE_PRINT_PLY;
#endif

extern struct state *state;

static int null_ply[MAX_DEPTH+10];

#if USE_SMP
static int forked;
static int slave_pids[MAX_CPUS];
static int slave_num = -1;
static struct slave slave_in;
static struct slave slave_out;
#endif

static int search_expired(void)
{
#if USE_SMP
	if (slave_num != -1) {
		if (slave_out.index != state->slavep[slave_num].index ||
		    state->slavep[slave_num].index == 0)
			return 1;
		return 0;
	}
#endif

	if (state->quit || state->stop_search)
		return 1;

	if (mulling) {
		if (state->computer)
			return 1;
		return 0;
	} else {
		if (state->computer == 0) 
			return 1;
	}

	if (pondering) {
		if (ponder_stop)
			return 1;

		if (next_to_play(&state->position) == -state->computer)
			return 0;
		if (real_position->hash1 != ponder_position->hash1) {
#if STORE_LEAF_POS
			state->predicted_move[state->stored_move_num] = 0;
#endif
			return 1;
		}
		lprintf(0,"predicted move\n");
#if STORE_LEAF_POS
		state->predicted_move[state->stored_move_num] = 1;
#endif
		
		pondering = 0;
		return 0;
	}
	
#if USE_SMP
	if (nodes >= TIMER_NODES && timer_expired()) return 1;
#else
	/* check for timer expiry every few nodes (too expensive to
	   check every node, and alarm isn't accurate enough for 
	   lightning) */
	if (search_depth > 2) {
		static int last_test;
		if (nodes < last_test) {
			last_test = nodes;
		} 

		if (nodes > last_test + TIMER_NODES) {
			if (timer_expired()) return 1;
			last_test = nodes;
		}
	}	
#endif

	return 0;
}


static inline int depth_extensions(Position *b, int depth, int ply,
				   Eval v1, Eval testv)
{
	if (state->max_search_depth) {
		return depth;
	}

	if (null_ply[ply-1])
		return depth;

	if ((depth+ply) >= search_depth+MAX_EXTENSIONS)
		goto razoring;

#if USE_PAWN_DEEPENING
	if ((b->board[b->last_move.to] == PAWN && 
	     YPOS(b->last_move.to) > 4 && 
	     b->control[b->last_move.to] >= 0 && 
	     b->board[b->last_move.to+NORTH] == 0 &&
	     b->control[b->last_move.to+NORTH] >= 0) ||
	    (b->board[b->last_move.to] == -PAWN && 
	     YPOS(b->last_move.to) < 3 &&
	     b->control[b->last_move.to] <= 0 && 
	     b->board[b->last_move.to+SOUTH] == 0 &&
	     b->control[b->last_move.to+SOUTH] <= 0)) {
		b->flags |= FLAG_EXTENDED;
		return depth+1;
	}
#endif

#if USE_CHECK_DEEPENING
	if (b->flags & FLAG_CHECK) {
		b->flags |= FLAG_EXTENDED;
		return depth+1;		
	}
#endif

	if (b->oldb && (b->oldb->flags & FLAG_EXTENDED)) {
		return depth;
	}

#if USE_RECAPTURE_DEEPENING
	if (b->oldb) {
		/* if its a recapture then extend - this
		   ensures we get to the end of capture
		   sequences */
		if (b->oldb->oldb &&
		    b->last_move.to == b->oldb->last_move.to &&
		    b->oldb->oldb->board[b->oldb->last_move.to] 
		    && abs(b->board[b->last_move.to]) != PAWN) {
			int mat1 = b->w_material - b->b_material;
			int mat2 = b->oldb->oldb->w_material - 
				b->oldb->oldb->b_material;
			if (abs(mat1 - mat2) < PAWN_VALUE/2) {
				b->flags |= FLAG_EXTENDED;		
				return depth+1;
			}
		}
	}
#endif

#if USE_BEST_CAPTURE_DEEPENING
	if (b->oldb && 
	    same_move(&b->oldb->best_capture, &b->last_move)) {
		b->flags |= FLAG_EXTENDED;		
		return depth+1;
	}
#endif

#if USE_HUNG_CAPTURE_DEEPENING
	if (b->oldb) {
		/* if its a capture of a hung piece then extend */
		if (abs(b->oldb->board[b->last_move.to]) > PAWN &&
		    !(b->hung_mask & (1<<b->pboard[b->last_move.to]))
		    ) {
			b->flags |= FLAG_EXTENDED;		
			return depth+1;
		}
	}
#endif

razoring:

	if ((b->flags & FLAG_EXTENDED) || 
	    (b->oldb && (b->oldb->flags & FLAG_EXTENDED)))
		return depth;

#if USE_RAZORING
	if (depth == 1 && 
	    EV(v1) < (EV(testv) - RAZOR_THRESHOLD) && b->oldb && 
	    !(b->oldb->flags & FLAG_EXTENDED)) {
		return 0;
	}
#endif

#if USE_ASYMMETRIC_RAZORING
	if (depth == 2 && 
	    computers_move(b) &&
	    EV(v1) < (EV(testv) - RAZOR_THRESHOLD) && b->oldb && 
	    !(b->oldb->flags & FLAG_EXTENDED)) {
		return 0;
	}
#endif

	return depth;
}



static int quiesce_start_ply;

static Eval quiesce(Position *b, Eval testv, int ply);


__fastfunc(static int try_black_capture(Position *b, int i, int j, int v1, int testv, Move *m1))
{
#if USE_QUIESCE_SHORTCUT
	int v2;

	if (same_move(&b->best_capture, m1))
		return 0;

	/* always try captures that might lead to check */
	if (!(b->flags & FLAG_COMPUTER_WHITE)) {
		if (capture_map[b->board[m1->from]+KING][m1->to][BLACKPIECES(b)[IKING].pos])
			return 1;

		/* always try a recapture */
		if (m1->to == b->last_move.to)
			return 1;
	}

	/* estimate how much this capture is worth */
	v2 = v1 + mat_value(-b->board[m1->to]);

	/* be optimistic about the positional values */
	if (b->piece_values[i] < 0)
		v2 += -b->piece_values[i];
		
	if (b->piece_values[j] < 0)
		v2 += -b->piece_values[j];
	
	/* if its not hung and the capturing piece isn't hung then
	   presume we will lose the piece we are capturing with */
	if (!(b->hung_mask & (1<<i)) && !(b->hung_mask & (1<<j)))
		v2 -= mat_value(b->board[m1->from]);

	/* don't capture if the estimated value is well below testv */
	if (v2 < testv - FUTILE_THRESHOLD)
		return 0;
#endif
	return 1;
}

__fastfunc(static int try_white_capture(Position *b, int i, int j, int v1, int testv, Move *m1))
{
#if USE_QUIESCE_SHORTCUT
	int v2;

	if (same_move(&b->best_capture, m1))
		return 0;

	if (b->flags & FLAG_COMPUTER_WHITE) {
		/* always try captures that might lead to check */
		if (capture_map[b->board[m1->from]+KING][m1->to][WHITEPIECES(b)[IKING].pos])
			return 1;

		/* always try a recapture */
		if (m1->to == b->last_move.to)
			return 1;
	}

	/* estimate how much this capture is worth */
	v2 = v1 + mat_value(b->board[m1->to]);

	/* be optimistic about the positional values */
	if (b->piece_values[i] > 0)
		v2 += b->piece_values[i];

	if (b->piece_values[j] > 0)
		v2 += b->piece_values[j];
	
	/* if its not hung and the capturing piece isn't hung then
	   presume we will lose the piece we are capturing with */
	if (!(b->hung_mask & (1<<i)) && !(b->hung_mask & (1<<j)))
		v2 -= mat_value(-b->board[m1->from]);

	if (v2 < testv - FUTILE_THRESHOLD)
		return 0;
#endif

	return 1;
}


static Eval quiesce_check(Position *b, Eval testv, int ply)
{
	Eval g, v;
	Move *moves;
	int num_moves, m;
	Position b1;

	if (!(b->flags & FLAG_EVAL_DONE)) {
		eval(b, EV(testv), 1);
	}

	/* check is a special case. All moves are generated
	   and the player can't elect to just "sit" */
	if (!b->moves) 
		gen_move_list(b, ply, NULL, testv);
	moves = b->moves;
	num_moves = b->num_moves;

	g = makeeval(b, ILLEGAL);

	for (m=0;m<num_moves;m++) {
		if (!do_move_part1(&b1, b, &moves[m])) continue;
		
		v = quiesce(&b1, flip(testv), ply+1);
		EV(v) = -EV(v);

#if TREE_PRINT_PLY
		if (ply < tree_print_ply) {
			lindent(2, ply);
			lprintf(2, "%s %d (%s) %s -> %e (%e) testv=%e QCheck\n",
				whites_move(b)?"W":"B", 
				b->move_num,
				short_movestr(b, &b->last_move), 
				short_movestr(b, &moves[m]), 
				EV(v), EV(g), EV(testv));
		}
#endif

		if (EV(v) > EV(g)) {
			g = v;
			if (EV(g) >= EV(testv)) 
				break;
		}
	}

#if USE_HASH_TABLES
	insert_hash(b, 0, testv, g, NULL);
#endif

	if (EV(g) > FORCED_WIN) {
		EV(g) -= 1;
	} else if (EV(g) < -FORCED_WIN) {
		EV(g) += 1;
	}	

	return g;
}


__fastfunc(static Eval quiesce(Position *b, Eval testv, int ply))
{
	Move m1;
	Eval g, v1, v;
	int i, j;
	uint32 pieces, topieces;
	Position b1;

	if (ply != quiesce_start_ply) {
		quiesce_nodes++;
	} 

	if (ply > maxply)
		maxply = ply;

	if (EV(testv) > FORCED_WIN && EV(testv) < WIN) {
		EV(testv) += 1;
	} else if (EV(testv) < -FORCED_WIN && EV(testv) > -WIN) {
		EV(testv) -= 1;
	}

#if USE_HASH_TABLES
	if (check_hash(b, 0, testv, &g, NULL) == HASH_HIT) {
#if TREE_PRINT_PLY
		if (ply < tree_print_ply) {
			lindent(2, ply);
			lprintf(2,"Quiesce hash hit v=%e hash1=%08x\n", 
				EV(g), b->hash1);
		}
#endif

		if (EV(g) > FORCED_WIN) {
			EV(g) -= 1;
		} else if (EV(g) < -FORCED_WIN) {
			EV(g) += 1;
		}
		return g;
	}
#endif

	if (!do_move_part2(b))
		return seteval(testv, -ILLEGAL);

	if ((b->flags & FLAG_CHECK) && (ply < MAX_DEPTH)) {
		return quiesce_check(b, testv, ply);
	}

	zero_move(&m1);

	v1 = eval(b, EV(testv), 0);
	g = v1;

	/* in the quiesce search the player has the option to "sit" and
	   take the current evaluation */
	if (EV(g) >= EV(testv) || ply >= MAX_DEPTH) {
		goto evaluated;
	}

	if (!(b->flags & FLAG_EVAL_DONE)) {
		v1 = eval(b, EV(testv), 1);
		g = v1;
		if (EV(g) >= EV(testv)) {
			goto evaluated;
		}
#if TEST_QUIESCE
		if (!(b->flags & FLAG_EVAL_DONE)) {
			lprintf(0,"eval not done!\n");
		}
#endif
	}

	if (EV(g) < EV(testv) - FUTILE_THRESHOLD/2) {
		EV(g) = EV(testv) - FUTILE_THRESHOLD/2;
#if STORE_LEAF_POS		
		g.pos.flags |= FLAG_FUTILE;
#endif
	}
	if (whites_move(b)) {
		/* first see if we can promote */
		pieces = b->pawns7th & WHITE_MASK;

		while (pieces) {
			i = ff_one(pieces);
			pieces &= ~(1<<i);
			if (b->board[b->pieces[i].pos+NORTH]) 
				continue;

			m1.from = b->pieces[i].pos;
			m1.to = b->pieces[i].pos + NORTH;

			if (!do_move_part1(&b1, b, &m1)) continue;

			v = quiesce(&b1, flip(testv), ply+1);
			EV(v) = -EV(v);
			if (EV(v) > EV(g)) {
				g = v;
				if (EV(g) >= EV(testv)) 
					goto evaluated;
			}
		}

		/* try the "best capture" move from the tactical eval */
		m1 = b->best_capture;
		if (!is_zero_move(&m1) && b->board[m1.to] < 0 &&
		    b->board[m1.from] > 0 && 
		    (b->topieces[m1.to] & (1<<b->pboard[m1.from])) &&
		    do_move_part1(&b1, b, &m1)) {
			v = quiesce(&b1, flip(testv), ply+1);
			EV(v) = -EV(v);
			if (EV(v) > EV(g)) {
				g = v;
				if (EV(g) >= EV(testv)) {
					goto evaluated;
				}
			}
		}

#if USE_FAST_QUIESCE
		goto evaluated;
#endif

		pieces = b->material_mask & BLACK_MASK;

		while (pieces) {
			i = ff_one(pieces);
			pieces &= ~(1<<i);

			topieces = b->topieces[b->pieces[i].pos] & WHITE_MASK;
			if (!topieces) continue;

			/* loop over all the capture possabilities */
			while (topieces) {
#if TEST_QUIESCE
				int is_futile = 0;
#endif

				j = fl_one(topieces);
				topieces &= ~(1<<j);
				m1.from = b->pieces[j].pos;
				m1.to = b->pieces[i].pos;

				if (!try_black_capture(b, i, j, EV(v1),
						       imax(EV(g), EV(testv)), &m1)) {

#if TEST_QUIESCE
					is_futile = 1;
#else
					continue;
#endif	
				}

				if (!do_move_part1(&b1, b, &m1)) continue;

				v = quiesce(&b1, flip(testv), ply+1);
				EV(v) = -EV(v);
				if (EV(v) > EV(g)) {
#if TEST_QUIESCE
					if (is_futile && EV(v) >= EV(testv)) {
						lprintf(0,"quiesce bug: v=%e g=%e testv=%e v1=%e mat=%e p1=%d p2=%d move=%s\n", 
							EV(v), EV(g), EV(testv),
							EV(v1), 
							mat_value(-b->board[m1.to]),
							b->piece_values[i],
							b->piece_values[j],
							short_movestr(b, &m1));
						lprintf(0,"%s\n", position_to_ppn(b));
						print_board(b->board);
						try_black_capture(b, i, j, EV(v1),
								  imax(EV(g), EV(testv)), 
								  &m1);
					}
#endif
					g = v;
					if (EV(g) >= EV(testv)) {
						
						goto evaluated;
					}
				}
			}
		}
		
		/* one more chance - an enpassent capture */
		if (!b->enpassent) 
			goto evaluated;

		topieces = b->topieces[b->enpassent] & WHITE_MASK & ~b->piece_mask;
		while (topieces) {
			j = ff_one(topieces);
			topieces &= ~(1<<j);
			
			m1.from = b->pieces[j].pos;
			m1.to = b->enpassent;
			
			if (!do_move_part1(&b1, b, &m1)) continue;
			
			v = quiesce(&b1, flip(testv), ply+1);
			EV(v) = -EV(v);
			if (EV(v) > EV(g)) {
				g = v;
				if (EV(g) >= EV(testv)) 
					goto evaluated;
			}
		}
	} else {
		/* first see if we can promote */
		pieces = b->pawns7th & BLACK_MASK;

		while (pieces) {
			i = ff_one(pieces);
			pieces &= ~(1<<i);
			if (b->board[b->pieces[i].pos+SOUTH]) 
				continue;

			m1.from = b->pieces[i].pos;
			m1.to = b->pieces[i].pos + SOUTH;

			if (!do_move_part1(&b1, b, &m1)) continue;

			v = quiesce(&b1, flip(testv), ply+1);
			EV(v) = -EV(v);
			if (EV(v) > EV(g)) {
				g = v;
				if (EV(g) >= EV(testv)) 
					goto evaluated;
			}
		}

		/* try the "best capture" move from the tactical eval */
		m1 = b->best_capture;
		if (!is_zero_move(&m1) && b->board[m1.to] > 0 &&
		    b->board[m1.from] < 0 && 
		    (b->topieces[m1.to] & (1<<b->pboard[m1.from])) &&
		    do_move_part1(&b1, b, &m1)) {
			v = quiesce(&b1, flip(testv), ply+1);
			EV(v) = -EV(v);
			if (EV(v) > EV(g)) {
				g = v;
				if (EV(g) >= EV(testv)) {
					goto evaluated;
				}
			}
		}

#if USE_FAST_QUIESCE
		goto evaluated;
#endif

		pieces = b->material_mask & WHITE_MASK;

		while (pieces) {
			i = ff_one(pieces);
			pieces &= ~(1<<i);

			topieces = b->topieces[b->pieces[i].pos] & BLACK_MASK;
			if (!topieces) continue;

			/* loop over all the capture possabilities */
			while (topieces) {
#if TEST_QUIESCE
				int is_futile = 0;
#endif

				j = fl_one(topieces);
				topieces &= ~(1<<j);
				m1.from = b->pieces[j].pos;
				m1.to = b->pieces[i].pos;

				if (!try_white_capture(b, i, j, EV(v1),
						       imax(EV(g), EV(testv)), &m1)) {

#if TEST_QUIESCE
					is_futile = 1;
#else
					continue;
#endif	
				}

				if (!do_move_part1(&b1, b, &m1)) continue;

				v = quiesce(&b1, flip(testv), ply+1);
				EV(v) = -EV(v);
				if (EV(v) > EV(g)) {
#if TEST_QUIESCE
					if (is_futile && EV(v) >= EV(testv)) {
						lprintf(0,"quiesce bug: v=%e g=%e testv=%e v1=%e mat=%e p1=%d p2=%d move=%s\n", 
							EV(v), EV(g), EV(testv),
							EV(v1), 
							mat_value(b->board[m1.to]),
							b->piece_values[i],
							b->piece_values[j],
							short_movestr(b, &m1));
						lprintf(0,"%s\n", position_to_ppn(b));
						print_board(b->board);
					}
#endif
					g = v;
					if (EV(g) >= EV(testv)) {
						goto evaluated;
					}
				}
			}
		}
		
		/* one more chance - an enpassent capture */
		if (!b->enpassent) 
			goto evaluated;

		topieces = b->topieces[b->enpassent] & BLACK_MASK & ~b->piece_mask;
		while (topieces) {
			j = ff_one(topieces);
			topieces &= ~(1<<j);
			
			m1.from = b->pieces[j].pos;
			m1.to = b->enpassent;
			
			if (!do_move_part1(&b1, b, &m1)) continue;
			
			v = quiesce(&b1, flip(testv), ply+1);
			EV(v) = -EV(v);
			if (EV(v) > EV(g)) {
				g = v;
				if (EV(g) >= EV(testv)) 
					goto evaluated;
			}
		}
	}

	zero_move(&m1);

evaluated:

#if TREE_PRINT_PLY
	if (ply < tree_print_ply) {
		lindent(2, ply);
		lprintf(2, "%s %d (%s) %s -> %e v1=%e testv=%e %s Quiesce\n",
			whites_move(b)?"W":"B", 
			b->move_num,
			short_movestr(b, &b->last_move), 
			short_movestr(b, &m1), 
			EV(g), EV(v1), EV(testv), 
			(b->flags & FLAG_EVAL_DONE)?"":"quick");
	}
#endif

#if USE_HASH_TABLES
	insert_hash(b, 0, testv, g, &m1);
#endif

	if (EV(g) > FORCED_WIN) {
		EV(g) -= 1;
	} else if (EV(g) < -FORCED_WIN) {
		EV(g) += 1;
	}	

	return g;
}


static Eval start_quiesce(Position *b, Eval testv, int ply)
{
	quiesce_start_ply = ply+1;

	if (ply > maxdepth)
		maxdepth = ply;

	if (ply > search_depth)
		extended_nodes++;

	if (ply < search_depth)
		truncated_nodes++;

	nodes++;

#if NO_QUIESCE
	{
		Eval v = eval(b, EV(testv), 0);
		insert_hash(b, 0, testv, v, NULL);
		return v;
	}
#else
	return quiesce(b, testv, ply+1);
#endif
}

/* this is like alpha-beta but only does a single null window search.
   It assumes that beta=testv and alpha=testv-1. */
static Eval abtest(Position *b, int depth0, Eval testv, int ply)
{
	Move *moves;
	int num_moves, m;
	int bm = -1;
	Eval g;
	Eval v1, v;
	int depth = depth0;
	int mate_threat = 0;
	Move m1;

	if (depth <= 0) {
		return start_quiesce(b, testv, ply);
	}

	nodes++;

	if (ply > maxply)
		maxply = ply;

	v1 = makeeval(b, INFINITY);

	if (EV(testv) > FORCED_WIN && EV(testv) < WIN) {
		EV(testv) += 1;
	} else if (EV(testv) < -FORCED_WIN && EV(testv) > -WIN) {
		EV(testv) -= 1;
	}

	zero_move(&m1);

#if USE_HASH_TABLES
	if (check_hash(b, depth, testv, &v1, &m1) == HASH_HIT) {
#if TREE_PRINT_PLY
		if (ply < tree_print_ply) {
			lindent(2, ply);
			lprintf(2,"hash hit depth=%d v=%e move=%s hash1=%08x\n", 
				depth, 
				EV(v1), short_movestr(b, &m1),
				b->hash1);
		}
#endif
		if (EV(v1) > FORCED_WIN) {
			EV(v1) -= 1;
		} else if (EV(v1) < -FORCED_WIN) {
			EV(v1) += 1;
		}
		return v1;
	}

#if 0
	/* this lowers the number of null move trees we traverse pointlessly */
	if (null_ply[ply-1] &&
	    check_hash(b,(depth-1), testv, &v1, &m1) == HASH_HIT &&
	    EV(v1) >= EV(testv)) {
		return v1;
	}
#endif
#endif

	if (!do_move_part2(b))
		return seteval(testv, -ILLEGAL);

	if (EV(v1) == INFINITY) {
		v1 = eval(b, EV(testv), depth);
	}

	m = -1;

	depth = depth_extensions(b, depth, ply, v1, testv);

	if (depth <= 0) {
		return start_quiesce(b, testv, ply);
	}

	g = makeeval(b, ILLEGAL);

#if USE_NULL_MOVE
	if (ply < MAX_DEPTH && 
	    (b->piece_mask & player_mask(b) & ~KING_MASK) &&
	    !null_ply[ply-1] &&	    
#if USE_ASYMMETRIC_NULLMOVE
	    (!computers_move(b) || depth > 2) &&
#endif
	    !(b->flags & FLAG_CHECK) &&
	    (whites_move(b)?b->white_moves:b->black_moves) > NULL_MOVE_THRESHOLD) {
		int enpassent_saved = b->enpassent;
		int depth2;
#if USE_NULLMOVE_FIXUP
		Move cap_saved = b->best_capture;
		etype tactics_saved = b->tactics;
		etype eval_saved = b->eval_result;
		uint32 flags_saved = b->flags;
#endif

		depth2 = depth-4;

#if USE_ASYMMETRIC_NULLMOVE
		/* this stops some of the worst null move blunders */
		if (computers_move(b))
			depth2 = imax(depth2, 1);
#endif

		null_ply[ply] = 1;			

#if TEST_NULL_MOVE
		b->flags &= ~FLAG_EVAL_DONE;
		b->flags &= ~FLAG_DONE_TACTICS;
		v = eval(b,INFINITY,MAX_DEPTH);
		if (EV(v) != eval_saved*next_to_play(b)) {
			lprintf(0,"***Incremental error %e %e\n", EV(v), 
				eval_saved*next_to_play(b));
		}
#endif

		b->enpassent = 0;
		b->move_num++;
		
		moves = b->moves;
		num_moves = b->num_moves;
		b->moves = NULL;
		b->num_moves = 0;
		b->hash1 ^= 1;


#if USE_NULLMOVE_FIXUP
		update_pawns(b);
		update_tactics(b);
#endif

#if TEST_NULL_MOVE
		{
			etype temp_result;

			if ((b->flags & FLAG_EVAL_DONE) && 
			    (b->flags & FLAG_DONE_TACTICS) &&
			    !(b->flags & FLAG_PROMOTE)) {
				b->flags &= ~FLAG_EVAL_DONE;
				b->flags &= ~FLAG_DONE_TACTICS;
				temp_result = b->eval_result*next_to_play(b);
				v = eval(b, INFINITY, MAX_DEPTH);
				if (EV(v) != temp_result) {
					lprintf(0, "****Null move error: %e %e %d\n", EV(v), 
						temp_result, whites_move(b));
					print_board(b->board);
					lprintf(0,"\n***After***\n");
					eval_debug(b);
					b->move_num--;
					b->flags &= ~FLAG_EVAL_DONE;
					b->flags &= ~FLAG_DONE_TACTICS;
					lprintf(0,"\n***Before***\n");
					eval_debug(b);
					if (b->fifty_count < 20)
						exit(1);
				}
			}
		}
#endif
#if 0
		{
			int t2 = b->tactics;
			b->flags &= ~FLAG_DONE_TACTICS;
			if (t2 != eval_tactics(b)) {
				lprintf(0,"bad tactics %e %e %e\n",
					tactics_saved, t2, b->tactics);
			}
		}
#endif

		v = abtest(b, depth2, flip(testv), ply+1);
		EV(v) = -EV(v);

#if CONFIRM_NULL_MOVES
		if (EV(v) >= EV(testv) && !computers_move(b)) {
			int old_nodes = nodes + quiesce_nodes;
			b->moves = NULL;
			b->num_moves = 0;
			v = abtest(b, depth2+1, flip(testv), ply+1);
			EV(v) = -EV(v);			
			confirm_nodes += (nodes + quiesce_nodes - old_nodes);
		}
#endif

#if USE_NULLMOVE_FIXUP
		b->tactics = tactics_saved;
		b->best_capture = cap_saved;
		b->eval_result = eval_saved;
		b->flags = flags_saved;
#endif

		b->hash1 ^= 1;
		b->move_num--;
		b->enpassent = enpassent_saved;
		b->moves = moves;
		b->num_moves = num_moves;

		null_ply[ply] = 0;

		/* don't let it see false zugzwang mates */
		if (EV(v) >= WIN) {
			EV(v) -= PAWN_VALUE;
		}
		
		if (EV(v) >= EV(testv)) {
			g = v;
			goto evaluated;
		}

		if (EV(v) <= -FORCED_WIN) {
			/* they are threatening mate! extend */
			if ((depth+ply) < search_depth+MAX_EXTENSIONS) {
				b->flags |= FLAG_EXTENDED;
				mate_threat = 1;
				depth += 1;
			}
		}
	}
#endif

#if USE_PESSIMISTIC_PRUNING
	if (depth <= 3 && computers_move(b)) {
		v = quiesce(b, testv, ply+1);
		if (EV(v) < EV(testv) - PAWN_VALUE/2) {
			EV(v) += PAWN_VALUE/4;
			g = v;
			goto evaluated;
		}
	}
#endif


	if (!b->moves) {
		gen_move_list(b, ply, &m1, testv);

		moves = b->moves;
		num_moves = b->num_moves;

		for (m=0;m<num_moves;m++) 
			moves[m].v = ILLEGAL+1;
	}

	moves = b->moves;
	num_moves = b->num_moves;

	if (num_moves == 0) {
		if (b->flags & FLAG_CHECK) 
			g = makeeval(b, ILLEGAL);
		else 
			g = makeeval(b, 0);
		goto evaluated;
	}

#if USE_SINGULAR_EXTENSION
	if ((b->flags & FLAG_CHECK) && num_moves == 1 && 
	    b->oldb && !(b->oldb->flags & FLAG_EXTENDED) &&
	    (depth+ply) < search_depth+MAX_EXTENSIONS) {
		b->flags |= FLAG_EXTENDED;
		depth++;
	}
#endif

	if (depth > depth0 + 1)
		depth = depth0 + 1;


#if USE_ETTC_HASH
	if (ettc_check_hash(b, moves, num_moves,
			    depth, testv, &v1, NULL)) {
		if (EV(v1) > FORCED_WIN) {
			EV(v1) -= 2;
		} else if (EV(v1) < -FORCED_WIN) {
			EV(v1) += 2;
		}
		return v1;
	}
#endif

	for (m=0;m<num_moves;m++) {
		Position b1;

#if 0
		/* a crazy selective search idea */
		if (m > 0 && EV(g) > EV(testv) - PAWN_VALUE/2) {
			v2.v = eval_move_tactics(b, &moves[m]);
			if (v2.v < 0 && 
			    v2.v + EV(v1) < EV(testv) - 0.8*PAWN_VALUE) {
				continue;
			}
		}
#endif

#if USE_SMP
		if (search_expired())
			return g;
#endif

		if (!do_move_part1(&b1, b, &moves[m])) {
			moves[m].v = ILLEGAL;
			continue;
		}

		if (check_repitition(&b1, 1)) {
			v = makeeval(b, draw_value(b) * next_to_play(b));
#if STORE_LEAF_POS
			v.pos.flags |= FLAG_FORCED_DRAW;
#endif
		} else {
			if (depth > depth0 && !mate_threat && !(b->flags & FLAG_CHECK)) {
				v = abtest(&b1, depth0-1, flip(testv), ply+1);
				EV(v) = -EV(v);
				if (computers_move(b)) {
					if (EV(v) != ILLEGAL && EV(v) >= EV(testv)) {
						b1.moves = NULL;
						b1.num_moves = 0;
						v = abtest(&b1, depth-1, flip(testv), ply+1);
						EV(v) = -EV(v);
					}
				} else {
					if (EV(v) != ILLEGAL && EV(v) < EV(testv)) {
						b1.moves = NULL;
						b1.num_moves = 0;
						v = abtest(&b1, depth-1, flip(testv), ply+1);
						EV(v) = -EV(v);
					}
				}
			} else {
				v = abtest(&b1, depth-1, flip(testv), ply+1);
				EV(v) = -EV(v);
			}
		}

		moves[m].v = EV(v);

		if (search_expired()) 
			return g;

		if (EV(v) > EV(g)) {
			g = v;
			bm = m;
		} 

#if TREE_PRINT_PLY
		if (ply < tree_print_ply) {
			lindent(2, ply);
			lprintf(2, "%s %d (%s) %s -> %e (%e) testv=%e depth=%d\n",
				whites_move(b)?"W":"B", 
				b->move_num,
				short_movestr(b, &b->last_move), 
				short_movestr(b, &moves[m]), 
				EV(v), EV(g), EV(testv), depth0);
		}
#endif

		if (EV(g) >= EV(testv)) {
			if (m == 0) {
				move_order_hits++;
			} else {
				move_order_misses++;
			}
			cutoff_hint(b, m, depth, ply);
			goto end_loop;
		}
	}

end_loop:

#if USE_SMP
	if (search_expired())
		return 0;
#endif

evaluated:

	if (search_expired()) 
		return g;

	if (EV(g) == ILLEGAL && !(b->flags & FLAG_CHECK)) {
 		g = makeeval(b, 0);
	}

#if TREE_PRINT_PLY
	if (ply < tree_print_ply) {
		lindent(2, ply);
		lprintf(2,"hash insert depth=%d v=%e hash1=%08x testv=%e\n", 
			depth0, EV(g), b->hash1, EV(testv));
	}
#endif
#if USE_HASH_TABLES
	insert_hash(b, depth0, testv, g, bm>=0?&moves[bm]:NULL);
#endif

	if (EV(g) > FORCED_WIN) {
		EV(g) -= 1;
	} else if (EV(g) < -FORCED_WIN) {
		EV(g) += 1;
	}

	return g;
}


/* this is the abtest routine for the root node */
static Eval abroot(Position *b, int depth, Eval testv, Move *move)
{
	Move *moves, m1;
	int num_moves, m;
	Eval g;
	Eval v;
	Position b1;

	search_depth = depth;

	g = makeeval(b, ILLEGAL);

	zero_move(&m1);

	nodes++;

#if USE_HASH_TABLES
	if (check_hash(b, depth, testv, &v, &m1) == HASH_HIT &&
	    !is_zero_move(&m1) && legal_move(b, &m1)) {
		do_move(&b1, b, &m1);
		if (!check_repitition(&b1, 1)) {
			if (!same_move(&m1, move)) {
				lprintf(0,"%s %s (HH)\n", 
					short_movestr(b, &m1), 
					evalstr(EV(v)));
			}
			(*move) = m1;
			return v;
		}
	}
#endif

	moves = b->moves;
	num_moves = b->num_moves;

	m1 = (*move);

#if USE_ETTC_HASH
	if (ettc_check_hash(b, moves, num_moves,
			    depth, testv, &v, &m1)) {
		if (!same_move(&m1, move)) {
			lprintf(0,"%s %s (ETTC)\n", 
				short_movestr(b, &m1), 
				evalstr(EV(v)));
		}
		(*move) = m1;
		return v;
	}
#endif

	if (!is_zero_move(move))
		m1 = (*move);

	order_moves(b, moves, num_moves, 0, &m1, testv);

	for (m=0;m<num_moves;m++) {

		if (!do_move(&b1, b, &moves[m])) {
			moves[m].v = ILLEGAL;
			continue;
		}

		if (check_repitition(&b1, 1)) {
			v = makeeval(b, draw_value(b) * next_to_play(b));
#if STORE_LEAF_POS
			v.pos.flags |= FLAG_FORCED_DRAW;
#endif
		} else {
			v = abtest(&b1,depth-1,flip(testv),1);
			EV(v) = -EV(v);
		}

		if (search_expired()) 
			return g;

		moves[m].v = EV(v);

		if (EV(v) > EV(g)) {
			g = v;
		} 

#if TREE_PRINT_PLY
		if (0 < tree_print_ply) {
			lindent(2, 0);
			lprintf(2, "%s %d (%s) %s -> %e (%e) testv=%e depth=%d\n",
				whites_move(b)?"W":"B", 
				b->move_num,
				short_movestr(b, &b->last_move), 
				short_movestr(b, &moves[m]), 
				EV(v), EV(g), EV(testv), depth);
		}
#endif

		if (EV(g) >= EV(testv)) {
			if (!same_move(&moves[m], move)) {
				lprintf(0,"%s %s\n", 
					short_movestr(b, &moves[m]), 
					evalstr(EV(g)));
			}
			(*move) = moves[m];
			goto end_loop;
		}
	}

end_loop:

	if (EV(g) == ILLEGAL && !(b->flags & FLAG_CHECK)) {
		g = makeeval(b, 0);
	}

#if USE_HASH_TABLES
	insert_hash(b, depth, testv, g, move);
#endif

	return g;
}


/* generate the root node moves list */
static void root_moves(Position *b, Move *move) 
{
	Move *moves;
	int num_moves, m;

	if (b->moves)
		return;

	/* generate the root node move list */
	gen_move_list(b, 0, move, makeeval(b,0));

	moves = b->moves;
	num_moves = b->num_moves;

	for (m=0;m<num_moves;m++) {
		moves[m].v = eval_move_tactics(b, &moves[m]);
	}

	sort_moves(moves, num_moves);
	
	while (num_moves && moves[num_moves-1].v <= ILLEGAL) 
		num_moves--;
	b->num_moves = num_moves;
}



#if USE_SMP
void smp_slave(void)
{
	Eval v;
	Move m1;
	Position b;

	srandom(getpid());

	lprintf(0,"slave %d started\n", slave_num);

	while (1) {
		while (slave_out.index == slave_in.index || 
		       slave_in.index == 0) {
			usleep(1000);
			slave_in = state->slavep[slave_num];
		}

		slave_out = slave_in;

		b = state->position;

		b.moves = NULL;
		b.num_moves = 0;
		root_moves(&b, &slave_in.move);

		order_clear(b.move_num);
		hash_change_tag(b.move_num);

		v = abroot(&b, slave_out.depth, slave_out.testv, &m1);

		slave_in = state->slavep[slave_num];

		if (search_expired())
			continue;

		slave_out.v = v;
		slave_out.nodes = nodes;
		slave_out.mo_hits = move_order_hits;
		slave_out.mo_misses = move_order_misses;
		slave_out.maxply = maxply;
		slave_out.move = m1;
		slave_out.done = 1;
		state->slavep[slave_num] = slave_out;

		nodes = 0;
		move_order_hits = move_order_misses = 0;
		maxply = 0;
	}
}

static Eval next_to_search(Eval v,struct slave *slaves, 
			   Eval alpha, Eval beta, int n)
{
	int i;
	int j, gap;
	Eval y;
	int maxwidth = 5;

	if (v == alpha) {
		v += MTD_THRESHOLD + imin((beta - alpha)/10, maxwidth);
	} else if (v == beta) {
		v -= (MTD_THRESHOLD-1) + imin((beta - alpha)/10, maxwidth);
	}
	
	gap = MTD_THRESHOLD/4;

	while (gap > 0) {
		i = 0;

		while (v+i <= beta || v-i > alpha) {
			y = v+i*gap;
			
			if (y <= beta) {
				for (j=0;j<n;j++)
					if (slaves[j].busy && 
					    slaves[j].testv == y) break;
				if (j == n) return y;
			}
		
			y = v-i*gap;
		
			if (y >= alpha) {
				for (j=0;j<n;j++)
					if (slaves[j].busy && slaves[j].testv == y) break;
				if (j == n) return y;
			}
			
			i++;
		}
		gap--;
	}

	return v;
}


static Eval smp_search(Position *b, Move *move, int depth, Eval guess)
{
	int n;
	Eval v, g, alpha, beta;
	static unsigned par_index;
	struct slave slaves[MAX_CPUS];
	int all_busy;

	/* generate the move list */
	root_moves(b, move);

	hash_change_tag(b->move_num);

	slave_in.index = 0;

	for (n=0;n<state->num_cpus;n++) {
		state->slavep[n] = slave_in;
	}

	usleep(1000);

	alpha = -INFINITY;
	beta = INFINITY;
	
	nodes = 0;

	memset(slaves, 0, sizeof(slaves));

	v = guess;

	while (alpha < beta-MTD_THRESHOLD) {
		if (search_expired())
			return v;

		/* find a spare processor */
		for (n=0;n<state->num_cpus;n++)
			if (!slaves[n].busy) break;

		if (n < state->num_cpus) {
			/* assign it to the closest value to v that is not
			   being searched */
			slaves[n].testv = next_to_search(v, slaves, 
							 alpha, beta,
							 state->num_cpus);
			slaves[n].index = ++par_index;
			slaves[n].depth = depth;
			slaves[n].busy = 1;
			slaves[n].done = 0;
			slaves[n].move = (*move);
			state->slavep[n] = slaves[n];	

			continue;
		}

		/* they are all busy - wait for one to
		   finish */
		all_busy = 1;
		while (all_busy) {
			for (n=0;n<state->num_cpus;n++)
				if (state->slavep[n].done) {
					all_busy = 0;
					break;
				}
			if (search_expired()) return v;
			if (all_busy) usleep(1);
		}
		
		slave_in = state->slavep[n];
		
		if (slaves[n].index != slave_in.index || slave_in.index == 0) {
			slaves[n].busy = 0;
			continue;
		}

		nodes += slave_in.nodes;
		move_order_hits += slave_in.mo_hits;
		move_order_misses += slave_in.mo_misses;
		maxply = imax(maxply, slave_in.maxply);

		g = slave_in.v;

		if (g < slave_in.testv) {
			if (g < beta) {
				v = beta = g-1;
			}
		} else {
			if (g >= alpha) {
				v = alpha = g+1;
				(*move) = slave_in.move;
			}
		}


#if 0
		lprintf(0,"slave(%d) depth=%d testv=%e g=%e move=%s alpha=%e beta=%e\n",
			n,
			slave_in.depth,
			slave_in.testv, g, 
			short_movestr(b, &slave_in.move),
			alpha, beta);
#endif

		slaves[n].busy = 0;


		/* abort any processors that are searching out of window */
		for (n=0;n<state->num_cpus;n++)
			if (slaves[n].busy==1 && 
			    (slaves[n].testv < alpha || slaves[n].testv > beta)) {
				slaves[n].busy = 0;
				state->slavep[n].index = 0;
			}
	}

	return v;
}
#endif


#if !APLINUX
static int mtd_converged(Position *b, int alpha, int beta, int depth, Eval *y)
{
	int m;
	int count=0, lower, upper;
	struct hash_entry *t;
	int count_alpha=0;

	/* its converged if only one move has an upper limit
	   on its evaluation of >= alpha */
	
	if (!b->moves) {
		return 0;
	}

	for (m=0;m<b->num_moves;m++) {
		Position b1;

		if (!do_move(&b1, b, &b->moves[m])) {
			lprintf(0, "illegal move %s in mtd_converged\n",
				short_movestr(b, &b->moves[m]));
			continue;
		}
		
		t = fetch_hash(&b1);

		if (!t) {
			return 0;
		}

		if (t->depth_high < (depth-1)) {
			return 0;
		}

		upper = (-EV(t->low));
		lower = (-EV(t->high));

		if (lower > -INFINITY)
			count_alpha++;

#if 0
		if (lower >= alpha && upper <= beta && lower < upper)
			y->v = (lower+upper+1)/2;
#endif

		if (upper > alpha) 
			count++;
	}

	if (count == 1)
		return 1;

	return 0;
}

static int converged;
static int small_window;

static int mtd_count;

static Eval mtd_search(Position *b, Move *move, int depth, 
		       Eval guess)
{
	Eval alpha, beta, y, g;
	int loops=0;
	int alpha_count=0;
	int beta_count=0;

	alpha = makeeval(b, ILLEGAL);
	beta = makeeval(b, -ILLEGAL);
	g = guess;

	converged = 0;
	small_window = 0;
	mtd_count = 0;

	/* generate the move list */
	root_moves(b, move);

	/* try to get a lower bound from the hash table */
#if USE_ETTC_HASH
	if (!is_zero_move(move) &&
	    ettc_check_hash(b, b->moves, b->num_moves,
			    depth, makeeval(b, ILLEGAL), &alpha, NULL)) {
		g = alpha;
	}
#endif

	while (EV(alpha) == ILLEGAL && EV(beta) > EV(alpha)) {
		static float offsets[7] = {
			0.5, 2, 6, 13, 30, 30, 30};
		if (loops > 6) break;
		EV(y) = EV(g) - offsets[loops]*PAWN_VALUE;

		if (EV(y) <= EV(alpha))
			EV(y) = EV(alpha) + 0.01*PAWN_VALUE;

		if (EV(y) >= EV(beta))
			EV(y) = EV(beta) - 0.01*PAWN_VALUE;

		g = abroot(b, depth, y, move);

		mtd_count++;
		loops++;

		if (search_expired()) {
			if (EV(alpha) != ILLEGAL)
				return alpha;
			return guess;
		}

#if 0
		lprintf(0,"mtd g=%e y=%e alpha=%e beta=%e nodes=%d %s\n",
			EV(g), EV(y), EV(alpha), EV(beta), nodes,
			short_movestr(b, move));
#endif
		if (EV(g) < EV(y)) {
			beta = g;
		} else {
			alpha = g;
		}
	}

	if (!is_zero_move(move) && EV(alpha) == EV(beta)) {
		small_window = 1;
		return alpha;
	}

	/* we've now got a fail high, so we have a lower limit on our eval
	   and a likely best move. */
	if (mtd_count == 1 && EV(g) < EV(guess)) {
		EV(g) = emax(EV(guess) - 0.5*MTD_THRESHOLD, EV(g));
	}

	/* if we got something from the hash table then use that to
	   prime g */
	if (mtd_count == 0) {
		EV(g) += MTD_THRESHOLD;
	}
	

	while (EV(beta) > EV(alpha) || is_zero_move(move)) {
		loops++;
		y = g;
		if (EV(g) == EV(alpha)) {
			alpha_count++;
			beta_count=0;
			EV(y) += alpha_count*alpha_count*MTD_THRESHOLD;
			EV(y) = emin(EV(y), (EV(alpha) + EV(beta))/2);
		} else if (EV(g) == EV(beta)) {
			alpha_count=0;
			beta_count++;
			EV(y) -= beta_count*beta_count*MTD_THRESHOLD;
			EV(y) = emax(EV(y), (EV(alpha) + EV(beta))/2);			
		}

		if (EV(alpha) >= EV(beta) - MTD_THRESHOLD)
			small_window = 1;

		if (EV(alpha) > ILLEGAL && EV(beta) < -ILLEGAL &&
		    mtd_converged(b, EV(alpha), EV(beta), depth, &y))
			break;

		if (EV(y) <= EV(alpha))
			EV(y) = EV(alpha) + 1;

		if (EV(y) > EV(beta))
			EV(y) = EV(beta);

		g = abroot(b, depth, y, move);

#if 0
		lprintf(0,"mtd g=%e y=%e alpha=%e beta=%e nodes=%d %s\n",
			EV(g), EV(y), EV(alpha), EV(beta), nodes,
			short_movestr(b, move));
#endif
		mtd_count++;

		if (search_expired()) {
			if (EV(alpha) != ILLEGAL)
				return alpha;
			return guess;
		}

		if (EV(g) < EV(y)) {
			beta = g;
		} else {
			alpha = g;
		}

		if (EV(alpha) >= EV(beta) - MTD_THRESHOLD)
			small_window = 1;
#if 0
		lprintf(0,"mtd g=%e y=%e alpha=%e beta=%e nodes=%d %s\n",
			EV(g), EV(y), EV(alpha), EV(beta), nodes,
			short_movestr(b, move));
#endif
		if (EV(alpha) > WIN || EV(beta) < -WIN) {
			converged = 1;
			break;
		}

		if (EV(alpha) >= EV(beta) - MTD_THRESHOLD &&
		    (mtd_count > 30 || 
		     (abs(EV(alpha)) < FORCED_WIN && !mulling))) {
			small_window = 1;
			break;
		}

	}

	if (EV(alpha) == ILLEGAL)
		alpha = g;

	return alpha;
}
#endif

Eval search_one_depth(Position *b, Move *move, 
		      int depth, Eval guess)
{
#if APLINUX
	return par_search(b, move, depth, guess);
#elif USE_SMP
	int n;
	for (n=0;n<state->num_cpus;n++)
		state->slavep[n].index = 0;
	guess = smp_search(b, move, depth, guess);
	for (n=0;n<state->num_cpus;n++)
		state->slavep[n].index = 0;
	return guess;
#else
	return mtd_search(b, move, depth, guess);
#endif
}

static void show_pv(Position *b, Move *move)
{
	Move m1;
	int i=0;
	struct hash_entry *t;
	Position b1;

	lprintf(0,"PV: %s ", short_movestr(b, move));

	do_move(&b1, b, move);

	for (i=1;i<MAX_DEPTH;i++) {
		t = fetch_hash(&b1);
		if (!t) break;
		m1.from = t->from;
		m1.to = t->to;
		if (is_zero_move(&m1)) break;
		lprintf(0,"%s ", 
			short_movestr(&b1, &m1));
		if (!do_move(&b1, &b1, &m1)) break;
	}

	lprintf(0,"\n");
}

static void show_eval(Position *b, Eval v, int edepth)
{
#if LEARN_EVAL
	int m = state->stored_move_num;
	Position b1;
	Eval v1;
	float f1,f2;
	
	state->leaf_eval[m].flags = 0;

	/* sometimes we get an empty board (!) */
	if (!v.pos.flags) 
		return;

	/* can't do derivatives for brain entries */
	if (v.pos.flags & FLAG_BRAIN_EVAL) {
		td_unusable_eval(&v);
		return;
	}

	/* occasionally we get a futility node here */
	if (v.pos.flags & FLAG_FUTILE) {
		td_unusable_eval(&v);
		return;
	}
	/* forced draws cause wild oscillations in evaluation */
	if (v.pos.flags & FLAG_FORCED_DRAW) {
		td_unusable_eval(&v);
		return;
	}

	/* leaf position is irrelevant for forced mate */
	if  (EV(v) < -FORCED_WIN || EV(v) > FORCED_WIN) {
		td_unusable_eval(&v);
		return;
	}
		

	memset(&b1, 0, sizeof(b1));
	memcpy(b1.board, v.pos.board, sizeof(b1.board));
	b1.move_num = v.pos.move_num;
	b1.stage = v.pos.stage;
	b1.flags = v.pos.flags;
	b1.enpassent = v.pos.enpassent;
	b1.fifty_count = v.pos.fifty_count;

	create_pboard(&b1);

	v1 = eval(&b1, INFINITY, MAX_DEPTH);


	lprintf(0,"eval: %e %e flags=%08x move=%d hash1=%08x\n %s\n", 
		EV(v)*next_to_play(b), 
		EV(v1)*next_to_play(&b1),
		v.pos.flags,
		v.pos.move_num,
		b1.hash1,
		position_to_ppn(&b1));

	f1 = tanh(EV(v)*EVAL_SCALE*next_to_play(b));
	f2 = tanh(EV(v1)*EVAL_SCALE*next_to_play(&b1));

	if (ABS(f1-f2) < EVAL_DIFF) {
		td_gradient(&b1);
	} else {
		lprintf(0,"***EVAL DIFF***\n %e %e\n", 
			EV(v)*next_to_play(b), EV(v1)*next_to_play(&b1));
		print_board(v.pos.board);
		eval_debug(&b1);
	}
#endif
}

static int terrible_move(Position *b, Move *move, Eval *v, int depth)
{
	struct hash_entry *t;
	Move m1;
	Position b1;

	do_move(&b1, b, move);
	t = fetch_hash(&b1);
	if (!t) return 0;

	m1.from = t->from;
	m1.to = t->to;

	if (EV(t->high) > PAWN_VALUE-EV(*v)) {
		lprintf(0,"unbounded: %s %e low=%e high=%e %s\n",
			short_movestr(b, move), EV(*v), EV(t->low), EV(t->high),
			short_movestr(&b1, &m1));
		EV(*v) = (-EV(t->low)) - 2;
		return 1;
	}

	return 0;
}

#if USE_SMP
void fork_slaves(void)
{
	int i;

	for (i=0;i<state->num_cpus;i++) {
		if ((slave_pids[i] = fork()) == 0) {
			slave_num = i;
			smp_slave();
		}
	}
	
	lprintf(0,"forked %d slaves\n", i);
}
#endif

Eval search(Position *b, Move *move)
{
	extern int display_pid;
	Eval v, lastv;
	int edepth;
	int depth;
	int terminate=0;
	Eval guess, v1, lastv1;
	int expired, force;
	int counter;
	struct hash_entry *t;

#if USE_SMP
	if (!forked) {
		forked = 1;
		fork_slaves();
	}
#endif

	nodes = maxply = maxdepth = 0;
	quiesce_nodes = 0;
	confirm_nodes = 0;
	extended_nodes = 0;
	truncated_nodes = 0;
	move_order_misses = move_order_hits = 0;
	hash_reset_stats();
	
	estimate_game_stage(b, 1);

	EV(v) = EV(lastv) = 0;

	order_clear(b->move_num);
	hash_change_tag(b->move_num);

	depth = 2;

	t = fetch_hash(b);
	if (t) {
		depth = imax(t->depth_low, depth);
		v = t->low;
		move->from = t->from;
		move->to = t->to;
		if (t->depth_high >= t->depth_low &&
		    EV(t->high) <= EV(t->low) + MTD_THRESHOLD)
			depth++;
		if (is_zero_move(move)) depth = 2;
	}

	edepth = depth-1;

	while (!terminate && depth < MAX_DEPTH) {
		if (pondering && ponder_stop)
			break;

		if (!process_exists(display_pid)) 
			break;

		guess = v;
		lastv = v;

		counter = 0;

	again:
		counter++;
		v = search_one_depth(b, move, depth, guess);
		
		expired = (nodes > TIMER_NODES && timer_expired());
		if (pondering)
			expired = 0;

		force = 0;
		if (b->num_moves == 1) {
			force = 1;
		}

		if (!pondering && !force && expired && 
		    !converged &&
		    terrible_move(b, move, &v, depth)) {
			if (counter < 4 && timer_extend()) {
				guess = v;
				lprintf(0,"overtime (guess=%e)\n", EV(guess));
				goto again;
			}
		}

		if (pondering || edepth <= 2) {
			terminate = 0;
		} else {
			terminate = timer_terminate(depth, next_to_play(b), force);
		}

		if (!search_expired()) {
			edepth = depth;
		} else {
			terminate = 1;
		}

		lprintf(0, "%s%s: %s d=%d n=%d mtd=%d t=%.1f %s\n", 
			pondering?"** ":"",
			colorstr(next_to_play(b)), 
			evalstr(EV(v)),
			edepth,
			nodes, 
			mtd_count,
			timer_elapsed(),
			short_movestr(b, move));


		if (!mulling && next_to_play(b) != state->computer) {
			v1 = v;
			EV(v1) = -EV(v1);
			lastv1 = lastv;
			EV(lastv1) = -EV(lastv1);
		} else {
			v1 = v;
			lastv1 = lastv;
		}

		if (state->ics_robot && edepth > 3 && !mulling) {
			if (!pondering && EV(v1) > EV(lastv1) + 4*PAWN_VALUE && 
			    EV(lastv1) < 3*PAWN_VALUE && EV(v1) > -2*PAWN_VALUE) {
				prog_printf("%s Eval=%s yeah!\n", ICS_WHISPER,
					    evalstr(EV(v1)));
			} else if (!pondering && EV(lastv1) < 0.9*PAWN_VALUE && EV(v1) > 1.3*PAWN_VALUE) {
				prog_printf("%s Eval=%s looking good\n", ICS_WHISPER,
					    evalstr(EV(v1)));
			} else if (EV(v1) < EV(lastv1) - 0.9*PAWN_VALUE && EV(lastv1) < 0) {
				prog_printf("%s Eval=%s uh oh\n", ICS_WHISPER,
					    evalstr(EV(v1)));
			} else if (EV(v1) < EV(lastv1) - 4*PAWN_VALUE && EV(lastv1) < 4*PAWN_VALUE) {
				prog_printf("%s Eval=%s damn\n", ICS_WHISPER,
					    evalstr(EV(v1)));
			}
		}

		if (EV(v) <= -(WIN-edepth) || EV(v) >= (WIN-edepth))
			terminate = 1;

		state->nps = (nodes+quiesce_nodes)/timer_elapsed();

		if (terminate && !pondering && state->ics_robot &&
		    state->move_time >= 1 && !mulling) {
			prog_printf("%s Eval=%s nps=%d depth=%d cpu=%.1f%%\n",
				    ICS_WHISPER,
				    evalstr(EV(v1)), 
				    state->nps,
				    edepth,
				    cpu_percent());
		}
		
		if (terminate && !pondering && edepth > 0) {
			show_pv(b, move);
			lprintf(0,"hash stats: %s  mo=%d%%\n", 
				hashstats(),
				(100*move_order_hits)/(move_order_misses+move_order_hits+1));
			lprintf(0,"c=%d%% q=%d%% ext=%d%% trunc=%d%% nps=%d cpu=%.1f%%\n",
				(100*confirm_nodes)/(nodes+1),
				(100*quiesce_nodes)/(nodes+1),
				(100*extended_nodes)/(nodes+1),
				(100*truncated_nodes)/(nodes+1),
				state->nps,
				cpu_percent());
#if LEARN_EVAL
			if (!mulling) {
				if (EV(v) > -FORCED_WIN || EV(v) < FORCED_WIN) {
					if (small_window) {
						show_eval(b,v,edepth);
					} else if (EV(lastv) != 0) {
						show_eval(b,lastv,edepth);
					}
				}
			}
#endif
		}

		depth++;
	}

#if USE_PBRAIN
	if (edepth > 2)
		brain_insert(b, move);
#endif

	return v;
}

static int find_winner(Position *b, Move *move)
{
	static Move moves[MAX_MOVES];
	int n, i;
	int bkpos = BLACKPIECES(b)[IKING].pos;
	int wkpos = WHITEPIECES(b)[IKING].pos;
	Position b1;

	b1 = (*b);

	if (whites_move(b) && (b1.topieces[bkpos] & WHITE_MASK)) {
		lprintf(0,"black is mated\n");
		b->winner = 1;
		return 1;
	}

	if (blacks_move(b) && (b1.topieces[wkpos] & BLACK_MASK)) {
		lprintf(0,"white is mated\n");
		b->winner = -1;
		return 1;
	}

	if (check_repitition(b, 3)) {
		lprintf(0,"claim a draw\n");
		if (!mulling)
			prog_printf("draw\n");
		b->winner = STALEMATE;
		return 1;
	}

	if (move && legal_move(b, move)) {
		do_move(&b1, b, move);
	}

	if (check_repitition(&b1, 3)) {
		lprintf(0,"claim a draw\n");
		b->winner = STALEMATE;
		return 1;
	}
	
	if (((b->material_mask & WHITE_MASK) == (b->piece_mask & WHITE_MASK) && 
	     b->w_material <= BISHOP_VALUE)   &&
	    ((b->material_mask & BLACK_MASK) == (b->piece_mask & BLACK_MASK) && 
	     b->b_material <= BISHOP_VALUE)) {
		/* both white and black have insufficient material to mate */
		lprintf(0,"insufficient material on either side\n");
		if (!mulling)
			prog_printf("draw\n");
		b->winner = STALEMATE;
		return 1;
	}


	n = generate_moves(&b1, moves);
	for (i=0;i<n;i++)
		if (legal_move(&b1, &moves[i]))
			return 0;

	if (whites_move(&b1) && (b1.topieces[wkpos] & BLACK_MASK)) {
		lprintf(0,"white is mated\n");
		b->winner = -1;
		return 1;
	}

	if (blacks_move(&b1) && (b1.topieces[bkpos] & WHITE_MASK)) {
		lprintf(0,"black is mated\n");
		b->winner = 1;
		return 1;
	}

	b->winner = STALEMATE;
	return 1;
}


/* this is the entry point for the chess program proper */
int make_move(Position *b, Move *move)
{
	Position b1;
	Eval bestv, v1;

	srandom(time(NULL));

#if 0
	printf("Position is %d:%d:%d bytes\n", 
	       (char *)&b1.part1_marker - (char *)&b1,
	       (char *)&b1.dont_copy - (char *)&b1.part1_marker,
	       sizeof(b1));
#endif

	if (next_to_play(b) == 1)
		b->flags |= FLAG_COMPUTER_WHITE;
	else 
		b->flags &= ~FLAG_COMPUTER_WHITE;

	ponder_stop = 0;
	state->stop_search = 0;
	b->winner = 0;

	zero_move(move);

	init_hash_table();

	if (find_winner(b, NULL))
		return 0;
	regen_moves(b);
	v1 = eval(b, INFINITY, MAX_DEPTH);

	b1 = (*b);

	timer_start(next_to_play(&b1));
	
	if (state->move_time > 0.5 || b->stage == OPENING)
		brain_fill_hash(&b1);

	lprintf(0,"searching position: %s\n", position_to_ppn(b));

#if USE_PBRAIN
	if (brain_lookup(b, move, &bestv)) {
		if (learning && !mulling) {
#if LEARN_EVAL
			td_unusable_eval(&bestv);
#endif
		}
		goto got_move;
	}
#endif

	bestv = search(&b1, move);

got_move:
	if (b1.hash1 == b->hash1) {
		b->evaluation = EV(bestv) * next_to_play(&b1);

		/* don't carry on when its hopeless */
#ifndef NO_RESIGN
		if (state->ics_robot && 
		    EV(bestv) < -RESIGN_VALUE && EV(v1) < -RESIGN_VALUE) {
			lprintf(0,"thinking about resigning\n");
			b->winner = -next_to_play(b);
			if (!mulling && state->autoplay)
				prog_printf("resign\n");
		}
#endif
	}

	move->v = b->evaluation;

	timer_off();

	find_winner(b, move);

	return 1;
}


/* this is used to make KnightCap think on the oppenents time */
void ponder_move(Position *b, Move *move)
{
	Position b1;
	Eval bestv, v1;
	Move m1;

	state->pondering = 1;

	if (next_to_play(b) == -1)
		b->flags |= FLAG_COMPUTER_WHITE;
	else 
		b->flags &= ~FLAG_COMPUTER_WHITE;

	zero_move(move);

	real_position = b;
	ponder_position = &b1;

	ponder_stop = 0;
	state->stop_search = 0;
	b->winner = 0;

	init_hash_table();

	if (find_winner(b, NULL)) {
		state->pondering = 0;
		return;
	}

	regen_moves(b);

	b1 = (*b);

	ponder_stop = 0;

	lprintf(0,"pondering position: %s\n", position_to_ppn(b));

	timer_off();

	zero_move(&m1);

	if (!check_hash(&b1, 0, makeeval(&b1, ILLEGAL), &v1, &m1) || is_zero_move(&m1)) {
		lprintf(0,"searching for move to ponder\n");
		search_one_depth(&b1, &m1, 2, makeeval(b, 0));
	}

	if (is_zero_move(&m1) || !legal_move(&b1, &m1) ||
	    !do_move(&b1, &b1, &m1)) {
		lprintf(0,"no move to ponder\n");
		state->pondering = 0;
		return;
	}

	lprintf(0,"assuming move %s\n", short_movestr(b, &m1));

	if (brain_lookup(&b1, &m1, &bestv)) {
		zero_move(move);
		state->pondering = 0;
		return;		
	}

	timer_start(next_to_play(&b1));

	brain_fill_hash(&b1);
	
	pondering = 1;
	v1 = eval(&b1, INFINITY, MAX_DEPTH);
	bestv = search(&b1, move);
	pondering = 0;

	timer_off();

	if (b1.hash1 != b->hash1) {
		zero_move(move);
		state->pondering = 0;
		return;
	}

	lprintf(0,"pondered right move\n");
#if STORE_LEAF_POS
	state->predicted_move[state->stored_move_num] = 1;
#endif

	b->evaluation = EV(bestv) * next_to_play(&b1);

	move->v = b->evaluation;

	/* don't carry on when its hopeless */
#ifndef NO_RESIGN
	if (state->ics_robot && state->autoplay &&
	    EV(bestv) < -RESIGN_VALUE && EV(v1) < -RESIGN_VALUE) {
		lprintf(0,"time to resign\n");
		b->winner = -next_to_play(b);
		if (!mulling)
			prog_printf("resign\n");
	}
#endif

	find_winner(b, move);
	state->pondering = 0;
}


