/* this is a set of "mini quiesce" routines that try to statically
   resolve the immediate tactics of a position as quickly as possible. They
   only take account of material */

#include "includes.h"
#include "knightcap.h"


#define UNCERTAINTY 0.5
#define OFFSET (PAWN_VALUE/2)

extern int debug;

#if 0
#define MAX_UNDO 1000

static uint32 undo_stack[MAX_UNDO];
static int undo_ptr;

static int mini_do_move(Position *b, Move *m)
{
	int undo_index = undo_ptr;
	undo_stack[undo_ptr++] = b->pboard[m->to];

	b->pieces[b->pboard[m->from]].pos = m->to;
	b->pboard[m->to] = b->pboard[m->from];
	b->pboard[m->from] = 0;
	b->board[m->to] = b->board[m->from];
	b->board[m->from] = 0;

	if (b->board[m->to] > 0) {
		b->w_material -= mat_value(b->board[m->to]);
	} else if (b->board[m->to] < 0) {
		b->b_material -= mat_value(-b->board[m->to]);
	}

#if 0
	/* moving this piece might expose new capture
	   possibilites by sliding pieces. To find
	   these we look at all the sliding pieces
	   that can get to this square and slide along those
	   lines to change the topieces arrays */
	pinnerlist = b->topieces[oldpos] & b->sliding_mask & ~donelist;
	while (pinnerlist) {
		k = ff_one(pinnerlist);
		pinnerlist &= ~(1<<k);
		
		dir = sliding_map[b->pieces[k].pos][oldpos];
		p2 = b->pieces[k].pos + dir;
		while (!b->board[p2] && !off_board(p2, dir)) {
			p2 += dir;
		}
		if (b->board[p2]) {
			save_pos[num_saved] = p2;
			save_mask[num_saved] = b->topieces[p2];
			num_saved++;
			b->topieces[p2] |= (1<<k);
			if (!same_color(b->board[p2],
					b->pieces[k].p)) {
				caplist |= (1<<b->pboard[p2]);
			}
		}
	}
#endif

	return undo_index;
}

static int mini_undo_move(Position *b, Move *m, int undo_index)
{

	b->board[m->from] = b->board[m->to];
	b->pboard[m->from] = b->pboard[m->to];
	b->pboard[m->to] = undo_stack[undo_index++];
	b->pieces[b->pboard[m->from]].pos = m->from;
	b->board[m->to] = b->pieces[b->pboard[m->to].p];

	if (b->board[m->to] > 0) {
		b->w_material += mat_value(b->board[m->to]);
	} else if (b->board[m->to] < 0) {
		b->b_material += mat_value(-b->board[m->to]);
	}
}
#endif

static int mini_quiesce_b(Position *b, 
			  uint32 caplist, 
			  uint32 donelist,
			  int beta, int ply);

