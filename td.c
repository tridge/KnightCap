#include "includes.h"
#include "knightcap.h"

extern struct state *state;
extern int demo_mode;
extern int player;
extern int dont_change[];


/* routines for updating the evaluation function according to the
   method of temporal differences */

#if LEARN_EVAL

/* return how many evaluation coefficients there are */
static int td_num_coefficients(void)
{
	return __TOTAL_COEFFS__;
}

/* flag an eval if it is from the brain or a forced mate or a forced draw*/
void td_unusable_eval(Eval *v)
{
	int m = state->stored_move_num;
	int i,n;

	return;

	n = td_num_coefficients();

	state->leaf_eval[m].v = v->v;

	state->leaf_eval[m].flags |= FLAG_BAD_EVAL;

	for (i=0; i<n; i++) {
		state->grad[m*n+i] = 0;
	}

	++state->stored_move_num;
}

/* return in state the partial derivative of the eval function with
   respect to each of the evaluation coefficients. Computed
   numerically*/
int td_gradient(Position *b)
{
	etype v, v2, v3, v4;
	int i, j, n, m;
	etype delta = 1;
	
	n = td_num_coefficients();

	m = state->stored_move_num;

	b->flags &= ~FLAG_EVAL_DONE;
	b->flags &= ~FLAG_DONE_TACTICS;
	
	v = eval_etype(b, INFINITY, MAX_DEPTH);

	state->leaf_eval[m].v = next_to_play(b)*v;
	if (!demo_mode) {
		state->leaf_eval[m].v *= state->computer;
	}

	j = 0;
	for (i=0;i<n;i++) {
		if (i == dont_change[j]) {
			++j;
			continue;
		}

		coefficients[i] += delta;

		/* material only affects the eval indirectly 
		   via the board, so update the board */
		if (i > IPIECE_VALUES && i < IPIECE_VALUES+KING)
			create_pboard(b);

		b->flags &= ~FLAG_EVAL_DONE;
		b->flags &= ~FLAG_DONE_TACTICS;
		v2 = eval_etype(b, INFINITY, MAX_DEPTH);

		state->grad[m*n+i] = 
			next_to_play(b)*(v2 - v) / delta;
		if (!demo_mode) {
			state->grad[m*n+i] *= state->computer;
		} 
		
#if 0
		coefficients[i] += delta;

		if (i > IPIECE_VALUES && i < IPIECE_VALUES+KING)
			create_pboard(b);

		b->flags &= ~FLAG_EVAL_DONE;
		b->flags &= ~FLAG_DONE_TACTICS;

		v3 = eval_etype(b, INFINITY, MAX_DEPTH);

		coefficients[i] -= delta;

		if (i > IPIECE_VALUES && i < IPIECE_VALUES+KING)
			create_pboard(b);

		if (i==574) {
			coefficients[i] -=delta;
			b->flags &= ~FLAG_EVAL_DONE;
			b->flags &= ~FLAG_DONE_TACTICS;
			v4 = eval_etype(b, INFINITY, MAX_DEPTH);
			coefficients[i] += delta;
			lprintf(0,"********coeff: %d grad: %e error: %e %e %e %e %e %e\n", i, 
				state->grad[m*n+i], 
				next_to_play(b)*(v3 - v) - 2*delta*state->grad[m*n+i], 
				v, v2, v3, v4);
		}
#endif
		coefficients[i] -=delta;
		if (i > IPIECE_VALUES && i < IPIECE_VALUES+KING)
			create_pboard(b);
	}

	++state->stored_move_num;
	return n;
}

void td_save_bad(int fd, Position *b1)
{
	int x;

	lseek(fd, 0, SEEK_END);

	if ((x = Write(fd, b1, sizeof(Position))) != sizeof(Position)) {
		lprintf(0,"***Error saving bad eval position %d %d\n",
			sizeof(Position), x);
	} 
} 

