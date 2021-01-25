#include "includes.h"
#include "knightcap.h"


#define GAME_MOVES 60
#define MORE_MOVES 30
#define SAFETY_TIME 5
#define MULT_HIGH 6
#define MULT_LOW 3
#define MULT_MAX 10
#define MIN_MOVE_TIME 0.1
#define LAG_TIME 0.25

extern struct state *state;

static float move_start_time, wallstart, cpustart;
static float timer_limit;
static time_t move_start_time_t;

static enum {IDLE, STARTED, EXPIRED} timer_state;

float timer_elapsed(void)
{
	if (timer_state == IDLE)
		return 0;

	return gettime() - move_start_time;
}

float cpu_percent(void)
{
	float cpu = cputime() - cpustart;
	float wall = walltime() - wallstart;
	return 100.0*(cpu/wall);
}

time_t timer_start_time(void)
{
	return move_start_time_t;
}

void timer_start(int player)
{
	timer_state = STARTED;
	wallstart = walltime();
	cpustart = cputime();
	move_start_time = gettime();
	move_start_time_t = time(NULL);
	timer_limit = state->move_time;
}


int timer_extend(void)
{
	float extension;
	timer_state = STARTED;

	extension = state->move_time/3;

	if (state->time_limit - (timer_elapsed()+SAFETY_TIME) < extension)
		extension = state->time_limit - (timer_elapsed()+SAFETY_TIME);

	if (extension <= 0)
		return 0;
	
	timer_limit += extension;

	return 1;
}

void timer_estimate(int secs, int increment)
{
	int n;
	float safety = SAFETY_TIME+increment;

	state->time_limit = secs;

	if (secs <= 0) {
		state->move_time = 0;
		return;
	}

	/* guess how long the game will be */
	n = GAME_MOVES - state->position.move_num;
	if (n < MORE_MOVES) n = MORE_MOVES;

	state->move_time = increment + (secs-safety)/n - LAG_TIME;

	if (state->move_time < MIN_MOVE_TIME)
		state->move_time = MIN_MOVE_TIME;

	if (state->position.move_num < 4) {
		state->move_time = fmin(15, state->move_time);
	}

	if (state->move_time > secs/2)
		state->move_time = secs/2;

	lprintf(0,"ics_time=%d ics_otim=%d\n",
		state->ics_time,  state->ics_otim);

	if (state->ics_time > state->ics_otim) {
	  state->move_time *= 1.5;
	  lprintf(0,"extending time to %2.1f\n", state->move_time);
	}

	if (state->max_move_time != 0 && state->move_time > state->max_move_time)
		state->move_time = state->max_move_time;

	if (!state->ics_robot)
		lprintf(0,"move_time=%2.1f\n", state->move_time);
}


int timer_expired(void)
{
	if (timer_state == IDLE)
		return 0;
	
	if (timer_state == STARTED) {
		if (timer_elapsed() > timer_limit)
			timer_state = EXPIRED;
	}

	return (timer_state == EXPIRED);
}

void timer_reset(void)
{
}

void timer_off(void)
{
	timer_state = IDLE;
}

int timer_terminate(int depth, int player, int force)
{
	float elapsed = timer_elapsed();
	float available = state->move_time;

	if (state->max_search_depth && 
	    depth > state->max_search_depth) {
		return 1;
	}

	if (force)
		return 1;

	if (timer_state == IDLE)
		return 0;

	if (elapsed < available * 0.7)
		return 0;

	return 1;
}


float cputime(void)
{
	struct rusage ru;

	getrusage(RUSAGE_SELF, &ru);
	return ru.ru_utime.tv_sec + 1.0e-6*ru.ru_utime.tv_usec;
}

float walltime(void)
{
	static struct timeval tv0;
	struct timeval tv;
	static int initialised;
	if (!initialised) {
		initialised = 1;
		gettimeofday(&tv0, NULL);
	}
	gettimeofday(&tv, NULL);
	return (tv.tv_sec - tv0.tv_sec) + 1.0e-6*(tv.tv_usec - tv0.tv_usec);
}

float gettime(void)
{
#if WALL_CLOCK
	return walltime();
#else
	return cputime();
#endif
}