/* a mini quiesce search which looks only at material. The caplist is
   a bitmask of pieces that we should consider capturing. The donelist
   is a list of pieces that have already been captured. beta is the
   normal alpha-beta cutoff that the search is aiming for 

   This is the white side of the search. We don't use a nega search because
   of the additional conditionals needed.
*/
static int mini_quiesce_w(Position *b, 
			  uint32 caplist, 
			  uint32 donelist,
			  int beta, int ply)
{
	uint32 topieces, pinnerlist;
	int i, j, k, v, v3, bestv, dir;
	Square p2;

	/* perhaps we've already reached the goal */
	v = b->w_material - b->b_material;
	if (v >= beta) {
		if (debug) {
			lindent(0, ply);
			lprintf(0,"w reached goal %d beta=%d\n", v, beta);
		}
		return v;
	}

	bestv = v;

	while (caplist & BLACK_MASK) {
		i = ff_one(caplist & BLACK_MASK);
		caplist &= ~(1<<i);

		/* a simple futility check. Note that we can exit the loop
		   completely as the piece lists are ordered */
		v3 = v + mat_value(-b->pieces[i].p); 
		if (v3 < beta) {
			if (debug) {
				lindent(0, ply);
				lprintf(0,"w futile %s v2=%d bestv=%d beta=%d\n", 
					posstr(b->pieces[i].pos), 
					v + mat_value(-b->pieces[i].p),
					bestv, beta);
					
			}
			if (v3 > bestv)
				bestv = v3;
			return bestv;
		}

		/* we can capture using any piece that hasn't already 
		   been moved */
		topieces = b->topieces[b->pieces[i].pos] & ~donelist & WHITE_MASK;

		while (topieces) {
			Square oldpos;
			uint32 oldcaplist;
			int num_saved = 0;
			Square save_pos[8];
			uint32 save_mask[8];

			/* capture with the smallest piece first */
			j = fl_one(topieces);
			topieces &= ~(1<<j);

			oldpos = b->pieces[j].pos;
			oldcaplist = caplist;
			b->pieces[j].pos = b->pieces[i].pos;
			b->b_material -= mat_value(-b->pieces[i].p);
			b->board[b->pieces[j].pos] = b->pieces[j].p;

			/* moving this piece might expose new capture
			   possibilites by sliding pieces. To find
			   these we look at all the sliding pieces
			   that can get to this square and slide along those
			   lines to change the topieces arrays */
			pinnerlist = b->topieces[oldpos] & b->sliding_mask & ~donelist & ~(1<<i);
			while (pinnerlist) {
				k = ff_one(pinnerlist);
				pinnerlist &= ~(1<<k);

				dir = sliding_map[b->pieces[k].pos][oldpos];
				p2 = b->pieces[k].pos + dir;
				while (!b->board[p2] && !off_board(p2, dir)) {
					p2 += dir;
				}
				if (b->board[p2]) {
					save_pos[num_saved] = p2;
					save_mask[num_saved] = b->topieces[p2];
					num_saved++;
					b->topieces[p2] |= (1<<k);
					if (!same_color(b->board[p2],
							b->pieces[k].p)) {
						caplist |= (1<<b->pboard[p2]);
					}
				}
			}

			if (debug) {
				lindent(0, ply);
				lprintf(0,"w %s%s\n", posstr(oldpos), posstr(b->pieces[i].pos));
			}

			v3 = -mini_quiesce_b(b, 
					     caplist | (1<<j),
					     donelist | (1<<i) | (1<<j),
					     -(beta-1), ply+1);

			b->board[b->pieces[j].pos] = b->pieces[i].p;
			b->b_material += mat_value(-b->pieces[i].p);
			b->pieces[j].pos = oldpos;
			caplist = oldcaplist;
			
			while (num_saved--) {
				b->topieces[save_pos[num_saved]] = save_mask[num_saved];
			}

			if (v3 > bestv) {
				bestv = v3;
			}

			if (bestv >= beta) {
				if (debug) {
					lindent(0, ply);
					lprintf(0,"w %s%s -> %d beta=%d\n", 
						posstr(oldpos), posstr(b->pieces[i].pos),
						bestv, beta);
				}
				if (ply == 0) {
					b->best_capture.from = oldpos;
					b->best_capture.to = b->pieces[i].pos;
				}
				return bestv;
			}
		}
	}

	if (debug) {
		lindent(0, ply);
		lprintf(0,"w -> %d beta=%d\n", 
			bestv, beta);
	}
	return bestv;
}


