#include "includes.h"
#include "knightcap.h"

extern struct state *state;

void autotune(int minnps)
{
	state->move_time = 3;
  
	test_fin("tune.fin");
	
	lprintf(0, "TUNE: %d\n", state->nps);
	
	if (state->nps > minnps) {
		lprintf(0,"TUNED\n");
		state->tuned = 1;
	} else {
		exit(0);
	}
}