/* Updates the 	coefficients according to the TD(lambda) algorithm. */
int td_update()
{
        int i,j,n,t; 
	int argmax;
	int num_moves;
	float c, max;
	float dw[__TOTAL_COEFFS__];
	float tanhv[MAX_GAME_MOVES];
	float d[MAX_GAME_MOVES];

	if (state->ics_robot && result() == TIME_FORFEIT)
		num_moves = state->stored_move_num-1;
	else 
		num_moves = state->stored_move_num;
		
	lprintf(0,"***moves: %d\n", num_moves);
	n = td_num_coefficients();

		
	/* Squash the evals and compute the temporal differences */
	tanhv[0] =  tanh(EVAL_SCALE*state->leaf_eval[0].v);
	for (t=0; t<num_moves-1; t++) {
		tanhv[t+1] = tanh(EVAL_SCALE*state->leaf_eval[t+1].v);
		d[t] = tanhv[t+1] - tanhv[t];
		if (!state->predicted_move[t+1])
			d[t] = RAMP(d[t]);
	}

	if (!state->ics_robot) {
		switch (state->won) {
		case STALEMATE: {
			d[num_moves-1] = tanh(EVAL_SCALE*DRAW_VALUE)
				- tanhv[num_moves-1];
			break;
		}
		case 1: {
			d[num_moves-1] = 1.0 - tanhv[num_moves-1];
			break;
		}
		case 0: {
			d[num_moves-1] = -1.0 - tanhv[num_moves-1];
			break;
		}
		} 
	} else {
		switch (result()) {
		case STALEMATE: {
			d[num_moves-1] = tanh(EVAL_SCALE*DRAW_VALUE)
				- tanhv[num_moves-1];
			break;
		}
		case 1: {
			d[num_moves-1] = 1.0 - tanhv[num_moves-1];
			break;
		}
		case 0: {
			d[num_moves-1] = -1.0 - tanhv[num_moves-1];
			break;
		}
		/* for time forfeited or resigned games we just assume the 
		   final eval was correct */
		case TIME_FORFEIT: {
			d[num_moves-1] = 0.0;
			break;
		}
		}
	}
	
	d[num_moves-1] = RAMP(d[num_moves-1]);

#if 1
	for (i=0; i<num_moves; i++) {
		lprintf(0,"%d %f %d\n", state->leaf_eval[i].v, d[i],
			(int)state->predicted_move[i<num_moves-1?i+1:i]);
	}
#endif


	/* calculate the coefficient updates */
	max = 0.0;
	j=0;
	for (i=0; i<n; i++) {
		/* "FACTORS" are multiplicative and have disproportionally
		   high derivatives so we don't adjust them */
		if (i==dont_change[j]) {
			++j;
			dw[i] = 0.0;
			continue;
		}	
		dw[i] = 0.0;
		c = (1.0 - tanhv[0]*tanhv[0])*EVAL_SCALE*state->grad[i];

		for (t=0; t<num_moves; t++) {
			dw[i] += d[t]*c;
			if (t<num_moves-1) {
				c = TD_LAMBDA*c + (1-tanhv[t+1]*tanhv[t+1])*
					EVAL_SCALE*state->grad[(t+1)*n+i];
			}
		}
		if (ABS(dw[i]) > max) {
			max = ABS(dw[i]);
			argmax = i;
		}
	}

	lprintf(0,"max: %f %d\n", TD_ALPHA*max, argmax);
			
	j = 0;
	for (i=0; i<n; i++) {
		if (i==dont_change[j]) {
			++j;
			continue;
		}

		coefficients[i] += TD_ALPHA*dw[i];
#if 1
		if (i%10 == 0 || i==argmax) {
			if (i==argmax) {
				lprintf(0,"***");
			}
			lprintf(0, "%d coeff %d: dw: %f\n",
				i, coefficients[i], TD_ALPHA*dw[i]);
		}
#endif
	}

	lprintf(0,"updated coefficients\n");

	return 1;
}

#else
void td_dummy(void) 
{}
#endif