static int mini_quiesce_b(Position *b, 
			  uint32 caplist, 
			  uint32 donelist,
			  int beta, int ply)
{
	uint32 topieces, pinnerlist;
	int i, j, k, v, v3, bestv, dir;
	Square p2;

	/* perhaps we've already reached the goal */
	v = b->b_material - b->w_material;
	if (v >= beta) {
		if (debug) {
			lindent(0, ply);
			lprintf(0,"b reached goal %d beta=%d\n", v, beta);
		}
		return v;
	}

	bestv = v;

	while (caplist & WHITE_MASK) {
		i = ff_one(caplist & WHITE_MASK);
		caplist &= ~(1<<i);

		/* a simple futility check. Note that we can exit the loop
		   completely as the piece lists are ordered */
		v3 = v + mat_value(b->pieces[i].p); 
		if (v3 < beta) {
			if (debug) {
				lindent(0, ply);
				lprintf(0,"b futile %s v2=%d bestv=%d beta=%d\n", 
					posstr(b->pieces[i].pos), 
					v + mat_value(b->pieces[i].p),
					bestv, beta);
					
			}
			if (v3 > bestv)
				bestv = v3;
			return bestv;
		}

		/* we can capture using any piece that hasn't already 
		   been moved */
		topieces = b->topieces[b->pieces[i].pos] & ~donelist & BLACK_MASK;

		while (topieces) {
			Square oldpos;
			uint32 oldcaplist;
			int num_saved = 0;
			Square save_pos[8];
			uint32 save_mask[8];

			/* capture with the smallest piece first */
			j = fl_one(topieces);
			topieces &= ~(1<<j);

			oldpos = b->pieces[j].pos;
			oldcaplist = caplist;
			b->pieces[j].pos = b->pieces[i].pos;
			b->w_material -= mat_value(b->pieces[i].p);
			b->board[b->pieces[j].pos] = b->pieces[j].p;

			/* moving this piece might expose new capture
			   possibilites by sliding pieces. To find
			   these we look at all the sliding pieces
			   that can get to this square and slide along those
			   lines to change the topieces arrays */
			pinnerlist = b->topieces[oldpos] & b->sliding_mask & ~donelist & ~(1<<i);
			while (pinnerlist) {
				k = ff_one(pinnerlist);
				pinnerlist &= ~(1<<k);

				dir = sliding_map[b->pieces[k].pos][oldpos];
				p2 = b->pieces[k].pos + dir;
				while (!b->board[p2] && !off_board(p2, dir)) {
					p2 += dir;
				}
				if (b->board[p2]) {
					save_pos[num_saved] = p2;
					save_mask[num_saved] = b->topieces[p2];
					num_saved++;
					b->topieces[p2] |= (1<<k);
					if (!same_color(b->board[p2],
							b->pieces[k].p)) {
						caplist |= (1<<b->pboard[p2]);
					}
				}
			}

			if (debug) {
				lindent(0, ply);
				lprintf(0,"b %s%s\n", posstr(oldpos), posstr(b->pieces[i].pos));
			}

			v3 = -mini_quiesce_w(b, 
					     caplist | (1<<j),
					     donelist | (1<<i) | (1<<j),
					     -(beta-1), ply+1);

			b->board[b->pieces[j].pos] = b->pieces[i].p;
			b->w_material += mat_value(b->pieces[i].p);
			b->pieces[j].pos = oldpos;
			caplist = oldcaplist;
			
			while (num_saved--) {
				b->topieces[save_pos[num_saved]] = save_mask[num_saved];
			}

			if (debug) {
				lindent(0, ply);
				lprintf(0,"b %s%s -> %d beta=%d\n", 
					posstr(oldpos), posstr(b->pieces[i].pos),
					v3, beta);
			}

			if (v3 > bestv) {
				bestv = v3;
			}
			
			if (bestv >= beta) {
				if (debug) {
					lindent(0, ply);
					lprintf(0,"b %s%s -> %d beta=%d\n", 
						posstr(oldpos), posstr(b->pieces[i].pos),
						bestv, beta);
				}
				if (ply == 0) {
					b->best_capture.from = oldpos;
					b->best_capture.to = b->pieces[i].pos;
				}
				return bestv;
			}
		}
	}

	if (debug) {
		lindent(0, ply);
		lprintf(0,"b -> %d beta=%d\n", 
			bestv, beta);
	}
	return bestv;
}


etype eval_tactics(Position *b)
{
	etype v, v2, bestv, ret=0;

	if (b->flags & FLAG_DONE_TACTICS) {
		return b->tactics;
	}

	zero_move(&b->best_capture);

	if (whites_move(b)) {
		bestv = v = (b->w_material - b->b_material);
		v2 = mini_quiesce_w(b, b->material_mask, 0, 
				    bestv + OFFSET, 0);
		if (v2 >= v + OFFSET) {
			ret = 2*THREAT;
			goto evaluated;
		}

		v = (b->b_material - b->w_material);
		v = mini_quiesce_b(b, b->material_mask, 0, 
				   OFFSET + v, 0) - v;
		if (v >= OFFSET) {
			ret = -THREAT;
			goto evaluated;
		}
	} else {
		bestv = v = (b->b_material - b->w_material);
		v2 = mini_quiesce_b(b, b->material_mask, 0, 
				    bestv + OFFSET, 0);
		if (v2 >= v + OFFSET) {
			ret = -2*THREAT;
			goto evaluated;
		}

		v = (b->w_material - b->b_material);
		v = mini_quiesce_w(b, b->material_mask, 0, 
				   OFFSET + v, 0) - v;

		if (v >= OFFSET) {
			ret = THREAT;
			goto evaluated;
		}
	}


evaluated:
	b->flags |= FLAG_DONE_TACTICS;
	b->tactics = ret;

	return ret;
}


void update_tactics(Position *b)
{
	etype v, v2, bestv;

	if (!(b->flags & FLAG_DONE_TACTICS)) {
		b->tactics = eval_tactics(b);
		b->eval_result += b->tactics;
		return;
	}

	if (b->tactics == 0)
		return;

	if (whites_move(b)) {
		if (b->tactics == THREAT) {
			b->eval_result += b->tactics;
			b->tactics *= 2;
			return;
		}

		if (b->tactics == 2*THREAT) {
			lprintf(0,"tactics bug 1!\n");
			return;
		}

		if (b->tactics == -THREAT) {
			lprintf(0,"tactics bug 2!\n");
			return;
		}

		if (b->tactics == -2*THREAT) {
			bestv = v = (b->w_material - b->b_material);
			v2 = mini_quiesce_w(b, b->material_mask, 0, 
					    bestv + OFFSET, 0);
			if (v2 >= v + OFFSET) {
				b->tactics = 2*THREAT;
				b->eval_result += 4*THREAT;
				return;
			}
			b->tactics = -THREAT;
			b->eval_result += THREAT;
			return;
		}
	}

	if (blacks_move(b)) {
		if (b->tactics == -THREAT) {
			b->eval_result += b->tactics;
			b->tactics *= 2;
			return;
		}
		
		if (b->tactics == -2*THREAT) {
			lprintf(0,"tactics bug 3!\n");
			return;
		}
		
		if (b->tactics == THREAT) {
			lprintf(0,"tactics bug 4!\n");
			return;
		}
		
		if (b->tactics == 2*THREAT) {
			bestv = v = (b->b_material - b->w_material);
			v2 = mini_quiesce_b(b, b->material_mask, 0, 
					    bestv + OFFSET, 0);
			if (v2 >= v + OFFSET) {
				b->tactics = -2*THREAT;
				b->eval_result -= 4*THREAT;
				return;
			}
			b->tactics = THREAT;
			b->eval_result -= THREAT;
			return;
		}
	}

	lprintf(0,"tactics bug 5!\n");
}

etype eval_move_tactics(Position *b, Move *move)
{
	etype v, v2, v1, bestv;
	Position b1;
	etype alpha, beta;

	if (!do_move(&b1, b, move))
		return -INFINITY;

	alpha = -INFINITY;
	beta = INFINITY;

	if (whites_move(&b1)) {
		bestv = v = (b->w_material - b->b_material);
		v1 = bestv + 1;
		while (alpha < beta - OFFSET) {
			v2 = mini_quiesce_w(&b1, b1.material_mask, 0, 
					    v1, 0);
			if (v2 >= v1) {
				alpha = v2;
				v1 = alpha+1;
			} else {
				beta = v2;
				v1 = beta-1;
			}
		}
		if (alpha > v) {
			return -(alpha - v);
		} else if (beta < v) {
			return -(beta - v);
		}
	} else {
		bestv = v = (b->b_material - b->w_material);
		v1 = bestv + 1;
		while (alpha < beta - OFFSET) {
			v2 = mini_quiesce_b(&b1, b1.material_mask, 0, 
					    v1, 0);
			if (v2 >= v1) {
				alpha = v2;
				v1 = alpha+1;
			} else {
				beta = v2;
				v1 = beta-1;
			}
		}
		if (alpha > v) {
			return -(alpha - v);
		} else if (beta < v) {
			return -(beta - v);
		}
	}

	return 0;
}
